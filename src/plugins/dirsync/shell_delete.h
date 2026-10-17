// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "engine.h"
#include "copy.h"
#include <shobjidl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>
#include <set>

namespace dirsync {
struct ImmediateItem {
    std::wstring path;
    Entry original;
    int side=0;
};
struct ImmediateOutcome {
    std::wstring path,recycledPath;
    HRESULT error=S_OK;
    bool deleted=false;
};
struct ImmediateReport {
    std::vector<ImmediateOutcome> items;
    HRESULT error=S_OK;
    bool cancelled=false,stale=false;
};
inline bool ImmediateEligible(const Row& row,int side)
{
    const auto& e=side?row.right:row.left;
    return (side?row.hasRight:row.hasLeft) && e.identified && !e.error &&
        row.state!=State::Blocked && row.reason!=Reason::Aliased && row.reason!=Reason::Ambiguous &&
        !(e.attributes&(FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_OFFLINE|FILE_ATTRIBUTE_ENCRYPTED|0x00040000|0x00400000));
}
// side: 0 left, 1 right, 2 both. Only explicit selected model rows are included.
inline std::vector<ImmediateItem> ImmediateSelection(const Result& result,const std::vector<size_t>& selected,int side)
{
    std::wstring left,right;std::vector<ImmediateItem> items;
    if(side<0 || side>2 || result.stale || result.cancelled || !SnapshotRoots(result,left,right)) return items;
    auto less=[](const std::wstring& a,const std::wstring& b) {return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_LESS_THAN;};
    std::set<std::wstring,decltype(less)> paths(less);
    for(size_t index:selected) if(index<result.rows.size()) {
        const auto& row=result.rows[index];
        // Rows originate in the snapshot, but never accept an escaping relative name.
        if(row.path.empty() || row.path.front()==L'\\' || row.path.find(L':')!=std::wstring::npos) continue;
        bool safe=true;
        for(size_t start=0;start<row.path.size();) {
            auto end=row.path.find(L'\\',start);if(end==std::wstring::npos) end=row.path.size();
            const auto part=row.path.substr(start,end-start);
            if(part.empty() || part==L"." || part==L".." || part.find(L'/')!=std::wstring::npos) safe=false;
            start=end+1;
        }
        if(!safe) continue;
        for(int s=0;s<2;++s) if((side==2 || side==s) && ImmediateEligible(row,s)) {
            auto path=(s?right:left)+L"\\"+row.path;
            if(paths.insert(path).second)
                items.push_back({std::move(path),s?row.right:row.left,s});
        }
    }
    std::stable_sort(items.begin(),items.end(),[](const ImmediateItem& a,const ImmediateItem& b) {
        if(a.original.directory!=b.original.directory) return !a.original.directory;
        return a.original.directory && a.path.size()>b.path.size();
    });
    return items;
}
template<class T> struct ShellObject {
    T* value=nullptr;
    ~ShellObject() {if(value) value->Release();}
    ShellObject()=default;
    ShellObject(const ShellObject&)=delete;
    ShellObject& operator=(const ShellObject&)=delete;
};
inline HRESULT ImmediateCheckpoint(const ImmediateItem& item)
{
    auto path=ApiPath(item.path);
    HANDLE file=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(file==INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
    BY_HANDLE_FILE_INFORMATION info{};
    bool ok=GetFileInformationByHandle(file,&info)!=FALSE;DWORD error=ok?ERROR_SUCCESS:GetLastError();CloseHandle(file);
    if(!ok) return HRESULT_FROM_WIN32(error);
    const auto& original=item.original;
    if(info.dwFileAttributes&FILE_ATTRIBUTE_READONLY) return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
    if((info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) ||
        info.dwVolumeSerialNumber!=original.volume || ((uint64_t(info.nFileIndexHigh)<<32)|info.nFileIndexLow)!=original.id ||
        bool(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=original.directory ||
        (!original.directory && (((uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow)!=original.size ||
        ((uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime)!=original.time)))
        return HRESULT_FROM_WIN32(ERROR_FILE_INVALID);
    if(original.directory) {
        WIN32_FIND_STREAM_DATA stream{};HANDLE streams=FindFirstStreamW(path.c_str(),FindStreamInfoStandard,&stream,0);
        if(streams!=INVALID_HANDLE_VALUE) {
            bool named=false;
            do {if(wcscmp(stream.cStreamName,L"::$DATA")) {named=true;break;}} while(FindNextStreamW(streams,&stream));
            error=GetLastError();FindClose(streams);
            if(named) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            if(error!=ERROR_HANDLE_EOF) return HRESULT_FROM_WIN32(error);
        } else if(GetLastError()!=ERROR_HANDLE_EOF) return HRESULT_FROM_WIN32(GetLastError());
        WIN32_FIND_DATAW data{};HANDLE find=FindFirstFileW((path+L"\\*").c_str(),&data);
        if(find==INVALID_HANDLE_VALUE) {error=GetLastError();return error==ERROR_FILE_NOT_FOUND?S_OK:HRESULT_FROM_WIN32(error);}
        bool empty=true;
        do {if(wcscmp(data.cFileName,L".") && wcscmp(data.cFileName,L"..")) {empty=false;break;}} while(FindNextFileW(find,&data));
        error=GetLastError();FindClose(find);
        if(!empty) return HRESULT_FROM_WIN32(ERROR_DIR_NOT_EMPTY);
        if(error!=ERROR_NO_MORE_FILES) return HRESULT_FROM_WIN32(error);
    }
    return S_OK;
}
// A scoped COM sink: all Shell references are released before this stack object.
class ImmediateSink final:public IFileOperationProgressSink {
    std::atomic<ULONG> references{1};
public:
    const ImmediateItem& item;ImmediateOutcome& outcome;std::atomic<bool>& cancel;bool recycle;
    ImmediateSink(const ImmediateItem& i,ImmediateOutcome& o,std::atomic<bool>& c,bool r):item(i),outcome(o),cancel(c),recycle(r) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return E_POINTER;*out=nullptr;
        if(id==IID_IUnknown || id==__uuidof(IFileOperationProgressSink)) {*out=static_cast<IFileOperationProgressSink*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++references;}
    ULONG STDMETHODCALLTYPE Release() override {return --references;}
    HRESULT Check() {return cancel?HRESULT_FROM_WIN32(ERROR_CANCELLED):S_OK;}
    HRESULT STDMETHODCALLTYPE StartOperations() override {return Check();}
    HRESULT STDMETHODCALLTYPE FinishOperations(HRESULT result) override {if(FAILED(result)) outcome.error=result;return S_OK;}
    HRESULT STDMETHODCALLTYPE PreDeleteItem(DWORD flags,IShellItem*) override {
        HRESULT hr=Check();
        // Refuse an operation that Shell intends to execute without recycling.
        if(SUCCEEDED(hr) && recycle && !(flags&TSF_DELETE_RECYCLE_IF_POSSIBLE)) hr=HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        if(SUCCEEDED(hr)) hr=ImmediateCheckpoint(item);
        if(FAILED(hr)) outcome.error=hr;return hr;
    }
    HRESULT STDMETHODCALLTYPE PostDeleteItem(DWORD,IShellItem*,HRESULT result,IShellItem* created) override {
        outcome.error=result;
        if(SUCCEEDED(result)) {
            DWORD attrs=GetFileAttributesW(ApiPath(item.path).c_str());DWORD error=GetLastError();
            outcome.deleted=attrs==INVALID_FILE_ATTRIBUTES && (error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND);
            if(!outcome.deleted) outcome.error=E_FAIL;
            if(created) {PWSTR path=nullptr;if(SUCCEEDED(created->GetDisplayName(SIGDN_FILESYSPATH,&path))) {outcome.recycledPath=path;CoTaskMemFree(path);}}
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UpdateProgress(UINT,UINT) override {return Check();}
    HRESULT STDMETHODCALLTYPE ResetTimer() override {return S_OK;}
    HRESULT STDMETHODCALLTYPE PauseTimer() override {return S_OK;}
    HRESULT STDMETHODCALLTYPE ResumeTimer() override {return S_OK;}
    HRESULT STDMETHODCALLTYPE PreRenameItem(DWORD,IShellItem*,LPCWSTR) override {return E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE PostRenameItem(DWORD,IShellItem*,LPCWSTR,HRESULT,IShellItem*) override {return E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE PreMoveItem(DWORD,IShellItem*,IShellItem*,LPCWSTR) override {return E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE PostMoveItem(DWORD,IShellItem*,IShellItem*,LPCWSTR,HRESULT,IShellItem*) override {return E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE PreCopyItem(DWORD,IShellItem*,IShellItem*,LPCWSTR) override {return E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE PostCopyItem(DWORD,IShellItem*,IShellItem*,LPCWSTR,HRESULT,IShellItem*) override {return E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE PreNewItem(DWORD,IShellItem*,LPCWSTR) override {return E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE PostNewItem(DWORD,IShellItem*,LPCWSTR,LPCWSTR,DWORD,HRESULT,IShellItem*) override {return E_UNEXPECTED;}
};
#ifdef DIRSYNC_COPY_TEST_HOOKS
inline std::function<void(size_t)>& ImmediateTestHook() {static thread_local std::function<void(size_t)> hook;return hook;}
#endif
inline ImmediateReport ImmediateDelete(Result snapshot,const std::vector<size_t>& selected,int side,bool recycle,
    HWND owner,std::atomic<bool>& cancel,Progress& checking,CopyProgress& progress)
{
    ImmediateReport report;const auto items=ImmediateSelection(snapshot,selected,side);
    if(items.empty()) {report.error=E_INVALIDARG;return report;}
    if(!Revalidate(snapshot,cancel,checking)) {report.stale=true;report.cancelled=cancel;report.error=HRESULT_FROM_WIN32(cancel?ERROR_CANCELLED:ERROR_FILE_INVALID);return report;}
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)) {report.error=initialized;return report;}
    struct Apartment {~Apartment(){CoUninitialize();}} apartment;
    progress.phase=CopyPhase::Deleting;
    for(const auto& item:items) {
#ifdef DIRSYNC_COPY_TEST_HOOKS
        if(ImmediateTestHook()) ImmediateTestHook()(report.items.size());
#endif
        if(cancel) {report.cancelled=true;report.error=HRESULT_FROM_WIN32(ERROR_CANCELLED);break;}
        report.items.push_back({item.path,L"",S_OK,false});auto& outcome=report.items.back();
        progress.item=report.items.size()-1;
        HRESULT hr=ImmediateCheckpoint(item);
        if(SUCCEEDED(hr)) {
            ImmediateSink sink(item,outcome,cancel,recycle);
            ShellObject<IFileOperation> operation;ShellObject<IShellItem> source;
            hr=CoCreateInstance(CLSID_FileOperation,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&operation.value));
            DWORD flags=FOF_NOCONFIRMATION|FOF_NOERRORUI|FOF_SILENT|FOF_NO_CONNECTED_ELEMENTS|FOFX_EARLYFAILURE;
            if(recycle) flags|=FOFX_RECYCLEONDELETE|FOFX_ADDUNDORECORD;
            if(SUCCEEDED(hr)) hr=operation.value->SetOperationFlags(flags);
            if(SUCCEEDED(hr)) hr=operation.value->SetOwnerWindow(owner);
            if(SUCCEEDED(hr)) hr=SHCreateItemFromParsingName(item.path.c_str(),nullptr,IID_PPV_ARGS(&source.value));
            if(SUCCEEDED(hr)) hr=operation.value->DeleteItem(source.value,&sink);
            if(SUCCEEDED(hr)) hr=operation.value->PerformOperations();
            BOOL aborted=FALSE;
            if(SUCCEEDED(hr)) hr=operation.value->GetAnyOperationsAborted(&aborted);
            if(SUCCEEDED(hr) && aborted) hr=HRESULT_FROM_WIN32(ERROR_CANCELLED);
        }
        if(FAILED(hr) && SUCCEEDED(outcome.error)) outcome.error=hr;
        if(outcome.deleted) ++progress.completed;
        if(FAILED(outcome.error) || !outcome.deleted) {
            report.error=FAILED(outcome.error)?outcome.error:E_FAIL;
            report.cancelled=cancel || report.error==HRESULT_FROM_WIN32(ERROR_CANCELLED);break;
        }
    }
    progress.phase=CopyPhase::Finished;return report;
}
}
