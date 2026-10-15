// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "copy_space_faults.h"
namespace dirsync_test {
namespace {
SpaceFault fault=SpaceFault::None;
DWORD errorCode=ERROR_DISK_FULL;
unsigned ordinal=1,reservations=0,copies=0;
bool reached=false;
HANDLE retained=INVALID_HANDLE_VALUE;
bool Prefix(const std::wstring& source,const std::wstring& target,DWORD disposition,DWORD limit,DWORD& written)
{
    constexpr DWORD shares=FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
    HANDLE in=::CreateFileW(source.c_str(),GENERIC_READ,shares,nullptr,OPEN_EXISTING,0,nullptr);
    if(in==INVALID_HANDLE_VALUE) return false;
    HANDLE out=::CreateFileW(target.c_str(),GENERIC_WRITE,shares,nullptr,disposition,0,nullptr);
    DWORD error=GetLastError();
    if(out==INVALID_HANDLE_VALUE) { CloseHandle(in); SetLastError(error); return false; }
    std::vector<BYTE> bytes(limit); DWORD read=0;
    bool ok=ReadFile(in,bytes.data(),limit,&read,nullptr)!=FALSE && read &&
        WriteFile(out,bytes.data(),read,&written,nullptr)!=FALSE && written==read;
    error=GetLastError(); CloseHandle(out); CloseHandle(in); SetLastError(error); return ok;
}
}
void ReleaseSpaceFault() { if(retained!=INVALID_HANDLE_VALUE) { CloseHandle(retained); retained=INVALID_HANDLE_VALUE; } }
void SetSpaceFault(SpaceFault value,DWORD error,unsigned item)
{ ReleaseSpaceFault(); fault=value; errorCode=error; ordinal=item; reservations=0; copies=0; reached=false; }
bool SpaceFaultReached() { return reached; }
HANDLE WINAPI ReserveFile(LPCWSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD disposition,DWORD flags,HANDLE templateFile)
{
    if(disposition==CREATE_NEW && std::wstring(path).find(L"\\.dirsync-")!=std::wstring::npos &&
       ++reservations==ordinal && fault==SpaceFault::Reserve) {
        reached=true; SetLastError(errorCode); return INVALID_HANDLE_VALUE;
    }
    return ::CreateFileW(path,access,share,security,disposition,flags,templateFile);
}
BOOL WINAPI TransferFile(LPCWSTR source,LPCWSTR target,LPPROGRESS_ROUTINE progress,LPVOID context,LPBOOL cancel,DWORD flags)
{
    ++copies;
    if(copies!=ordinal || (fault!=SpaceFault::MainData && fault!=SpaceFault::NamedStream && fault!=SpaceFault::RetainStage))
        return ::CopyFileExW(source,target,progress,context,cancel,flags);
    // Model a failing CopyFileExW which leaves real partial bytes in the owned
    // reservation. This is fault injection, not a full-volume or OS copy test.
    DWORD written=0;
    const bool ads=fault==SpaceFault::NamedStream;
    if(!Prefix(source,target,OPEN_EXISTING,ads?4:1024*1024,written)) return FALSE;
    DWORD mainWritten=written;
    if(ads && !Prefix(std::wstring(source)+L":漢字",std::wstring(target)+L":漢字",CREATE_NEW,1024*1024,written)) return FALSE;
    if(progress) {
        LARGE_INTEGER total{},transferred{},streamSize{},streamTransferred{};
        total.QuadPart=8*1024*1024+(ads?mainWritten:0);
        transferred.QuadPart=written+(ads?mainWritten:0);
        streamSize.QuadPart=8*1024*1024; streamTransferred.QuadPart=written;
        progress(total,transferred,streamSize,streamTransferred,ads?2:1,CALLBACK_CHUNK_FINISHED,nullptr,nullptr,context);
    }
    reached=true;
    if(fault==SpaceFault::RetainStage) {
        retained=::CreateFileW(target,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    }
    SetLastError(errorCode); return FALSE;
}
BOOL WINAPI PublishFile(LPCWSTR source,LPCWSTR target,DWORD flags)
{
    if(fault==SpaceFault::Publish && copies==ordinal) { reached=true; SetLastError(errorCode); return FALSE; }
    return ::MoveFileExW(source,target,flags);
}
}
// Compile the real adapter with only its three WinAPI calls redirected in tests.
// Production and lifecycle projects continue compiling ../copy.cpp directly.
#define CreateFileW ::dirsync_test::ReserveFile
#define CopyFileExW ::dirsync_test::TransferFile
#define MoveFileExW ::dirsync_test::PublishFile
#include "../copy.cpp"
