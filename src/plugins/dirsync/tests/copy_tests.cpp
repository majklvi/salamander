// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "../copy.h"
#include <sddl.h>
#include <iostream>
#include <stdexcept>
using namespace dirsync;
namespace {
int checks=0, sequence=0;
void Check(bool ok,const char* name) { ++checks; if (!ok) throw std::runtime_error(name); }
void Put(const std::wstring& p,const std::string& data,bool fresh=true) {
    HANDLE h=CreateFileW(ApiPath(p).c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,fresh?CREATE_NEW:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Check(h!=INVALID_HANDLE_VALUE,"fixture open"); DWORD n=0;
    bool ok=WriteFile(h,data.data(),DWORD(data.size()),&n,nullptr)!=FALSE; CloseHandle(h);
    Check(ok && n==data.size(),"fixture write");
}
std::string Read(const std::wstring& p) {
    HANDLE h=CreateFileW(ApiPath(p).c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    Check(h!=INVALID_HANDLE_VALUE,"read copied fixture");
    char buffer[4096]; std::string data; DWORD n=0;
    while (ReadFile(h,buffer,sizeof(buffer),&n,nullptr) && n) data.append(buffer,n);
    CloseHandle(h); return data;
}
void Cleanup(const std::wstring& root) {
    WIN32_FIND_DATAW d{}; HANDLE f=FindFirstFileW((ApiPath(root)+L"\\*").c_str(),&d);
    if (f!=INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(d.cFileName,L".") || !wcscmp(d.cFileName,L"..")) continue;
            auto p=root+L"\\"+d.cFileName;
            if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!(d.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) Cleanup(p);
                SetFileAttributesW(ApiPath(p).c_str(),FILE_ATTRIBUTE_NORMAL); RemoveDirectoryW(ApiPath(p).c_str());
            } else { SetFileAttributesW(ApiPath(p).c_str(),FILE_ATTRIBUTE_NORMAL); DeleteFileW(ApiPath(p).c_str()); }
        } while (FindNextFileW(f,&d)); FindClose(f);
    }
}
struct Fixture {
    std::wstring root,left,right;
    std::atomic<bool> cancel{false}; Progress checking; CopyProgress progress;
    Fixture() {
        std::vector<wchar_t> temp(32768); DWORD n=GetTempPathW(DWORD(temp.size()),temp.data()); Check(n && n<temp.size(),"local temp");
        root=std::wstring(temp.data())+L"dirsync-copy-tests-"+std::to_wstring(GetCurrentProcessId())+L"-"+
            std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(++sequence);
        Check(CreateDirectoryW(ApiPath(root).c_str(),nullptr)!=FALSE,"new owned fixture");
        left=root+L"\\left"; right=root+L"\\right"; Dir(left); Dir(right);
    }
    void Dir(const std::wstring& p) { Check(CreateDirectoryW(ApiPath(p).c_str(),nullptr)!=FALSE,"fixture dir"); }
    Result Scan(bool asymmetric=false) { Options o; o.asymmetric=asymmetric; return Compare(left,right,o,cancel,checking); }
    CopyReport Run(Result r,const std::vector<size_t>& selected={}) {
        std::vector<size_t> rows=selected; if(rows.empty()) for(size_t i=0;i<r.rows.size();++i) rows.push_back(i);
        return CopyNewFiles(std::move(r),rows,cancel,checking,progress);
    }
    void NoStaging(const std::wstring& dir) {
        WIN32_FIND_DATAW d{}; HANDLE f=FindFirstFileW((ApiPath(dir)+L"\\.dirsync-*").c_str(),&d);
        bool absent=f==INVALID_HANDLE_VALUE; if(!absent) FindClose(f); Check(absent,"no unreported staging left");
    }
    ~Fixture() { SetCopyHook({}); Cleanup(root); RemoveDirectoryW(ApiPath(root).c_str()); }
};
void Acl(const std::wstring& path,const wchar_t* text) {
    PSECURITY_DESCRIPTOR sd=nullptr; Check(ConvertStringSecurityDescriptorToSecurityDescriptorW(text,SDDL_REVISION_1,&sd,nullptr)!=FALSE,"SDDL");
    BOOL ok=SetFileSecurityW(ApiPath(path).c_str(),DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,sd);
    LocalFree(sd); Check(ok!=FALSE,"fixture DACL");
}
std::wstring Dacl(const std::wstring& path) {
    DWORD n=0; GetFileSecurityW(ApiPath(path).c_str(),DACL_SECURITY_INFORMATION,nullptr,0,&n); std::vector<BYTE> sd(n);
    Check(n && GetFileSecurityW(ApiPath(path).c_str(),DACL_SECURITY_INFORMATION,sd.data(),n,&n),"get DACL");
    LPWSTR text=nullptr; Check(ConvertSecurityDescriptorToStringSecurityDescriptorW(sd.data(),SDDL_REVISION_1,DACL_SECURITY_INFORMATION,&text,nullptr)!=FALSE,"DACL text");
    std::wstring value(text); LocalFree(text);
    // SE_DACL_AUTO_INHERITED is bookkeeping, not an access/protection difference.
    if(value.rfind(L"D:AI",0)==0) value.erase(2,2);
    return value;
}
void Success() {
    Fixture f; Put(f.left+L"\\a", "DATA"); Put(f.left+L"\\a:one","ADS"); Put(f.left+L"\\a:漢字","UNICODE"); Put(f.left+L"\\empty","");
    Acl(f.right,L"D:P(A;OICI;FA;;;OW)(A;OICI;FR;;;BU)");
    Acl(f.left+L"\\a",L"D:P(A;;FA;;;OW)(A;;FR;;;WD)");
    Check(SetFileAttributesW(ApiPath(f.left+L"\\a").c_str(),FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)!=FALSE,"source flags");
    auto r=f.Scan(); auto expected=r.rows[0].left.time; auto report=f.Run(r);
    for(auto& item:report.items) if(item.error) std::cerr<<"success error "<<item.error<<"\n";
    Check(report.items.size()==2 && report.items[0].status==CopyStatus::Copied && report.items[1].status==CopyStatus::Copied,"new copies succeed");
    Check(Read(f.right+L"\\a")=="DATA" && Read(f.right+L"\\a:one")=="ADS" && Read(f.right+L"\\a:漢字")=="UNICODE","exact data and ADS");
    Check(Read(f.right+L"\\empty").empty(),"empty file");
    Put(f.right+L"\\reference","ACL");
    Check(Dacl(f.right+L"\\a")==Dacl(f.right+L"\\reference") && Dacl(f.right+L"\\a")!=Dacl(f.left+L"\\a"),"destination inherits, not source DACL");
    DWORD attr=GetFileAttributesW(ApiPath(f.right+L"\\a").c_str());
    Check((attr & (FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))==(FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM),"source selected flags retained");
    HANDLE h=CreateFileW(ApiPath(f.right+L"\\a").c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    FILETIME t{}; Check(h!=INVALID_HANDLE_VALUE && GetFileTime(h,nullptr,nullptr,&t),"read final mtime"); CloseHandle(h);
    Check(((uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime)==expected,"final mtime"); f.NoStaging(f.right);
    Check(f.progress.copied>0 && f.progress.verified>0 && f.progress.completed==2,"copy and Verify progress");
    Fixture link; Put(link.left+L"\\source","OLD");
    Check(CreateHardLinkW(ApiPath(link.root+L"\\alias").c_str(),ApiPath(link.left+L"\\source").c_str(),nullptr)!=FALSE,"source hardlink fixture");
    auto independent=link.Run(link.Scan());
    Check(independent.items.size()==1 && independent.items[0].status==CopyStatus::Copied,"hardlinked source copied");
    Put(link.root+L"\\alias","NEW",false);
    Check(Read(link.right+L"\\source")=="OLD","new target independent of source hardlinks");
}
void Scope() {
    Fixture f; Put(f.left+L"\\new","N"); Put(f.left+L"\\same","SOURCE"); Put(f.right+L"\\same","ORIGINAL"); Put(f.right+L"\\delete","KEEP");
    auto r=f.Scan(true); for(auto& row:r.rows) if(row.path==L"same") row.action=Action::CopyRight;
    auto report=f.Run(r); Check(report.items.size()==1 && report.items[0].status==CopyStatus::Copied,"only new eligible");
    Check(Read(f.right+L"\\same")=="ORIGINAL" && Read(f.right+L"\\delete")=="KEEP","no user overwrite or delete");
    Check(report.omitted==2,"unsupported proposals counted");
    auto repeat=f.Run(r); Check(repeat.stale,"old plan cannot be executed again");
    Fixture reverse; Put(reverse.right+L"\\back","RIGHT"); auto back=reverse.Run(reverse.Scan());
    Check(back.items.size()==1 && back.items[0].status==CopyStatus::Copied && Read(reverse.left+L"\\back")=="RIGHT","right to left");
    Fixture filter; Put(filter.left+L"\\a","A"); Put(filter.left+L"\\b","B"); auto selected=filter.Run(filter.Scan(),{0});
    Check(selected.items.size()==1 && GetFileAttributesW(ApiPath(filter.right+L"\\b").c_str())==INVALID_FILE_ATTRIBUTES,"only frozen visible rows execute");
}
void Boundaries() {
    for(int mode=0;mode<9;++mode) {
        Fixture f; Put(f.left+L"\\a",std::string(1024*1024,'A')); Put(f.left+L"\\b","B"); auto r=f.Scan();
        bool injected=false;
        SetCopyHook([&](CopyPhase phase,const std::wstring& stage) {
            if(injected) return;
            if(mode==0 && phase==CopyPhase::Publishing) { injected=true; Put(f.right+L"\\a","FOREIGN"); }
            if(mode==1 && phase==CopyPhase::Verifying) { injected=true; Put(stage,"CORRUPT",false); }
            if(mode==2 && phase==CopyPhase::Verifying) { injected=true; f.cancel=true; }
            if(mode==3 && phase==CopyPhase::Copying && f.progress.copied>0) { injected=true; f.cancel=true; }
            if(mode==4 && phase==CopyPhase::Publishing) { injected=true; f.cancel=true; }
            if(mode==5 && phase==CopyPhase::Verifying) { injected=true; Put(stage+L":extra","FOREIGN-ADS"); }
            if(mode==6 && phase==CopyPhase::Verifying) { injected=true; Put(f.left+L"\\a","MUTATED",false); }
            if(mode==7 && phase==CopyPhase::Verifying) { injected=true; Acl(stage,L"D:P(A;;FA;;;OW)(A;;FR;;;WD)"); }
            if(mode==8 && phase==CopyPhase::Publishing) { injected=true; Put(stage,"CHANGED",false); }
        });
        auto report=f.Run(r); SetCopyHook({});
        Check(injected && report.items.size()==2 && report.items[0].status!=CopyStatus::Copied,"injected copy failure detected");
        Check(report.items[1].status==CopyStatus::NotStarted,"later item not started after failure/Stop");
        if(mode==0) Check(Read(f.right+L"\\a")=="FOREIGN","publish collision never replaced");
        else Check(GetFileAttributesW(ApiPath(f.right+L"\\a").c_str())==INVALID_FILE_ATTRIBUTES,"failed or stopped copy never published");
        Check(report.items[0].remainingStage.empty(),"stage cleanup reported success"); f.NoStaging(f.right);
    }
    Fixture stale; Put(stale.left+L"\\a","A"); auto old=stale.Scan(); Put(stale.right+L"\\a","FOREIGN");
    auto report=stale.Run(old); Check(report.stale && Read(stale.right+L"\\a")=="FOREIGN","stale plan rejected before writes");
    Fixture cancelled; Put(cancelled.left+L"\\a","A"); auto r=cancelled.Scan(); cancelled.cancel=true;
    Check(cancelled.Run(r).cancelled,"cancel before run"); cancelled.NoStaging(cancelled.right);
    Fixture partial; Put(partial.left+L"\\a","A"); Put(partial.left+L"\\b","B"); auto two=partial.Scan();
    SetCopyHook([&](CopyPhase phase,const std::wstring&) { if(phase==CopyPhase::Copying && partial.progress.completed==1) partial.cancel=true; });
    auto stopped=partial.Run(two); SetCopyHook({});
    Check(stopped.items[0].status==CopyStatus::Copied && stopped.cancelled && Read(partial.right+L"\\a")=="A","Stop preserves completed new target");
    Check(GetFileAttributesW(ApiPath(partial.right+L"\\b").c_str())==INVALID_FILE_ATTRIBUTES,"Stop before second target"); partial.NoStaging(partial.right);
    Fixture denied; Put(denied.left+L"\\a","A");
    Acl(denied.right,L"D:P(D;;0x2;;;WD)(A;;FA;;;OW)");
    auto plan=denied.Scan();
    auto error=denied.Run(plan); Acl(denied.right,L"D:P(A;;FA;;;OW)");
    if(error.stale || error.items.empty() || error.items[0].error!=ERROR_ACCESS_DENIED)
        std::cerr<<"denied stale="<<error.stale<<" items="<<error.items.size()<<" code="<<(error.items.empty()?0:error.items[0].error)<<"\n";
    Check(error.items.size()==1 && error.items[0].status==CopyStatus::Failed && error.items[0].error==ERROR_ACCESS_DENIED,"actual denied destination write");
    Check(GetFileAttributesW(ApiPath(denied.right+L"\\a").c_str())==INVALID_FILE_ATTRIBUTES,"denied copy publishes nothing");
}
void Paths() {
    Fixture f; auto left=f.left,right=f.right;
    for(int i=0;i<6;++i) { auto name=L"\\漢字-žluťoučký-"+std::wstring(40,L'x')+std::to_wstring(i); left+=name; right+=name; f.Dir(left); }
    auto filename=L"\\"+std::wstring(180,L'j')+L"emoji-😀.txt";
    Put(left+filename,"LONG"); Put(left+filename+L":žluťoučký","ADS");
    Check(left.size()>260,"long UTF16 fixture"); auto report=f.Run(f.Scan());
    for(auto& item:report.items) if(item.error) std::cerr<<"path error "<<item.error<<"\n";
    Check(report.items.size()==1 && report.items[0].status==CopyStatus::Copied,"copy creates required Unicode parents");
    Check(Read(right+filename)=="LONG" && Read(right+filename+L":žluťoučký")=="ADS","long name, Unicode and ADS copy");
    f.NoStaging(right);
    Fixture utf8; auto a=utf8.left,b=utf8.right;
    for(int i=0;i<2;++i) { auto name=L"\\"+std::wstring(38,L'階')+std::to_wstring(i); a+=name;b+=name;utf8.Dir(a);utf8.Dir(b); }
    Put(a+L"\\file","UTF8");
    auto copied=utf8.Run(utf8.Scan());
    Check(a.size()<260 && WideCharToMultiByte(CP_UTF8,0,a.c_str(),-1,nullptr,0,nullptr,nullptr)>260,"UTF8 long byte path fixture");
    Check(copied.items.size()==1 && copied.items[0].status==CopyStatus::Copied && Read(b+L"\\file")=="UTF8","short UTF16 long UTF8 components");
}
void AfterPublication() {
    for(int mode=0;mode<3;++mode) {
        Fixture f; Put(f.left+L"\\a","PUBLISHED"); Put(f.left+L"\\a:note","ADS"); Put(f.left+L"\\b","NEXT");
        auto plan=f.Scan(); bool injected=false;
        SetCopyHook([&](CopyPhase phase,const std::wstring& target) {
            if(phase!=CopyPhase::Published || injected) return;
            injected=true;
            if(mode==0) Acl(target,L"D:P(D;;0x100;;;WD)(A;;FA;;;OW)"); // deny FILE_WRITE_ATTRIBUTES
            else if(mode==1) Acl(target,L"D:P(A;;FA;;;OW)(A;;FR;;;WD)");
            else f.cancel=true;
        });
        auto report=f.Run(plan); SetCopyHook({});
        Check(injected && report.items.size()==2 && report.items[0].published,"observed actual publication");
        if(mode<2) {
            Check(report.items[0].status==CopyStatus::Failed && report.items[0].error==DWORD(mode==0?ERROR_ACCESS_DENIED:ERROR_INVALID_DATA),"post-publication error is reported");
            Check(!report.cancelled && f.progress.completed==0,"failed final check is not successful completion");
            Acl(f.right+L"\\a",L"D:P(A;;FA;;;OW)");
        } else {
            Check(report.items[0].status==CopyStatus::Copied && f.progress.completed==1,"Stop after publication preserves verified completion");
            Check(report.cancelled && report.items[1].status==CopyStatus::Cancelled,"Stop after publication prevents next copy");
        }
        Check(Read(f.right+L"\\a")=="PUBLISHED" && Read(f.right+L"\\a:note")=="ADS","published data and ADS retained after error or Stop");
        Check(Read(f.left+L"\\a")=="PUBLISHED","source preserved");
        Check(report.items[0].remainingStage.empty() && GetFileAttributesW(ApiPath(f.right+L"\\b").c_str())==INVALID_FILE_ATTRIBUTES,"no staging report or subsequent target");
        if(mode<2) Check(report.items[1].status==CopyStatus::NotStarted,"post-publication failure stops batch");
        f.NoStaging(f.right);
        HANDLE h=CreateFileW(ApiPath(f.right+L"\\a").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
        Check(h!=INVALID_HANDLE_VALUE,"published file handles released"); if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    Fixture f; Put(f.left+L"\\a",std::string(1024*1024,'V')); auto plan=f.Scan(); bool read=false;
    SetCopyHook([&](CopyPhase phase,const std::wstring&) {
        if(phase==CopyPhase::Verifying && f.progress.verified>0) { read=true; f.cancel=true; }
    });
    auto report=f.Run(plan); SetCopyHook({});
    Check(read && report.cancelled && report.items[0].status==CopyStatus::Cancelled,"Stop during actual Verify reads");
    Check(!report.items[0].published && f.progress.verified>0 && f.progress.verified<2*1024*1024,"Verify interrupted before reading entire file");
    Check(GetFileAttributesW(ApiPath(f.right+L"\\a").c_str())==INVALID_FILE_ATTRIBUTES,"partial Verify never publishes"); f.NoStaging(f.right);
}
}
int main() {
    try { Success(); Scope(); Boundaries(); Paths(); AfterPublication(); std::cout<<checks<<" copy assertions passed\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"FAILED: "<<e.what()<<"\n"; return 1; }
}
