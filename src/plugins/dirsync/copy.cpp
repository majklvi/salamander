// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// Ordinary Win32 file operations following core worker patterns: new copies,
// independent recovery copies and explicitly selected existing-file replacements.
#include "precomp.h"
#include "copy.h"
#include "recovery.h"
#include "overwrite.h"
#include <aclapi.h>
#include <objbase.h>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <cstdio>

namespace dirsync {
namespace {
struct Failure { DWORD error; };
void Need(BOOL ok) { if (!ok) throw Failure{GetLastError()}; }
void Stop(const std::atomic<bool>& cancel) { if (cancel) throw Failure{ERROR_CANCELLED}; }
struct File {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit File(HANDLE value = INVALID_HANDLE_VALUE) : h(value) {}
    ~File() { Close(); }
    void Close() { if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = INVALID_HANDLE_VALUE; } }
    File(const File&) = delete; File& operator=(const File&) = delete;
};
constexpr DWORD shares = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
constexpr DWORD selectedAttrs = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
bool Unsupported(DWORD a) { return (a & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_ENCRYPTED |
    FILE_ATTRIBUTE_OFFLINE | 0x00040000 | 0x00400000)) != 0; }
HANDLE Open(const std::wstring& p, DWORD access, DWORD share = shares, DWORD disposition = OPEN_EXISTING)
{ return CreateFileW(ApiPath(p).c_str(), access, share, nullptr, disposition,
    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr); }
