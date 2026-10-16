// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <algorithm>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace
{
int checks=0;
void Check(bool ok,const char* what) { ++checks; if(!ok) throw std::runtime_error(what); }
struct Fixture
{
    std::wstring root,left,right;
    std::vector<std::wstring> directories,files;
    Fixture()
    {
        std::vector<wchar_t> temp(32768); Check(GetTempPathW(DWORD(temp.size()),temp.data())!=0,"temp path");
        root=std::wstring(temp.data())+L"dirsync-lifecycle-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
        Dir(root); left=root+L"\\left"; right=root+L"\\right"; Dir(left); Dir(right);
    }
    void Dir(const std::wstring& path) { Check(CreateDirectoryW(path.c_str(),nullptr)!=FALSE,"fixture directory"); directories.push_back(path); }
    void Data(const std::wstring& path,BYTE value=42)
    {
        HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
        Check(f!=INVALID_HANDLE_VALUE,"copy fixture handle"); files.push_back(path);
        std::vector<BYTE> data(1024*1024,value); DWORD n=0;
        bool ok=WriteFile(f,data.data(),DWORD(data.size()),&n,nullptr)!=FALSE && n==data.size();
        CloseHandle(f); Check(ok,"copy fixture data");
    }
    void Sparse(const std::wstring& path)
    {
        HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
        Check(f!=INVALID_HANDLE_VALUE,"fixture handle"); files.push_back(path);
        DWORD returned=0; LARGE_INTEGER end{}; end.QuadPart=64LL*1024*1024*1024; FILETIME ft{0,0};
        bool ok=DeviceIoControl(f,FSCTL_SET_SPARSE,nullptr,0,nullptr,0,&returned,nullptr)!=FALSE;
        if(ok) ok=SetFilePointerEx(f,end,nullptr,FILE_BEGIN) && SetEndOfFile(f) && SetFileTime(f,nullptr,nullptr,&ft);
        CloseHandle(f); Check(ok,"sparse data and time");
    }
    // Register only descendants of this fixture's private recovery folder for cleanup.
    void TrackRecovery(const std::wstring& path) {
        Check(path.rfind(root+L"\\recovery",0)==0,"owned recovery cleanup boundary");
        WIN32_FIND_DATAW d{}; HANDLE h=FindFirstFileW((L"\\\\?\\"+path+L"\\*").c_str(),&d);
        if(h==INVALID_HANDLE_VALUE) return;
        do {
            if(!wcscmp(d.cFileName,L".") || !wcscmp(d.cFileName,L"..")) continue;
            auto child=path+L"\\"+d.cFileName;
            Check(!(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT),"no reparse in owned recovery cleanup");
            if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) { directories.push_back(child); TrackRecovery(child); }
            else files.push_back(child);
        } while(FindNextFileW(h,&d)); FindClose(h);
    }
    ~Fixture() { for(const auto& p:files) DeleteFileW(p.c_str()); for(auto i=directories.rbegin();i!=directories.rend();++i) RemoveDirectoryW(i->c_str()); }
};
struct Module
{
    using Start=BOOL (WINAPI*)(const wchar_t*,const wchar_t*,BOOL);
    using Status=unsigned (WINAPI*)();
    using Release=unsigned (WINAPI*)(BOOL);
    using CopyCancel=unsigned (WINAPI*)(BOOL);
    HMODULE handle=nullptr; Start start=nullptr; Status status=nullptr; Release release=nullptr;
    Start copyStart=nullptr; Status copyStatus=nullptr; CopyCancel copyCancel=nullptr; Release copyRelease=nullptr;
    using DeleteFinish=unsigned (WINAPI*)(unsigned);
    DeleteFinish deleteFinish=nullptr;
    explicit Module(const std::wstring& path)
    {
        handle=LoadLibraryW(path.c_str());
        if(!handle) std::cerr<<"LoadLibrary error "<<GetLastError()<<"\n";
        Check(handle!=nullptr,"load probe DLL");
        start=reinterpret_cast<Start>(GetProcAddress(handle,"ProbeStart"));
        status=reinterpret_cast<Status>(GetProcAddress(handle,"ProbeStatus"));
        release=reinterpret_cast<Release>(GetProcAddress(handle,"ProbeRelease"));
        Check(start && status && release,"probe exports");
        copyStart=reinterpret_cast<Start>(GetProcAddress(handle,"ProbeCopyStart"));
        copyStatus=reinterpret_cast<Status>(GetProcAddress(handle,"ProbeCopyStatus"));
        copyCancel=reinterpret_cast<CopyCancel>(GetProcAddress(handle,"ProbeCopyCancel"));
        copyRelease=reinterpret_cast<Release>(GetProcAddress(handle,"ProbeCopyRelease"));
        Check(copyStart && copyStatus && copyCancel && copyRelease,"copy lifecycle exports");
        deleteFinish=reinterpret_cast<DeleteFinish>(GetProcAddress(handle,"ProbeDeleteFinish"));
        Check(deleteFinish!=nullptr,"deletion lifecycle export");
    }
    ~Module() { if(handle) { if(release) release(TRUE); FreeLibrary(handle); } }
};
void Run(const std::wstring& dll,Fixture& f,bool content)
{
    Module module(dll);
    auto settingsTest=reinterpret_cast<Module::Status>(GetProcAddress(module.handle,"ProbeSettingsTest"));
    Check(settingsTest && settingsTest()==15,"persistent recovery and appearance settings: round trips, long Unicode paths and malformed values");
    Check(module.release(FALSE)==1,"idle normal release succeeds");
    Check(module.start(f.left.c_str(),f.right.c_str(),content)!=FALSE,"start module worker");
    ULONGLONG deadline=GetTickCount64()+5000; unsigned status=0;
    do { status=module.status(); if((status&1) && (content ? (status&4) : (status&2))) break; Sleep(1); } while(GetTickCount64()<deadline);
    Check((status&1) && (content ? (status&4) : (status&2)),"observe worker in requested phase");
    Check(module.release(FALSE)==0,"active normal release is refused");
    Check((module.status()&8)==0,"refused release does not cancel");
    ULONGLONG start=GetTickCount64(); unsigned result=module.release(TRUE);
    Check(result==225,"force release joins worker, marks cancellation and blocks proposals");
    Check(GetTickCount64()-start<3000,"local force release completes within three seconds");
    Check(module.release(TRUE)==1,"repeated force release is safe");
    Check(FreeLibrary(module.handle)!=FALSE,"immediate FreeLibrary"); module.handle=nullptr;
    Check(GetModuleHandleW(dll.c_str())==nullptr,"DLL unmapped after force release");
    if(content)
    {
        for(const auto& p:f.files)
        {
            HANDLE file=CreateFileW(p.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
            bool ok=file!=INVALID_HANDLE_VALUE; if(ok) CloseHandle(file); Check(ok,"no content handles survive unload");
        }
    }
    std::cout<<(content?"content":"scan")<<": force Release + immediate FreeLibrary passed\n";
}
void CopyControls(const std::wstring& dll,bool verify,bool close,bool recovery=false,bool overwrite=false,bool keep=true)
{
    Fixture f; f.Data(f.left+L"\\a"); f.Data(f.left+L"\\b"); Module module(dll);
    if(recovery || overwrite) { f.Data(f.right+L"\\a"); f.Data(f.right+L"\\b"); f.Dir(f.root+L"\\recovery"); }
    Check(module.copyStart(f.left.c_str(),f.right.c_str(),(verify?1:0)|(recovery?4:0)|(overwrite?8:0)|(keep?0:16))!=FALSE,"start copy lifecycle worker");
    ULONGLONG deadline=GetTickCount64()+3000;
    while(!(module.copyStatus()&1) && GetTickCount64()<deadline) Sleep(1);
    Check(module.copyStatus()==1,"observe active actual copy/Verify callback");
    ULONGLONG start=GetTickCount64();
    Check(module.copyCancel(close)==31,"real Stop/Close handler cancels and joins copy with correct report");
    Check(GetTickCount64()-start<3000,"Stop/Close local join within three seconds");
    WIN32_FIND_DATAW entry{}; HANDLE find=FindFirstFileW((f.right+L"\\*").c_str(),&entry); bool empty=true;
    if(find!=INVALID_HANDLE_VALUE) {
        do { if(wcscmp(entry.cFileName,L".") && wcscmp(entry.cFileName,L"..")) empty=false; } while(FindNextFileW(find,&entry));
        FindClose(find);
    }
    Check(empty!=(recovery || overwrite),"Stop/Close leaves originals or empty new-file destination");
    if(recovery || overwrite) f.TrackRecovery(f.root+L"\\recovery");
    for(const auto& path:f.files) {
        HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
        Check(h!=INVALID_HANDLE_VALUE,"source handles released after Stop/Close join"); if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    Check(FreeLibrary(module.handle)!=FALSE,"unload after copy Stop/Close"); module.handle=nullptr;
    std::cout<<(overwrite?(keep?"overwrite with recovery ":"overwrite without recovery "):recovery?"recovery ":"")
        <<(close?"Close":"Stop")<<" during "<<(verify?"Verify":"copy")<<": handlers and join passed\n";
}
void DeleteControls(const std::wstring& dll,unsigned mode)
{
    Fixture f; if(mode&2) f.Data(f.right+L"\\0-done",17);
    f.Data(f.right+L"\\a",17); f.Data(f.right+L"\\a:note",17); f.Data(f.right+L"\\b",17); f.Dir(f.root+L"\\recovery");
    Module module(dll);
    Check(module.copyStart(f.left.c_str(),f.right.c_str(),mode|32)!=FALSE,"start actual deletion worker");
    ULONGLONG deadline=GetTickCount64()+3000;
    while(!(module.copyStatus()&1) && GetTickCount64()<deadline) Sleep(1);
    Check(module.copyStatus()==1,"observe deletion or backup callback before cancellation");
    ULONGLONG start=GetTickCount64();
    Check(module.deleteFinish(mode)==31,"real deletion Stop/Close/force Release joins and preserves actual report");
    Check(GetTickCount64()-start<3000,"local deletion cancellation/join within three seconds");
    if(mode&2) {
        const auto done=f.right+L"\\0-done";
        Check(GetFileAttributesW(done.c_str())==INVALID_FILE_ATTRIBUTES,"previous accepted deletion remains deleted");
        f.files.erase(std::remove(f.files.begin(),f.files.end(),done),f.files.end());
    }
    f.TrackRecovery(f.root+L"\\recovery");
    for(const auto& path:f.files) {
        HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
        Check(h!=INVALID_HANDLE_VALUE,"no deletion handles survive join"); if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    WIN32_FIND_DATAW data{}; HANDLE find=FindFirstFileW((f.right+L"\\.dirsync-*").c_str(),&data);
    Check(find==INVALID_HANDLE_VALUE,"no deletion staging survives cancellation"); if(find!=INVALID_HANDLE_VALUE) FindClose(find);
    Check(FreeLibrary(module.handle)!=FALSE,"immediate unload after deletion cancel"); module.handle=nullptr;
    Check(GetModuleHandleW(dll.c_str())==nullptr,"deletion DLL unmapped");
    std::cout<<"deletion lifecycle mode "<<mode<<": passed\n";
}
void CopyUnload(const std::wstring& dll,int mode)
{
    Fixture f;
    if(mode&2) { f.Data(f.left+L"\\0-done"); f.Data(f.left+L"\\0-done:note"); }
    f.Data(f.left+L"\\a"); f.Data(f.left+L"\\b"); Module module(dll);
    if(mode&8) {
        if(mode&2) { f.Data(f.right+L"\\0-done",17); f.Data(f.right+L"\\0-done:old",17); }
        f.Data(f.right+L"\\a",17); f.Data(f.right+L"\\a:old",17);
        f.Data(f.right+L"\\b",17); f.Data(f.right+L"\\b:old",17); f.Dir(f.root+L"\\recovery");
    }
    Check(module.copyStart(f.left.c_str(),f.right.c_str(),mode)!=FALSE,"start unload copy worker");
    ULONGLONG deadline=GetTickCount64()+3000;
    while(!(module.copyStatus()&1) && GetTickCount64()<deadline) Sleep(1);
    Check(module.copyStatus()==1,"unload interrupts actual copy/Verify callback");
    Check(module.copyRelease(FALSE)==0,"normal release refuses active copying");
    Check(module.copyStatus()==1,"refused copy release leaves worker active");
    ULONGLONG start=GetTickCount64();
    Check(module.copyRelease(TRUE)==31,"force release joins copy, preserves completed items and cancels rest");
    Check(GetTickCount64()-start<3000,"copy force release within three seconds");
    Check(module.copyRelease(TRUE)==1,"repeated force release after copy is safe");
    Check(FreeLibrary(module.handle)!=FALSE,"immediate unload after copying release"); module.handle=nullptr;
    Check(GetModuleHandleW(dll.c_str())==nullptr,"copy DLL unmapped after release");
    WIN32_FIND_DATAW entry{}; HANDLE find=FindFirstFileW((f.right+L"\\*").c_str(),&entry);
    unsigned count=0;
    if(find!=INVALID_HANDLE_VALUE) {
        do { if(wcscmp(entry.cFileName,L".") && wcscmp(entry.cFileName,L"..")) {
            ++count; Check((mode&8) ? (!wcscmp(entry.cFileName,L"a") || !wcscmp(entry.cFileName,L"b") ||
                ((mode&2) && !wcscmp(entry.cFileName,L"0-done"))) : ((mode&2) && !wcscmp(entry.cFileName,L"0-done")),
                "only originals and completed targets remain after unload");
        } } while(FindNextFileW(find,&entry));
        FindClose(find);
    }
    Check(count==((mode&8)?2u:0u)+((mode&2)?1u:0u),"no unpublished target or stage after unload");
    if(mode&8) {
        for(const auto& name:{L"a",L"b"}) for(const auto& stream:{L"",L":old"}) {
            const auto path=f.right+L"\\"+name+stream;
            HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
            Check(h!=INVALID_HANDLE_VALUE,"unreplaced original main/ADS accessible after unload");
            if(h!=INVALID_HANDLE_VALUE) {
                std::vector<BYTE> bytes(1024*1024); DWORD read=0;
                bool ok=ReadFile(h,bytes.data(),DWORD(bytes.size()),&read,nullptr)!=FALSE && read==bytes.size();
                for(BYTE value:bytes) if(value!=17) ok=false;
                Check(ok && GetFileSize(h,nullptr)==bytes.size(),"unreplaced original main/ADS unchanged"); CloseHandle(h);
            }
        }
        f.TrackRecovery(f.root+L"\\recovery");
    }
    if(mode&2) {
        if(mode&8) {
            const auto oldStream=f.right+L"\\0-done:old";
            HANDLE old=CreateFileW(oldStream.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
            const DWORD error=GetLastError(); if(old!=INVALID_HANDLE_VALUE) CloseHandle(old);
            Check(old==INVALID_HANDLE_VALUE && error==ERROR_FILE_NOT_FOUND,"completed replacement omits old-only ADS");
            f.files.erase(std::remove(f.files.begin(),f.files.end(),oldStream),f.files.end());
        }
        for(const auto& suffix:{L"\\0-done",L"\\0-done:note"}) {
            std::wstring path=f.right+suffix; if(!(mode&8) || !wcscmp(suffix,L"\\0-done:note")) f.files.push_back(path);
            HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
            Check(h!=INVALID_HANDLE_VALUE,"completed main/ADS accessible after unload");
            if(h!=INVALID_HANDLE_VALUE) {
                std::vector<BYTE> bytes(1024*1024); DWORD read=0;
                bool ok=ReadFile(h,bytes.data(),DWORD(bytes.size()),&read,nullptr)!=FALSE && read==bytes.size();
                for(BYTE value:bytes) if(value!=42) ok=false;
                Check(ok && GetFileSize(h,nullptr)==bytes.size(),"completed main/ADS bytes preserved"); CloseHandle(h);
            }
        }
    }
    for(const auto& path:f.files) {
        HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
        Check(h!=INVALID_HANDLE_VALUE,"no handles survive force copy unload"); if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    std::cout<<((mode&8)?((mode&16)?"overwrite without recovery ":"overwrite with recovery "):"")
        <<"force Release during "<<((mode&1)?"Verify":"copy")<<", prior completed="<<((mode&2)?1:0)<<": passed\n";
}
}
int main()
{
    try
    {
        std::vector<wchar_t> exe(32768); DWORD n=GetModuleFileNameW(nullptr,exe.data(),DWORD(exe.size()));
        Check(n>0 && n<exe.size(),"executable path"); std::wstring dll(exe.data(),n);
        dll.resize(dll.find_last_of(L'\\')+1); dll+=L"dirsync_lifecycle_probe.dll";
        Fixture scan; for(int i=0;i<256;++i) scan.Dir(scan.left+L"\\d"+std::to_wstring(i));
        {
            // CI may return an 8.3 TEMP ancestor (e.g. RUNNER~1). The picker
            // deliberately returns full names, so compare against a full root.
            DWORD size=GetLongPathNameW(scan.root.c_str(),nullptr,0);
            Check(size!=0,"resolve full fixture root size");
            std::vector<wchar_t> root(size);
            DWORD count=GetLongPathNameW(scan.root.c_str(),root.data(),size);
            Check(count!=0 && count<size,"resolve full fixture root");
            std::wstring chosen(root.data(),count);
            for(wchar_t part:{L'階',L'長',L'文'}) {chosen+=L"\\"+std::wstring(90,part);scan.Dir(L"\\\\?\\"+chosen);}
            Module module(dll);
            using FolderTest=unsigned (WINAPI*)(const wchar_t*);
            auto folderTest=reinterpret_cast<FolderTest>(GetProcAddress(module.handle,"ProbeFolderPath"));
            Check(chosen.size()>260 && folderTest && folderTest(chosen.c_str())==3,"folder picker preserves long Unicode names and expands shell aliases; missing folder is refused");
            using ProposalTest=unsigned (WINAPI*)(const wchar_t*,const wchar_t*);
            auto proposals=reinterpret_cast<ProposalTest>(GetProcAddress(module.handle,"ProbeProposalRules"));
            Check(proposals && proposals(scan.left.c_str(),scan.right.c_str())==77,"proposal eligibility, selected/visible plan scope, menu availability, restore/reverse, hit testing and long Unicode path lists");
            using StopTest=BOOL (WINAPI*)();
            auto stops=reinterpret_cast<StopTest>(GetProcAddress(module.handle,"ProbeStopControls"));
            Check(stops && stops(),"only the appropriate adjacent Stop can cancel scanning, validation or file operations");
            using PreviewTest=BOOL (WINAPI*)(const wchar_t*,const wchar_t*);
            auto preview=reinterpret_cast<PreviewTest>(GetProcAddress(module.handle,"ProbePreviewControls"));
            Check(preview && preview(scan.left.c_str(),scan.right.c_str()),"independent toggle combinations, retained results/options, muted Skip and edge-to-edge list");
        }
        Fixture content; content.Sparse(content.left+L"\\same.bin"); content.Sparse(content.right+L"\\same.bin");
        for(int pass=0;pass<3;++pass) { Run(dll,scan,false); Run(dll,content,true); }
        for(bool verify:{false,true}) for(bool close:{false,true}) CopyControls(dll,verify,close);
        for(bool verify:{false,true}) for(bool close:{false,true}) CopyControls(dll,verify,close,true);
        for(bool keep:{false,true}) for(bool verify:{false,true}) for(bool close:{false,true}) CopyControls(dll,verify,close,false,true,keep);
        for(int mode=0;mode<4;++mode) CopyUnload(dll,mode);
        for(bool keep:{false,true}) for(int mode=0;mode<4;++mode) CopyUnload(dll,mode|8|(keep?0:16));
        for(bool keep:{false,true}) for(bool prior:{false,true}) for(unsigned control:{0u,128u,256u})
            DeleteControls(dll,(keep?0u:16u)|(prior?2u:0u)|control);
        for(bool verify:{false,true}) for(unsigned control:{0u,128u,256u}) DeleteControls(dll,64u|(verify?1u:0u)|control);
        std::cout<<checks<<" lifecycle checks passed\n"; return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAILED: "<<e.what()<<"\n"; return 1; }
}
