// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
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
    void Data(const std::wstring& path)
    {
        HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
        Check(f!=INVALID_HANDLE_VALUE,"copy fixture handle"); files.push_back(path);
        std::vector<BYTE> data(1024*1024,42); DWORD n=0;
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
    }
    ~Module() { if(handle) { if(release) release(TRUE); FreeLibrary(handle); } }
};
void Run(const std::wstring& dll,Fixture& f,bool content)
{
    Module module(dll);
    auto settingsTest=reinterpret_cast<Module::Status>(GetProcAddress(module.handle,"ProbeSettingsTest"));
    Check(settingsTest && settingsTest()==13,"persistent recovery settings: defaults, disabled/enabled, long Unicode paths and malformed values");
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
void CopyControls(const std::wstring& dll,bool verify,bool close,bool recovery=false)
{
    Fixture f; f.Data(f.left+L"\\a"); f.Data(f.left+L"\\b"); Module module(dll);
    if(recovery) { f.Data(f.right+L"\\a"); f.Data(f.right+L"\\b"); f.Dir(f.root+L"\\recovery"); }
    Check(module.copyStart(f.left.c_str(),f.right.c_str(),(verify?1:0)|(recovery?4:0))!=FALSE,"start copy lifecycle worker");
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
    Check(empty!=recovery,"Stop/Close leaves originals or empty new-file destination");
    if(recovery) f.TrackRecovery(f.root+L"\\recovery");
    for(const auto& path:f.files) {
        HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
        Check(h!=INVALID_HANDLE_VALUE,"source handles released after Stop/Close join"); if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    Check(FreeLibrary(module.handle)!=FALSE,"unload after copy Stop/Close"); module.handle=nullptr;
    std::cout<<(recovery?"recovery ":"")<<(close?"Close":"Stop")<<" during "<<(verify?"Verify":"copy")<<": handlers and join passed\n";
}
void CopyUnload(const std::wstring& dll,int mode)
{
    Fixture f;
    if(mode&2) { f.Data(f.left+L"\\0-done"); f.Data(f.left+L"\\0-done:note"); }
    f.Data(f.left+L"\\a"); f.Data(f.left+L"\\b"); Module module(dll);
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
            ++count; Check((mode&2) && !wcscmp(entry.cFileName,L"0-done"),"only completed target remains after unload");
        } } while(FindNextFileW(find,&entry));
        FindClose(find);
    }
    Check(count==((mode&2)?1u:0u),"no unpublished target or stage after unload");
    if(mode&2) {
        for(const auto& suffix:{L"\\0-done",L"\\0-done:note"}) {
            std::wstring path=f.right+suffix; f.files.push_back(path);
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
    std::cout<<"force Release during "<<((mode&1)?"Verify":"copy")<<", prior completed="<<((mode&2)?1:0)<<": passed\n";
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
        Fixture content; content.Sparse(content.left+L"\\same.bin"); content.Sparse(content.right+L"\\same.bin");
        for(int pass=0;pass<3;++pass) { Run(dll,scan,false); Run(dll,content,true); }
        for(bool verify:{false,true}) for(bool close:{false,true}) CopyControls(dll,verify,close);
        for(bool verify:{false,true}) for(bool close:{false,true}) CopyControls(dll,verify,close,true);
        for(int mode=0;mode<4;++mode) CopyUnload(dll,mode);
        std::cout<<checks<<" lifecycle checks passed\n"; return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAILED: "<<e.what()<<"\n"; return 1; }
}