Entry Info(HANDLE h)
{
    BY_HANDLE_FILE_INFORMATION i{}; FILE_BASIC_INFO b{};
    Need(GetFileInformationByHandle(h, &i)); Need(GetFileInformationByHandleEx(h, FileBasicInfo, &b, sizeof(b)));
    Entry e; e.identified = true; e.volume = i.dwVolumeSerialNumber;
    e.id = (uint64_t(i.nFileIndexHigh) << 32) | i.nFileIndexLow;
    e.size = (uint64_t(i.nFileSizeHigh) << 32) | i.nFileSizeLow;
    e.time = (uint64_t(i.ftLastWriteTime.dwHighDateTime) << 32) | i.ftLastWriteTime.dwLowDateTime;
    e.change = uint64_t(b.ChangeTime.QuadPart); e.attributes = i.dwFileAttributes;
    e.directory = (i.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; e.links = i.nNumberOfLinks; return e;
}
bool SameId(const Entry& a, const Entry& b) { return a.identified && b.identified && a.volume == b.volume && a.id == b.id; }
bool Unchanged(const Entry& a, const Entry& b) { return SameId(a,b) && a.size == b.size && a.time == b.time &&
    a.change == b.change && a.attributes == b.attributes && a.links == b.links; }
std::wstring Join(const std::wstring& root, const std::wstring& relative)
{ return root + (root.back() == L'\\' ? L"" : L"\\") + relative; }
bool Relative(const std::wstring& p)
{
    if (p.empty() || p.front() == L'\\' || p.find_first_of(L"/:\0", 0, 3) != std::wstring::npos) return false;
    size_t start = 0;
    while (start < p.size()) {
        size_t end = p.find(L'\\', start); if (end == std::wstring::npos) end = p.size();
        auto part = p.substr(start, end - start);
        if (part.empty() || part == L"." || part == L".." || part.back() == L'.' || part.back() == L' ') return false;
        start = end + 1;
    }
    return p.back() != L'\\';
}
void Absent(const std::wstring& p)
{
    DWORD attr = GetFileAttributesW(ApiPath(p).c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) throw Failure{ERROR_FILE_EXISTS};
    DWORD error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) throw Failure{error};
}
void Parents(const std::wstring& root, const std::wstring& full, bool create, std::atomic<bool>& cancel)
{
    size_t end = full.find_last_of(L'\\'), base = 3;
    if (full.rfind(L"\\\\",0) == 0) {
        size_t server = full.find(L'\\',2); base = full.find(L'\\',server + 1);
        if (base == std::wstring::npos) throw Failure{ERROR_BAD_PATHNAME};
    }
    size_t n = base;
    for (;;) {
        Stop(cancel); std::wstring p = full.substr(0,n);
        File h(Open(p, FILE_READ_ATTRIBUTES));
        if (h.h == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (!create || n <= root.size() || (error != ERROR_PATH_NOT_FOUND && error != ERROR_FILE_NOT_FOUND)) throw Failure{error};
            if (!CreateDirectoryW(ApiPath(p).c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) throw Failure{GetLastError()};
            h.h = Open(p,FILE_READ_ATTRIBUTES); Need(h.h != INVALID_HANDLE_VALUE);
        }
        Entry e = Info(h.h);
        if (!e.directory || Unsupported(e.attributes)) throw Failure{ERROR_NOT_SUPPORTED};
        if (n >= end) break;
        size_t next = full.find(L'\\',n); if (next == n) next = full.find(L'\\',n + 1);
        n = next == std::wstring::npos || next > end ? end : next;
    }
}
struct Security {
    std::vector<BYTE> bytes;
    explicit Security(const std::wstring& p) {
        DWORD n=0; GetFileSecurityW(ApiPath(p).c_str(),DACL_SECURITY_INFORMATION,nullptr,0,&n);
        if (!n) throw Failure{GetLastError()}; bytes.resize(n);
        Need(GetFileSecurityW(ApiPath(p).c_str(),DACL_SECURITY_INFORMATION,bytes.data(),n,&n));
    }
    void Apply(const std::wstring& p) const {
        SECURITY_DESCRIPTOR_CONTROL c{}; DWORD revision=0;
        Need(GetSecurityDescriptorControl(const_cast<BYTE*>(bytes.data()), &c, &revision));
        Need(SetFileSecurityW(ApiPath(p).c_str(), DACL_SECURITY_INFORMATION |
            ((c & SE_DACL_PROTECTED) ? PROTECTED_DACL_SECURITY_INFORMATION : UNPROTECTED_DACL_SECURITY_INFORMATION),
            const_cast<BYTE*>(bytes.data())));
    }
    bool Matches(const std::wstring& p) const {
        Security b(p); PACL x=nullptr,y=nullptr; BOOL xp=FALSE,yp=FALSE,xd=FALSE,yd=FALSE;
        Need(GetSecurityDescriptorDacl(const_cast<BYTE*>(bytes.data()),&xp,&x,&xd));
        Need(GetSecurityDescriptorDacl(b.bytes.data(),&yp,&y,&yd));
        SECURITY_DESCRIPTOR_CONTROL xc{},yc{}; DWORD revision=0;
        Need(GetSecurityDescriptorControl(const_cast<BYTE*>(bytes.data()),&xc,&revision));
        Need(GetSecurityDescriptorControl(b.bytes.data(),&yc,&revision));
        return xp==yp && (xc & SE_DACL_PROTECTED)==(yc & SE_DACL_PROTECTED) &&
            ((!x && !y) || (x && y && x->AclSize == y->AclSize && memcmp(x,y,x->AclSize)==0));
    }
};
using Streams = std::map<std::wstring,uint64_t>;
#if defined(DIRSYNC_ENGINE_TEST) || defined(DIRSYNC_COPY_TEST_HOOKS)
thread_local std::function<void(CopyPhase,const std::wstring&)> hook;
#endif
Streams StreamsOf(const std::wstring& p)
{
    WIN32_FIND_STREAM_DATA d{}; HANDLE find = FindFirstStreamW(ApiPath(p).c_str(),FindStreamInfoStandard,&d,0);
    if (find == INVALID_HANDLE_VALUE) throw Failure{GetLastError()};
    Streams streams;
    try {
        do { streams.emplace(d.cStreamName,uint64_t(d.StreamSize.QuadPart)); } while (FindNextStreamW(find,&d));
        DWORD error=GetLastError(); if (error != ERROR_HANDLE_EOF) throw Failure{error};
    } catch (...) { FindClose(find); throw; }
    FindClose(find); return streams;
}
void Verify(const std::wstring& source, const std::wstring& stage, std::atomic<bool>& cancel, CopyProgress& progress)
{
    Streams a=StreamsOf(source), b=StreamsOf(stage);
    if (a != b) throw Failure{ERROR_CRC};
    std::vector<BYTE> x(256*1024), y(x.size());
    for (const auto& stream:a) {
        Stop(cancel); auto suffix=stream.first==L"::$DATA" ? L"" : stream.first;
        File in(Open(source+suffix,GENERIC_READ)), out(Open(stage+suffix,GENERIC_READ));
        Need(in.h != INVALID_HANDLE_VALUE && out.h != INVALID_HANDLE_VALUE);
        uint64_t total=0;
        for (;;) {
            Stop(cancel); DWORD nx=0,ny=0;
            Need(ReadFile(in.h,x.data(),DWORD(x.size()),&nx,nullptr));
            Need(ReadFile(out.h,y.data(),DWORD(y.size()),&ny,nullptr));
            if (nx!=ny || memcmp(x.data(),y.data(),nx)!=0) throw Failure{ERROR_CRC};
            if (!nx) break; total+=nx; progress.verified.fetch_add(uint64_t(nx)*2,std::memory_order_relaxed);
#if defined(DIRSYNC_ENGINE_TEST) || defined(DIRSYNC_COPY_TEST_HOOKS)
            if (hook) hook(CopyPhase::Verifying,stage);
#endif
        }
        if (total!=stream.second) throw Failure{ERROR_CRC};
    }
    if (StreamsOf(source)!=a || StreamsOf(stage)!=a) throw Failure{ERROR_FILE_INVALID};
}
void Phase(CopyProgress& p, CopyPhase phase, const std::wstring& path) {
    p.phase=phase;
#if defined(DIRSYNC_ENGINE_TEST) || defined(DIRSYNC_COPY_TEST_HOOKS)
    if (hook) hook(phase,path);
#else
    (void)path;
#endif
}
struct Callback { std::atomic<bool>& cancel; CopyProgress& progress; uint64_t base; const std::wstring& stage; };
DWORD CALLBACK CopyCallback(LARGE_INTEGER, LARGE_INTEGER transferred, LARGE_INTEGER, LARGE_INTEGER,
                           DWORD, DWORD, HANDLE, HANDLE, LPVOID context)
{
    auto& c=*static_cast<Callback*>(context);
    c.progress.copied=c.base+uint64_t(transferred.QuadPart);
#if defined(DIRSYNC_ENGINE_TEST) || defined(DIRSYNC_COPY_TEST_HOOKS)
    try { if (hook) hook(CopyPhase::Copying,c.stage); } catch (...) { c.cancel=true; }
#endif
    return c.cancel ? PROGRESS_CANCEL : PROGRESS_CONTINUE;
}
void CopyOne(const std::wstring& sourceRoot, const std::wstring& targetRoot, const Row& row,
             CopyOutcome& outcome, std::atomic<bool>& cancel, CopyProgress& progress,
             const Entry* original=nullptr,const Security* targetSecurity=nullptr)
{
    const Entry& expected=row.action==Action::CopyRight ? row.left : row.right;
    const std::wstring source=Join(sourceRoot,row.path), target=Join(targetRoot,row.path);
    Entry owned{}; std::wstring stage;
    try {
        if (!Relative(row.path)) throw Failure{ERROR_INVALID_NAME};
        Stop(cancel); Parents(sourceRoot,source,false,cancel); Parents(targetRoot,target,false,cancel);
    } catch (const Failure& e) {
        // Missing target descendants may be created, never missing roots/ancestors.
        if (e.error!=ERROR_PATH_NOT_FOUND && e.error!=ERROR_FILE_NOT_FOUND) throw;
        Parents(sourceRoot,source,false,cancel); Parents(targetRoot,target,true,cancel);
    }
    File in(Open(source,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE)); Need(in.h != INVALID_HANDLE_VALUE);
    if (!Unchanged(expected,Info(in.h))) throw Failure{ERROR_FILE_INVALID};
    auto checkTarget=[&]() {
        if(!original) { Absent(target); return; }
        File current(Open(target,FILE_READ_ATTRIBUTES|READ_CONTROL)); Need(current.h!=INVALID_HANDLE_VALUE);
        if(!Unchanged(*original,Info(current.h)) || SameId(expected,*original) || !targetSecurity || !targetSecurity->Matches(target))
            throw Failure{ERROR_FILE_INVALID};
        if(original->attributes & FILE_ATTRIBUTE_READONLY) throw Failure{ERROR_ACCESS_DENIED};
    };
    checkTarget(); Stop(cancel);
    try {
        auto parent=target.substr(0,target.find_last_of(L'\\'));
        for (int tries=0; tries<16; ++tries) {
            GUID guid{}; if (CoCreateGuid(&guid)!=S_OK) throw Failure{ERROR_GEN_FAILURE};
            wchar_t token[48]{}; if (!StringFromGUID2(guid,token,int(std::size(token)))) throw Failure{ERROR_GEN_FAILURE};
            stage=Join(parent,L".dirsync-"+std::wstring(token)+L".tmp");
            File f(Open(stage,GENERIC_WRITE|READ_CONTROL,0,CREATE_NEW));
            if (f.h==INVALID_HANDLE_VALUE) { DWORD error=GetLastError(); if (error==ERROR_FILE_EXISTS) continue; throw Failure{error}; }
            owned=Info(f.h); break;
        }
        if (!owned.identified) throw Failure{ERROR_FILE_EXISTS};
        Security inherited(stage);
        const Security& desired=targetSecurity?*targetSecurity:inherited;
        Callback callback{cancel,progress,progress.copied.load(),stage};
        Phase(progress,CopyPhase::Copying,stage); Stop(cancel);
        // Destination is our CREATE_NEW reservation, never the user's target.
        Need(CopyFileExW(ApiPath(source).c_str(),ApiPath(stage).c_str(),CopyCallback,&callback,nullptr,0));
        Stop(cancel);
        File staging(Open(stage,GENERIC_READ|FILE_WRITE_ATTRIBUTES)); Need(staging.h!=INVALID_HANDLE_VALUE);
        if (!SameId(owned,Info(staging.h))) throw Failure{ERROR_FILE_INVALID};
        Phase(progress,CopyPhase::Preparing,stage); Stop(cancel);
        // A normal Modify grant need not include WRITE_DAC. Avoid rewriting an
        // already correct inherited DACL; restore it only if copying changed it.
        if (!desired.Matches(stage)) desired.Apply(stage);
        FILETIME mtime{DWORD(expected.time),DWORD(expected.time>>32)};
        Need(SetFileTime(staging.h,nullptr,nullptr,&mtime));
        Need(SetFileAttributesW(ApiPath(stage).c_str(),(expected.attributes & selectedAttrs)|FILE_ATTRIBUTE_ARCHIVE));
        Phase(progress,CopyPhase::Verifying,stage); Verify(source,stage,cancel,progress);
        auto prepared=Info(staging.h);
        if (prepared.time!=expected.time || (prepared.attributes & selectedAttrs)!=(expected.attributes & selectedAttrs) || !desired.Matches(stage))
            throw Failure{ERROR_INVALID_DATA};
        if (!Unchanged(expected,Info(in.h))) throw Failure{ERROR_FILE_INVALID};
        Phase(progress,CopyPhase::Publishing,stage); Stop(cancel);
        if (!Unchanged(prepared,Info(staging.h)) || !desired.Matches(stage)) throw Failure{ERROR_FILE_INVALID};
        Parents(targetRoot,target,false,cancel); checkTarget();
        staging.Close();
        // Same-volume replacement changes this name, never the old hardlink's data.
        // There is deliberately no delete/recreate or cross-volume fallback.
        Need(MoveFileExW(ApiPath(stage).c_str(),ApiPath(target).c_str(),original?MOVEFILE_REPLACE_EXISTING:0));
        outcome.published=true;
        Phase(progress,CopyPhase::Published,target);
        File published(Open(target,FILE_READ_ATTRIBUTES|FILE_WRITE_ATTRIBUTES)); Need(published.h!=INVALID_HANDLE_VALUE);
        if (!SameId(owned,Info(published.h))) throw Failure{ERROR_FILE_INVALID};
        Need(SetFileTime(published.h,nullptr,nullptr,&mtime));
        Need(SetFileAttributesW(ApiPath(target).c_str(),(expected.attributes & selectedAttrs)|FILE_ATTRIBUTE_ARCHIVE));
        auto final=Info(published.h);
        if (final.time!=expected.time || (final.attributes & selectedAttrs)!=(expected.attributes & selectedAttrs) || !desired.Matches(target))
            throw Failure{ERROR_INVALID_DATA};
        outcome.status=CopyStatus::Copied; progress.completed.fetch_add(1); return;
    } catch (...) {
        // Only our unpublished staging can be removed, after checking its identity.
        // A published destination is never deleted or rolled back on failure.
        if (owned.identified && !outcome.published) {
            try {
                File f(Open(stage,FILE_READ_ATTRIBUTES|FILE_WRITE_ATTRIBUTES));
                if (f.h!=INVALID_HANDLE_VALUE) {
                    if (SameId(owned,Info(f.h))) {
                        SetFileAttributesW(ApiPath(stage).c_str(),FILE_ATTRIBUTE_NORMAL); f.Close();
                        if (!DeleteFileW(ApiPath(stage).c_str())) outcome.remainingStage=stage;
                    } else outcome.remainingStage=stage;
                } else if (GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PATH_NOT_FOUND) outcome.remainingStage=stage;
            } catch (const Failure&) { outcome.remainingStage=stage; }
        }
        throw;
    }
}
} // namespace
bool IsNewFileCopy(const Row& r)
{
    if (r.state==State::Blocked || r.state==State::Conflict) return false;
    const Entry* e=nullptr;
    if (r.action==Action::CopyRight && r.hasLeft && !r.hasRight) e=&r.left;
    if (r.action==Action::CopyLeft && r.hasRight && !r.hasLeft) e=&r.right;
    return e && e->identified && !e->directory && !e->error && !Unsupported(e->attributes) && Relative(r.path);
}
bool IsOriginalToSave(const Row& r)
{
    if (r.state==State::Blocked || r.state==State::Conflict || !r.hasLeft || !r.hasRight ||
        (r.action!=Action::CopyLeft && r.action!=Action::CopyRight) || !Relative(r.path)) return false;
    for (const Entry* e:{&r.left,&r.right})
        if (!e->identified || e->directory || e->error || Unsupported(e->attributes)) return false;
    return true;
}
namespace {
// A journal failure must never be mistaken for a completed recovery record.
#if defined(DIRSYNC_ENGINE_TEST) || defined(DIRSYNC_COPY_TEST_HOOKS)
thread_local std::function<DWORD(const std::wstring&)> recoveryLogHook;
#endif
std::wstring RecoveryCanonical(const std::wstring& path,std::atomic<bool>& cancel)
{
    Parents(path,Join(path,L"root-check"),false,cancel);
    File directory(Open(path,FILE_READ_ATTRIBUTES)); Need(directory.h!=INVALID_HANDLE_VALUE);
    DWORD n=GetFinalPathNameByHandleW(directory.h,nullptr,0,VOLUME_NAME_DOS); Need(n!=0);
    std::vector<wchar_t> buffer(size_t(n)+1);
    DWORD used=GetFinalPathNameByHandleW(directory.h,buffer.data(),DWORD(buffer.size()),VOLUME_NAME_DOS);
    Need(used && used<buffer.size());
    std::wstring result;
    if (!NormalizeRoot(std::wstring(buffer.data(),used),result)) throw Failure{ERROR_INVALID_NAME};
    return result;
}
bool RecoveryWithin(const std::wstring& root,const std::wstring& path)
{
    return path.size()>=root.size() && CompareStringOrdinal(root.c_str(),int(root.size()),path.c_str(),int(root.size()),TRUE)==CSTR_EQUAL &&
        (path.size()==root.size() || root.back()==L'\\' || path[root.size()]==L'\\');
}
void RecoveryDirectory(const std::wstring& path,std::atomic<bool>& cancel)
{
    Stop(cancel);
    if (!CreateDirectoryW(ApiPath(path).c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) throw Failure{GetLastError()};
    Parents(path,Join(path,L"directory-check"),false,cancel);
}
std::wstring RecoveryPair(const std::wstring& left,const std::wstring& right)
{
    auto text=left+L"\n"+right; CharUpperBuffW(&text[0],DWORD(text.size()));
    uint64_t hash=14695981039346656037ULL;
    for (wchar_t c:text) { hash^=uint16_t(c); hash*=1099511628211ULL; }
    wchar_t token[32]{}; swprintf_s(token,L"pair-%016llx",hash); return token;
}
std::wstring RecoveryRun(const std::wstring& pair,std::atomic<bool>& cancel)
{
    for (int i=0;i<16;++i) {
        Stop(cancel); GUID guid{}; if (CoCreateGuid(&guid)!=S_OK) throw Failure{ERROR_GEN_FAILURE};
        wchar_t token[48]{}; if (!StringFromGUID2(guid,token,int(std::size(token)))) throw Failure{ERROR_GEN_FAILURE};
        auto run=Join(pair,L"run-"+std::wstring(token));
        if (CreateDirectoryW(ApiPath(run).c_str(),nullptr)) return run;
        if (GetLastError()!=ERROR_ALREADY_EXISTS) throw Failure{GetLastError()};
    }
    throw Failure{ERROR_FILE_EXISTS};
}
std::wstring Json(const std::wstring& value)
{
    std::wstring out=L"\"";
    for (wchar_t c:value) {
        if (c==L'"' || c==L'\\') { out+=L'\\'; out+=c; }
        else if (c<32) { wchar_t escape[8]{}; swprintf_s(escape,L"\\u%04x",unsigned(c)); out+=escape; }
        else out+=c;
    }
    return out+L"\"";
}
void RecoveryLine(HANDLE journal,const std::wstring& line)
{
#if defined(DIRSYNC_ENGINE_TEST) || defined(DIRSYNC_COPY_TEST_HOOKS)
    if (recoveryLogHook) { DWORD error=recoveryLogHook(line); if (error) throw Failure{error}; }
#endif
    auto text=line+L"\n";
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.c_str(),int(text.size()),nullptr,0,nullptr,nullptr);
    if (!n) throw Failure{GetLastError()};
    std::vector<char> bytes(static_cast<size_t>(n));
    Need(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.c_str(),int(text.size()),bytes.data(),n,nullptr,nullptr)!=0);
    DWORD written=0; Need(WriteFile(journal,bytes.data(),DWORD(bytes.size()),&written,nullptr));
    if (written!=bytes.size()) throw Failure{ERROR_WRITE_FAULT};
    Need(FlushFileBuffers(journal));
}
std::wstring OutcomeLine(size_t index,const CopyOutcome& c,const wchar_t* completed=L"saved")
{
    const wchar_t* status=c.status==CopyStatus::Copied?completed:c.status==CopyStatus::Failed?L"failed":
        c.status==CopyStatus::Cancelled?L"cancelled":L"not_started";
    return L"{\"event\":\"result\",\"item\":"+std::to_wstring(index+1)+L",\"status\":"+Json(status)+
        L",\"error\":"+std::to_wstring(c.error)+L",\"published\":"+(c.published?L"true":L"false")+
        L",\"remainingStage\":"+Json(c.remainingStage)+L"}";
}
}
#if defined(DIRSYNC_ENGINE_TEST) || defined(DIRSYNC_COPY_TEST_HOOKS)
void SetRecoveryLogHook(std::function<DWORD(const std::wstring&)> callback) { recoveryLogHook=std::move(callback); }
#endif
RecoveryReport SaveOriginals(Result snapshot,const std::vector<size_t>& visible,const std::wstring& recoveryRoot,
    std::atomic<bool>& cancel,Progress& checking,CopyProgress& progress)
{
    RecoveryReport report; std::wstring left,right;
    File journal;
    try {
        std::set<size_t> unique;
        for (size_t i:visible) {
            if (i>=snapshot.rows.size() || !unique.insert(i).second) continue;
            const auto& row=snapshot.rows[i];
            if (IsOriginalToSave(row)) { RecoveryItem item; item.row=i; item.right=row.action==Action::CopyRight; report.items.push_back(std::move(item)); }
            else if (row.action!=Action::None) ++report.omitted;
        }
        if (report.items.empty()) return report;
        if (!SnapshotRoots(snapshot,left,right) || !Revalidate(snapshot,cancel,checking)) {
            report.stale=true; report.cancelled=cancel.load(); return report;
        }
        std::wstring root;
        if (!NormalizeRoot(recoveryRoot,root)) throw Failure{ERROR_INVALID_NAME};
        root=RecoveryCanonical(root,cancel);
        for (const auto& input:{RecoveryCanonical(left,cancel),RecoveryCanonical(right,cancel)})
            if (RecoveryWithin(root,input) || RecoveryWithin(input,root)) throw Failure{ERROR_INVALID_PARAMETER};
        auto pair=Join(root,RecoveryPair(RecoveryCanonical(left,cancel),RecoveryCanonical(right,cancel)));
        RecoveryDirectory(pair,cancel); report.runDirectory=RecoveryRun(pair,cancel);
        File run(Open(report.runDirectory,FILE_READ_ATTRIBUTES)); Need(run.h!=INVALID_HANDLE_VALUE); const Entry runId=Info(run.h); run.Close();
        report.journal=Join(report.runDirectory,L"manifest.jsonl");
        journal.h=Open(report.journal,GENERIC_WRITE,FILE_SHARE_READ,CREATE_NEW); Need(journal.h!=INVALID_HANDLE_VALUE);
        auto append=[&](const std::wstring& line) {
            try { RecoveryLine(journal.h,line); }
            catch (const Failure& failure) { report.journalError=failure.error; throw; }
        };
        append(L"{\"event\":\"run\",\"schema\":1,\"operation\":\"save_originals_only\",\"left\":"+
            Json(left)+L",\"right\":"+Json(right)+L",\"runDirectory\":"+Json(report.runDirectory)+L"}");
        for (size_t n=0;n<report.items.size();++n) {
            auto& item=report.items[n]; const auto& row=snapshot.rows[item.row];
            const auto& entry=item.right?row.right:row.left;
            item.original=Join(item.right?right:left,row.path);
            auto side=Join(report.runDirectory,item.right?L"right":L"left"); RecoveryDirectory(side,cancel);
            auto operation=Join(side,std::to_wstring(n+1)); RecoveryDirectory(operation,cancel);
            auto data=Join(operation,L"data"); RecoveryDirectory(data,cancel);
            item.saved=Join(data,row.path); item.copy.row=item.row; item.copy.target=item.saved;
            append(L"{\"event\":\"planned\",\"item\":"+std::to_wstring(n+1)+L",\"side\":"+
                Json(item.right?L"right":L"left")+L",\"relative\":"+Json(row.path)+L",\"original\":"+Json(item.original)+
                L",\"saved\":"+Json(item.saved)+L",\"size\":"+std::to_wstring(entry.size)+L",\"mtime\":"+
                std::to_wstring(entry.time)+L",\"attributes\":"+std::to_wstring(entry.attributes)+L"}");
        }
        size_t n=0;
        for (;n<report.items.size();++n) {
            auto& item=report.items[n]; const auto& row=snapshot.rows[item.row]; progress.item=n;
            try {
                Stop(cancel); File currentRun(Open(report.runDirectory,FILE_READ_ATTRIBUTES)); Need(currentRun.h!=INVALID_HANDLE_VALUE);
                if (!SameId(runId,Info(currentRun.h))) throw Failure{ERROR_FILE_INVALID};
                Row original; original.path=row.path; original.hasLeft=true; original.left=item.right?row.right:row.left; original.action=Action::CopyRight;
                auto data=item.saved.substr(0,item.saved.size()-row.path.size()-1);
                CopyOne(item.right?right:left,data,original,item.copy,cancel,progress);
            } catch (const Failure& failure) {
                item.copy.error=failure.error; report.error=failure.error;
                report.cancelled=cancel.load() || failure.error==ERROR_CANCELLED || failure.error==ERROR_OPERATION_ABORTED || failure.error==ERROR_REQUEST_ABORTED;
                item.copy.status=report.cancelled?CopyStatus::Cancelled:CopyStatus::Failed;
            } catch (...) { item.copy.error=report.error=ERROR_NOT_ENOUGH_MEMORY; item.copy.status=CopyStatus::Failed; }
            append(OutcomeLine(n,item.copy)); item.logged=true;
            if (item.copy.status!=CopyStatus::Copied) { ++n; break; }
        }
        for (;n<report.items.size();++n) { append(OutcomeLine(n,report.items[n].copy)); report.items[n].logged=true; }
        append(L"{\"event\":\"end\",\"status\":"+Json(report.error?(report.cancelled?L"cancelled":L"failed"):L"complete")+L"}");
        report.complete=!report.error;
    } catch (const Failure& failure) {
        report.error=failure.error;
        report.cancelled=cancel.load() || failure.error==ERROR_CANCELLED || failure.error==ERROR_OPERATION_ABORTED || failure.error==ERROR_REQUEST_ABORTED;
    }
    catch (...) { report.error=ERROR_NOT_ENOUGH_MEMORY; }
    Phase(progress,CopyPhase::Finished,L""); return report;
}
bool IsFileOverwrite(const Row& row) { return IsOriginalToSave(row); }
OverwriteReport OverwriteFiles(Result snapshot,const std::vector<size_t>& visible,bool keepOriginals,
    const std::wstring& recoveryRoot,std::atomic<bool>& cancel,Progress& checking,CopyProgress& progress)
{
    OverwriteReport report; report.keepOriginals=keepOriginals;
    std::wstring left,right; File journal;
    try {
        std::set<size_t> unique; std::vector<size_t> selected;
        for(size_t i:visible) {
            if(i>=snapshot.rows.size() || !unique.insert(i).second) continue;
            const auto& row=snapshot.rows[i];
            if(IsFileOverwrite(row)) { report.items.push_back({i}); selected.push_back(i); }
            else if(row.action!=Action::None) ++report.omitted;
        }
        if(report.items.empty()) return report;
        if(!SnapshotRoots(snapshot,left,right) || !Revalidate(snapshot,cancel,checking)) {
            report.stale=true; report.cancelled=cancel.load(); return report;
        }
        for(auto& item:report.items) {
            const auto& row=snapshot.rows[item.row]; bool toRight=row.action==Action::CopyRight;
            item.target=Join(toRight?right:left,row.path);
            // Do not change attributes on a read-only original or its other aliases.
            if((toRight?row.right:row.left).attributes & FILE_ATTRIBUTE_READONLY) {
                item.status=CopyStatus::Failed; item.error=ERROR_ACCESS_DENIED; throw Failure{item.error};
            }
        }
        if(keepOriginals) {
            report.recovery=SaveOriginals(snapshot,selected,recoveryRoot,cancel,checking,progress);
            if(!report.recovery.complete || report.recovery.items.size()!=report.items.size() ||
                !std::all_of(report.recovery.items.begin(),report.recovery.items.end(),
                    [](const RecoveryItem& item) { return item.logged && item.copy.status==CopyStatus::Copied; })) {
                report.stale=report.recovery.stale; report.cancelled=report.recovery.cancelled;
                report.error=report.recovery.error?report.recovery.error:ERROR_INVALID_DATA;
                return report;
            }
            report.journal=Join(report.recovery.runDirectory,L"overwrite.jsonl");
            journal.h=Open(report.journal,GENERIC_WRITE,FILE_SHARE_READ,CREATE_NEW); Need(journal.h!=INVALID_HANDLE_VALUE);
        }
        auto append=[&](const std::wstring& line) {
            if(!keepOriginals) return;
            try { RecoveryLine(journal.h,line); }
            catch(const Failure& failure) { report.journalError=failure.error; throw; }
        };
        append(L"{\"event\":\"run\",\"schema\":1,\"operation\":\"overwrite\",\"keepOriginals\":true,\"left\":"+
            Json(left)+L",\"right\":"+Json(right)+L",\"recoveryManifest\":"+Json(report.recovery.journal)+L"}");
        for(size_t n=0;n<report.items.size();++n) {
            const auto& item=report.items[n]; const auto& row=snapshot.rows[item.row]; bool toRight=row.action==Action::CopyRight;
            append(L"{\"event\":\"planned\",\"item\":"+std::to_wstring(n+1)+L",\"source\":"+Json(Join(toRight?left:right,row.path))+
                L",\"target\":"+Json(item.target)+L",\"saved\":"+Json(keepOriginals?report.recovery.items[n].saved:L"")+L"}");
        }
        size_t n=0;
        for(;n<report.items.size();++n) {
            auto& item=report.items[n]; const auto& row=snapshot.rows[item.row]; bool toRight=row.action==Action::CopyRight;
            progress.item=n+(keepOriginals?report.items.size():0);
            try {
                Stop(cancel); Security originalSecurity(item.target);
                const Entry& original=toRight?row.right:row.left;
                CopyOne(toRight?left:right,toRight?right:left,row,item,cancel,progress,&original,&originalSecurity);
            } catch(const Failure& failure) {
                item.error=report.error=failure.error;
                report.cancelled=cancel.load() || failure.error==ERROR_CANCELLED || failure.error==ERROR_OPERATION_ABORTED || failure.error==ERROR_REQUEST_ABORTED;
                item.status=report.cancelled?CopyStatus::Cancelled:CopyStatus::Failed;
            } catch(...) { item.error=report.error=ERROR_NOT_ENOUGH_MEMORY; item.status=CopyStatus::Failed; }
            append(OutcomeLine(n,item,L"replaced"));
            if(item.status!=CopyStatus::Copied) { ++n; break; }
        }
        for(;n<report.items.size();++n) append(OutcomeLine(n,report.items[n],L"replaced"));
        append(L"{\"event\":\"end\",\"status\":"+Json(report.error?(report.cancelled?L"cancelled":L"failed"):L"complete")+L"}");
        report.complete=!report.error;
    } catch(const Failure& failure) {
        report.error=failure.error;
        report.cancelled=cancel.load() || failure.error==ERROR_CANCELLED || failure.error==ERROR_OPERATION_ABORTED || failure.error==ERROR_REQUEST_ABORTED;
    } catch(...) { report.error=ERROR_NOT_ENOUGH_MEMORY; }
    Phase(progress,CopyPhase::Finished,L""); return report;
}
CopyReport CopyNewFiles(Result snapshot,const std::vector<size_t>& visible,std::atomic<bool>& cancel,Progress& checking,CopyProgress& progress)
{
    CopyReport report; std::wstring left,right;
    std::set<size_t> unique;
    for (size_t i:visible) {
        if (i>=snapshot.rows.size() || !unique.insert(i).second) continue;
        const auto& r=snapshot.rows[i];
        if (IsNewFileCopy(r)) report.items.push_back({i});
        else if (r.action!=Action::None) ++report.omitted;
    }
    if (report.items.empty()) return report;
    if (!SnapshotRoots(snapshot,left,right) || !Revalidate(snapshot,cancel,checking)) {
        report.stale=true; report.cancelled=cancel.load(); return report;
    }
    for (size_t n=0;n<report.items.size();++n) {
        auto& outcome=report.items[n]; const auto& row=snapshot.rows[outcome.row];
        bool toRight=row.action==Action::CopyRight; outcome.target=Join(toRight?right:left,row.path); progress.item=n;
        try { Stop(cancel); CopyOne(toRight?left:right,toRight?right:left,row,outcome,cancel,progress); }
        catch (const Failure& e) {
            outcome.error=e.error; report.cancelled=cancel.load() || e.error==ERROR_CANCELLED || e.error==ERROR_REQUEST_ABORTED || e.error==ERROR_OPERATION_ABORTED;
            outcome.status=report.cancelled?CopyStatus::Cancelled:CopyStatus::Failed; break;
        }
        catch (...) { outcome.error=ERROR_NOT_ENOUGH_MEMORY; outcome.status=CopyStatus::Failed; break; }
    }
    Phase(progress,CopyPhase::Finished,L""); return report;
}
#if defined(DIRSYNC_ENGINE_TEST) || defined(DIRSYNC_COPY_TEST_HOOKS)
void SetCopyHook(std::function<void(CopyPhase,const std::wstring&)> value) { hook=std::move(value); }
#endif
} // namespace dirsync
