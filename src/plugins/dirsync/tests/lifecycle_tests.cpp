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
    void Sparse(const std::wstring& path)
    {
        HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
        Check(f!=INVALID_HANDLE_VALUE,"fixture handle"); files.push_back(path);
        DWORD returned=0; LARGE_INTEGER end{}; end.QuadPart=64LL*1024*1024*1024; FILETIME ft{0,0};
        bool ok=DeviceIoControl(f,FSCTL_SET_SPARSE,nullptr,0,nullptr,0,&returned,nullptr)!=FALSE;
        if(ok) ok=SetFilePointerEx(f,end,nullptr,FILE_BEGIN) && SetEndOfFile(f) && SetFileTime(f,nullptr,nullptr,&ft);
        CloseHandle(f); Check(ok,"sparse data and time");
    }
    ~Fixture() { for(const auto& p:files) DeleteFileW(p.c_str()); for(auto i=directories.rbegin();i!=directories.rend();++i) RemoveDirectoryW(i->c_str()); }
};
struct Module
{
    using Start=BOOL (WINAPI*)(const wchar_t*,const wchar_t*,BOOL);
    using Status=unsigned (WINAPI*)();
    using Release=unsigned (WINAPI*)(BOOL);
    HMODULE handle=nullptr; Start start=nullptr; Status status=nullptr; Release release=nullptr;
    explicit Module(const std::wstring& path)
    {
        handle=LoadLibraryW(path.c_str());
        if(!handle) std::cerr<<"LoadLibrary error "<<GetLastError()<<"\n";
        Check(handle!=nullptr,"load probe DLL");
        start=reinterpret_cast<Start>(GetProcAddress(handle,"ProbeStart"));
        status=reinterpret_cast<Status>(GetProcAddress(handle,"ProbeStatus"));
        release=reinterpret_cast<Release>(GetProcAddress(handle,"ProbeRelease"));
        Check(start && status && release,"probe exports");
    }
    ~Module() { if(handle) { if(release) release(TRUE); FreeLibrary(handle); } }
};
void Run(const std::wstring& dll,Fixture& f,bool content)
{
    Module module(dll);
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
        std::cout<<checks<<" lifecycle checks passed\n"; return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAILED: "<<e.what()<<"\n"; return 1; }
}
