// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "../copy.h"
#include "../recovery.h"
#include "../overwrite.h"
#include "../delete.h"
#include "copy_space_faults.h"
#include <sddl.h>
#include <winioctl.h>
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
    // Same ordinary junction fixture as engine_tests.cpp; no privilege changes.
    void Junction(const std::wstring& path,const std::wstring& target) {
        Dir(path); std::wstring substitute=L"\\??\\"+target;
        std::vector<unsigned char> buffer(16+(substitute.size()+1+target.size()+1)*sizeof(wchar_t));
        struct Header { DWORD tag; WORD length,reserved,subOffset,subLength,printOffset,printLength; };
        auto* header=reinterpret_cast<Header*>(buffer.data()); header->tag=IO_REPARSE_TAG_MOUNT_POINT;
        header->length=WORD(buffer.size()-8); header->subLength=WORD(substitute.size()*sizeof(wchar_t));
        header->printOffset=WORD((substitute.size()+1)*sizeof(wchar_t)); header->printLength=WORD(target.size()*sizeof(wchar_t));
        memcpy(buffer.data()+16,substitute.c_str(),(substitute.size()+1)*sizeof(wchar_t));
        memcpy(buffer.data()+16+header->printOffset,target.c_str(),(target.size()+1)*sizeof(wchar_t));
        HANDLE h=CreateFileW(ApiPath(path).c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        Check(h!=INVALID_HANDLE_VALUE,"recovery junction handle"); DWORD returned=0;
        bool ok=DeviceIoControl(h,FSCTL_SET_REPARSE_POINT,buffer.data(),DWORD(buffer.size()),nullptr,0,&returned,nullptr)!=FALSE;
        CloseHandle(h); Check(ok,"recovery junction fixture");
    }
    Result Scan(bool asymmetric=false) { Options o; o.asymmetric=asymmetric; return Compare(left,right,o,cancel,checking); }
    CopyReport Run(Result r,const std::vector<size_t>& selected={}) {
        std::vector<size_t> rows=selected; if(rows.empty()) for(size_t i=0;i<r.rows.size();++i) rows.push_back(i);
        return CopyNewFiles(std::move(r),rows,cancel,checking,progress);
    }
    void NoStaging(const std::wstring& dir) {
        WIN32_FIND_DATAW d{}; HANDLE f=FindFirstFileW((ApiPath(dir)+L"\\.dirsync-*").c_str(),&d);
        bool absent=f==INVALID_HANDLE_VALUE; if(!absent) FindClose(f); Check(absent,"no unreported staging left");
    }
    ~Fixture() { SetCopyHook({}); SetRecoveryLogHook({}); dirsync_test::SetSpaceFault(dirsync_test::SpaceFault::None,0,1); Cleanup(root); RemoveDirectoryW(ApiPath(root).c_str()); }
};
void Acl(const std::wstring& path,const wchar_t* text,bool protect=true) {
    PSECURITY_DESCRIPTOR sd=nullptr; Check(ConvertStringSecurityDescriptorToSecurityDescriptorW(text,SDDL_REVISION_1,&sd,nullptr)!=FALSE,"SDDL");
    BOOL ok=SetFileSecurityW(ApiPath(path).c_str(),DACL_SECURITY_INFORMATION|
        (protect?PROTECTED_DACL_SECURITY_INFORMATION:UNPROTECTED_DACL_SECURITY_INFORMATION),sd);
    if(!ok) std::wcerr<<L"SetFileSecurity fixture "<<path<<L": "<<GetLastError()<<L"\n";
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
void ModifyOnlyDestination() {
    Fixture f; Put(f.left+L"\\a","MODIFY"); Put(f.left+L"\\a:note","ADS");
    // Root remains controllable by the fixture; new files inherit Modify only.
    // OWNER RIGHTS suppresses the owner's implicit WRITE_DAC grant as well.
    Acl(f.right,L"D:P(A;;FA;;;WD)(A;OICIIO;0x1301bf;;;WD)(A;OICIIO;0x1301bf;;;S-1-3-4)");
    bool denied=false;
    SetCopyHook([&](CopyPhase phase,const std::wstring& stage) {
        if(phase!=CopyPhase::Copying || !f.progress.copied || denied) return;
        HANDLE h=CreateFileW(ApiPath(stage).c_str(),WRITE_DAC,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
            nullptr,OPEN_EXISTING,0,nullptr);
        denied=h==INVALID_HANDLE_VALUE && GetLastError()==ERROR_ACCESS_DENIED;
        if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    });
    auto report=f.Run(f.Scan()); SetCopyHook({});
    Check(denied,"fixture staging has Modify but not WRITE_DAC");
    Check(report.items.size()==1 && report.items[0].status==CopyStatus::Copied,"Modify-only destination needs no redundant DACL write");
    Check(Read(f.right+L"\\a")=="MODIFY" && Read(f.right+L"\\a:note")=="ADS","Modify-only main data and ADS verified");
    Put(f.right+L"\\reference","ACL");
    Check(Dacl(f.right+L"\\a")==Dacl(f.right+L"\\reference"),"Modify-only target retains inherited permissions");
    f.NoStaging(f.right);
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
Result RecoveryPlan(Fixture& f) {
    auto plan=f.Scan(true);
    for(auto& row:plan.rows) if(row.hasLeft && row.hasRight && !row.left.directory && !row.right.directory)
        row.action=Action::CopyRight;
    return plan;
}
RecoveryReport Save(Fixture& f,const Result& plan,const std::wstring& root,const std::vector<size_t>& selected={}) {
    auto rows=selected; if(rows.empty()) for(size_t i=0;i<plan.rows.size();++i) rows.push_back(i);
    return SaveOriginals(plan,rows,root,f.cancel,f.checking,f.progress);
}
void RecoverySuccess() {
    Fixture f; auto root=f.root+L"\\obnova-漢字"; f.Dir(root);
    Put(f.left+L"\\a","NEW"); Put(f.right+L"\\a","ORIGINAL"); Put(f.right+L"\\a:note","OLD ADS");
    Put(f.right+L"\\a:漢字",""); Put(f.left+L"\\b","LEFT OLD"); Put(f.right+L"\\b","NEW RIGHT");
    Put(f.left+L"\\new","NOT COPIED"); Put(f.right+L"\\delete","NOT DELETED");
    Acl(f.right+L"\\a",L"D:P(A;;FA;;;OW)(A;;FR;;;WD)"); auto originalAcl=Dacl(f.right+L"\\a");
    Check(SetFileAttributesW(ApiPath(f.right+L"\\a").c_str(),FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)!=FALSE,"original recovery flags");
    auto plan=RecoveryPlan(f); for(auto& row:plan.rows) if(row.path==L"b") row.action=Action::CopyLeft;
    auto report=Save(f,plan,root);
    if(report.error) std::cerr<<"recovery error "<<report.error<<"\n";
    Check(report.complete && !report.error && report.items.size()==2 && report.omitted==2,"only existing file copy proposals saved");
    Check(report.items[0].right && !report.items[1].right,"save target side in both directions");
    for(const auto& item:report.items) Check(item.logged && item.copy.status==CopyStatus::Copied && item.copy.published,"saved outcomes durably logged");
    Check(Read(report.items[0].saved)=="ORIGINAL" && Read(report.items[0].saved+L":note")=="OLD ADS" && Read(report.items[0].saved+L":漢字").empty(),"recovery exact main and full ADS set");
    Check(Read(report.items[1].saved)=="LEFT OLD","reverse saves left original, not proposed source");
    Check(Read(f.left+L"\\a")=="NEW" && Read(f.right+L"\\a")=="ORIGINAL" && Read(f.left+L"\\b")=="LEFT OLD" && Read(f.right+L"\\b")=="NEW RIGHT","all originals unchanged");
    Check(Read(f.right+L"\\delete")=="NOT DELETED" && GetFileAttributesW(ApiPath(f.right+L"\\new").c_str())==INVALID_FILE_ATTRIBUTES,"delete and new-file proposals not executed");
    Check(Dacl(f.right+L"\\a")==originalAcl,"original permissions unchanged");
    Put(root+L"\\reference","ACL"); Check(Dacl(report.items[0].saved)==Dacl(root+L"\\reference"),"recovery inherits recovery permissions");
    Check((GetFileAttributesW(ApiPath(report.items[0].saved).c_str()) & (FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))==
        (FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM),"recovery retains selected flags");
    auto log=Read(report.journal);
    Check(log.find("\"schema\":1")!=std::string::npos && log.find("\"operation\":\"save_originals_only\"")!=std::string::npos &&
        log.find("\"status\":\"complete\"")!=std::string::npos && log.back()=='\n',"versioned JSONL run and complete record");
    Check(log.find("\"original\":")!=std::string::npos && log.find("\"saved\":")!=std::string::npos && log.find("\"mtime\":")!=std::string::npos,"journal includes restoration mapping and metadata");
    auto again=Save(f,plan,root);
    Check(again.complete && again.runDirectory!=report.runDirectory && Read(report.items[0].saved)=="ORIGINAL","repeated run never collides with old recovery");
    auto restored=f.root+L"\\restored";
    Check(CopyFileW(ApiPath(report.items[0].saved).c_str(),ApiPath(restored).c_str(),TRUE)!=FALSE &&
        Read(restored)=="ORIGINAL" && Read(restored+L":note")=="OLD ADS","ordinary manual restore to new name preserves data and ADS");
    // Recovery is independent even when the original has other hardlink names.
    Fixture link; auto recovery=link.root+L"\\recovery"; link.Dir(recovery);
    Put(link.left+L"\\a","NEW"); Put(link.right+L"\\a","OLD");
    Check(CreateHardLinkW(ApiPath(link.root+L"\\alias").c_str(),ApiPath(link.right+L"\\a").c_str(),nullptr)!=FALSE,"recovery hardlink fixture");
    auto independent=Save(link,RecoveryPlan(link),recovery); Check(independent.complete,"hardlink original saved");
    Put(link.root+L"\\alias","MOD",false); Check(Read(independent.items[0].saved)=="OLD","recovery copy independent of original aliases");
}
void RecoveryFailures() {
    for(int mode=0;mode<14;++mode) {
        Fixture f; auto root=f.root+L"\\recovery"; f.Dir(root);
        Put(f.left+L"\\a","NEW"); Put(f.right+L"\\a",std::string(2*1024*1024,'O')); Put(f.right+L"\\a:note","OLD ADS");
        Put(f.left+L"\\b","NEW B"); Put(f.right+L"\\b","OLD B"); auto plan=RecoveryPlan(f);
        bool injected=false;
        if(mode==0) root=f.right; // inside compared tree
        if(mode==1) root=f.root; // ancestor of compared trees
        if(mode==2) root+=L"\\missing";
        if(mode==3) Put(f.left+L"\\a","CHANGED",false);
        if(mode==4) Acl(root,L"D:P(D;OICI;0x2;;;WD)(A;OICI;FA;;;OW)");
        HANDLE lock=INVALID_HANDLE_VALUE;
        if(mode==10) {
            lock=CreateFileW(ApiPath(f.right+L"\\a").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
            Check(lock!=INVALID_HANDLE_VALUE,"exclusive original lock");
        }
        if(mode==11) { auto alias=f.root+L"\\junction"; f.Junction(alias,root); root=alias; }
        if(mode==5 || mode==6 || mode==7 || mode==12) SetCopyHook([&](CopyPhase phase,const std::wstring& stage) {
            if(injected) return;
            if((mode==5 && phase==CopyPhase::Copying && f.progress.copied>0) ||
                (mode==6 && phase==CopyPhase::Verifying && f.progress.verified>0)) { injected=true; f.cancel=true; }
            if(mode==7 && phase==CopyPhase::Verifying) { injected=true; Put(stage,"CORRUPT",false); }
            if(mode==12 && phase==CopyPhase::Published) { injected=true; Acl(stage,L"D:P(D;;0x100;;;WD)(A;;FA;;;OW)"); }
        });
        if(mode==8) dirsync_test::SetSpaceFault(dirsync_test::SpaceFault::MainData,ERROR_DISK_FULL,1);
        if(mode==9) SetRecoveryLogHook([&](const std::wstring& line)->DWORD {
            if(line.find(L"\"event\":\"result\"")!=std::wstring::npos) { injected=true; return ERROR_DISK_FULL; } return 0;
        });
        if(mode==13) SetRecoveryLogHook([](const std::wstring& line)->DWORD {
            return line.find(L"\"event\":\"planned\"")==std::wstring::npos?0:ERROR_WRITE_FAULT;
        });
        auto report=Save(f,plan,root); SetCopyHook({}); SetRecoveryLogHook({});
        if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock);
        if(mode==4) Acl(root,L"D:P(A;OICI;FA;;;OW)");
        if(mode==12 && report.items[0].copy.published) Acl(report.items[0].saved,L"D:P(A;OICI;FA;;;OW)");
        Check(!report.complete && (report.error || report.stale),"failed recovery never complete");
        Check(Read(f.right+L"\\a")==std::string(2*1024*1024,'O') && Read(f.right+L"\\a:note")=="OLD ADS" && Read(f.right+L"\\b")=="OLD B","recovery failure leaves original data and ADS untouched");
        Check(report.items.size()==2 && report.items[1].copy.status==CopyStatus::NotStarted,"failure prevents next recovery copy");
        if(mode<4) Check(report.journal.empty(),"invalid root or stale snapshot creates no log");
        if(mode==3) Check(report.stale,"source change invalidates complete proposal snapshot");
        if(mode==5 || mode==6) Check(injected && report.cancelled && report.items[0].copy.status==CopyStatus::Cancelled,"Stop interrupts real transfer or Verify");
        if(mode==7) Check(injected && report.items[0].copy.status==CopyStatus::Failed && !report.items[0].copy.published,"corrupt staging never published");
        if(mode==8) Check(dirsync_test::SpaceFaultReached() && report.error==ERROR_DISK_FULL,"recovery partial disk-full failure preserved");
        if(mode==9) Check(injected && report.journalError==ERROR_DISK_FULL && report.items[0].copy.status==CopyStatus::Copied && !report.items[0].logged &&
            Read(report.items[0].saved)==std::string(2*1024*1024,'O') && Read(report.journal).find("\"event\":\"end\"")==std::string::npos,"journal failure preserves saved file but rejects completed recovery");
        if(mode==10) Check(report.stale || report.error==ERROR_SHARING_VIOLATION,"locked original refused");
        if(mode==11) Check(report.journal.empty(),"reparse recovery root rejected before writes");
        if(mode==12) Check(injected && report.items[0].copy.published && report.items[0].copy.status==CopyStatus::Failed &&
            report.items[0].logged && report.error==ERROR_ACCESS_DENIED && Read(report.items[0].saved)==std::string(2*1024*1024,'O') &&
            Read(report.journal).find("\"status\":\"failed\"")!=std::string::npos,"post-publication metadata failure retains recovery and logs failure");
        if(mode==13) Check(report.journalError==ERROR_WRITE_FAULT && !report.items[0].copy.published &&
            report.items[0].copy.status==CopyStatus::NotStarted,"planned log must be written before any recovery copy");
        if(mode>=5 && mode<=8) Check(report.items[0].logged && report.items[1].logged && !report.journalError &&
            Read(report.journal).find("\"status\":\"complete\"")==std::string::npos,"ordinary failure/cancellation logged including not-started items");
        if(mode>=5 && mode<=8) f.NoStaging(report.items[0].saved.substr(0,report.items[0].saved.find_last_of(L'\\')));
    }
    Fixture f; auto root=f.root+L"\\recovery"; f.Dir(root);
    Put(f.left+L"\\a","NEW"); Put(f.right+L"\\a","ORIGINAL A"); Put(f.left+L"\\b","NEW"); Put(f.right+L"\\b","ORIGINAL B");
    SetCopyHook([&](CopyPhase phase,const std::wstring&) { if(phase==CopyPhase::Published) f.cancel=true; });
    auto report=Save(f,RecoveryPlan(f),root); SetCopyHook({});
    Check(report.cancelled && report.items[0].copy.status==CopyStatus::Copied && report.items[0].logged &&
        report.items[1].copy.status==CopyStatus::Cancelled,"Stop after publication retains completed logged original");
    Check(Read(report.items[0].saved)=="ORIGINAL A" && Read(f.right+L"\\a")=="ORIGINAL A" && Read(f.right+L"\\b")=="ORIGINAL B","Stop leaves originals and prior saved file intact");
}
void RecoveryPaths() {
    for(bool longUtf16:{false,true}) {
        Fixture f; auto a=f.left,b=f.right,root=f.root;
        for(int i=0;i<(longUtf16?5:2);++i) {
            auto part=L"\\"+std::wstring(35,L'階')+std::to_wstring(i); a+=part; b+=part; root+=part; f.Dir(a); f.Dir(b); f.Dir(root);
        }
        auto name=longUtf16?L"\\"+std::wstring(170,L'x')+L"-漢字-😀.txt":L"\\file.txt";
        Put(a+name,"NEW"); Put(b+name,"OLD"); Put(b+name+L":žluťoučký","ADS");
        auto plan=RecoveryPlan(f); auto report=Save(f,plan,root);
        if(report.error) std::cerr<<"recovery path error "<<report.error<<"\n";
        Check(report.complete && report.items.size()==1,"recovery long and Unicode paths");
        Check(Read(report.items[0].saved)=="OLD" && Read(report.items[0].saved+L":žluťoučký")=="ADS" && Read(b+name)=="OLD","long recovery exact original and ADS");
        auto bytes=Read(report.journal);
        Check(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),int(bytes.size()),nullptr,0)>0,"manifest valid UTF-8");
        Check(WideCharToMultiByte(CP_UTF8,0,root.c_str(),-1,nullptr,0,nullptr,nullptr)>260,"recovery path exceeds MAX_PATH in UTF-8");
        if(longUtf16) Check(report.items[0].saved.size()>260,"saved full UTF-16 path exceeds MAX_PATH");
    }
    Fixture f; auto root=f.root+L"\\recovery"; f.Dir(root);
    for(const auto& folder:{L"one",L"two"}) {
        auto a=f.left+L"\\"+folder,b=f.right+L"\\"+folder; f.Dir(a); f.Dir(b);
        Put(a+L"\\漢字.txt","NEW"); Put(b+L"\\漢字.txt",folder==std::wstring(L"one")?"": "OLD TWO");
    }
    auto plan=RecoveryPlan(f); std::vector<size_t> rows;
    for(size_t i=0;i<plan.rows.size();++i) if(IsOriginalToSave(plan.rows[i])) rows.push_back(i);
    Check(rows.size()==2,"nested originals fixture");
    auto report=Save(f,plan,root,{rows[0],rows[0],size_t(-1)});
    Check(report.complete && report.items.size()==1 && Read(report.items[0].saved).empty(),"visible selection deduplicated; empty original retained");
    auto both=Save(f,plan,root,rows);
    Check(both.complete && both.items.size()==2 && both.items[0].saved!=both.items[1].saved && Read(both.items[1].saved)=="OLD TWO","same basename in separate directories maps without collisions");
}
Result OverwritePlan(Fixture& f) {
    auto plan=f.Scan();
    for(auto& row:plan.rows) if(row.hasLeft && row.hasRight && !row.left.directory && !row.right.directory) row.action=Action::CopyRight;
    return plan;
}
OverwriteReport Replace(Fixture& f,const Result& plan,bool keep,const std::wstring& root,const std::vector<size_t>& selected={}) {
    auto rows=selected; if(rows.empty()) for(size_t i=0;i<plan.rows.size();++i) rows.push_back(i);
    return OverwriteFiles(plan,rows,keep,root,f.cancel,f.checking,f.progress);
}
void OverwriteSuccess() {
    for(bool keep:{false,true}) for(bool protectedAcl:{false,true}) {
        Fixture f; auto recovery=f.root+L"\\recovery"; f.Dir(recovery);
        Put(f.left+L"\\a","NEW MAIN"); Put(f.left+L"\\a:new-only","NEW ADS"); Put(f.left+L"\\a:漢字","NEW SHARED");
        Put(f.right+L"\\a","OLD MAIN"); Put(f.right+L"\\a:old-only","OLD ADS"); Put(f.right+L"\\a:漢字","OLD SHARED");
        Acl(f.right+L"\\a",protectedAcl?L"D:P(A;;FA;;;OW)(A;;FR;;;BU)":L"D:(A;;FA;;;OW)(A;;FR;;;BU)",protectedAcl);
        Acl(f.left+L"\\a",L"D:P(A;;FA;;;OW)(A;;FR;;;WD)");
        auto targetAcl=Dacl(f.right+L"\\a"),sourceAcl=Dacl(f.left+L"\\a"); Check(targetAcl!=sourceAcl,"distinct source and target policies");
        Check((targetAcl.rfind(L"D:P",0)==0)==protectedAcl,"fixture actually tests protected and unprotected DACLs");
        Check(SetFileAttributesW(ApiPath(f.right+L"\\a").c_str(),FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)!=FALSE,"old hidden and system flags");
        Check(CreateHardLinkW(ApiPath(f.root+L"\\alias").c_str(),ApiPath(f.right+L"\\a").c_str(),nullptr)!=FALSE,"old target alias");
        Check(SetFileAttributesW(ApiPath(f.left+L"\\a").c_str(),FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)!=FALSE,"new selected flags");
        Put(f.left+L"\\b","OLD LEFT"); Put(f.right+L"\\b","NEW RIGHT");
        Acl(f.left+L"\\b",L"D:P(A;;FA;;;OW)(A;;FR;;;BU)"); auto reverseAcl=Dacl(f.left+L"\\b");
        Acl(f.right+L"\\b",L"D:P(A;;FA;;;OW)(A;;FR;;;WD)");
        Put(f.left+L"\\new","OMIT NEW"); Put(f.right+L"\\extra","OMIT EXTRA");
        auto plan=OverwritePlan(f); uint64_t expectedTime=0;
        for(auto& row:plan.rows) { if(row.path==L"b") row.action=Action::CopyLeft; if(row.path==L"a") expectedTime=row.left.time; if(row.path==L"extra") row.action=Action::DeleteRight; }
        auto report=Replace(f,plan,keep,keep?recovery:L"invalid-unused-recovery-path");
        if(report.error) std::cerr<<"overwrite success error "<<report.error<<"\n";
        Check(report.complete && !report.error && report.items.size()==2 && report.omitted==2,"overwrites only visible existing file proposals");
        for(const auto& item:report.items) Check(item.status==CopyStatus::Copied && item.published,"overwrite completes after publication");
        Check(Read(f.right+L"\\a")=="NEW MAIN" && Read(f.right+L"\\a:new-only")=="NEW ADS" && Read(f.right+L"\\a:漢字")=="NEW SHARED","new target has exact new data and ADS");
        Check(GetFileAttributesW(ApiPath(f.right+L"\\a:old-only").c_str())==INVALID_FILE_ATTRIBUTES,"old-only ADS is not merged into replacement");
        Check(Read(f.left+L"\\b")=="NEW RIGHT" && Read(f.left+L"\\a")=="NEW MAIN","reverse overwrite and source preserved");
        Check(Dacl(f.right+L"\\a")==targetAcl && Dacl(f.right+L"\\a")!=sourceAcl,"original protected or inherited target DACL retained");
        Check(Dacl(f.left+L"\\b")==reverseAcl && Dacl(f.left+L"\\b")!=Dacl(f.right+L"\\b"),"reverse overwrite keeps left target permissions");
        Check(Read(f.root+L"\\alias")=="OLD MAIN" && Read(f.root+L"\\alias:old-only")=="OLD ADS" && Read(f.root+L"\\alias:漢字")=="OLD SHARED","other old hardlink data and ADS unaffected");
        Check(Dacl(f.root+L"\\alias")==targetAcl && GetFileAttributesW(ApiPath(f.root+L"\\alias").c_str())==(FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM),"other hardlink permissions and attributes unchanged");
        HANDLE h=CreateFileW(ApiPath(f.right+L"\\a").c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        FILETIME time{}; Check(h!=INVALID_HANDLE_VALUE && GetFileTime(h,nullptr,nullptr,&time),"replacement time handle"); CloseHandle(h);
        Check(((uint64_t(time.dwHighDateTime)<<32)|time.dwLowDateTime)==expectedTime,"replacement mtime matches source");
        Check((GetFileAttributesW(ApiPath(f.right+L"\\a").c_str())&(FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))==
            (FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM),"replacement selected source flags");
        Check(Read(f.right+L"\\extra")=="OMIT EXTRA" && GetFileAttributesW(ApiPath(f.right+L"\\new").c_str())==INVALID_FILE_ATTRIBUTES,"no delete or new-file action executed");
        Check(f.progress.completed==(keep?4u:2u),"progress counts recovery and replacement transfers");
        if(keep) {
            Check(report.recovery.complete && report.recovery.items.size()==2 && !report.journal.empty(),"verified recovery completes before replacements");
            Check(Read(report.recovery.items[0].saved)=="OLD MAIN" && Read(report.recovery.items[0].saved+L":old-only")=="OLD ADS" &&
                Read(report.recovery.items[0].saved+L":漢字")=="OLD SHARED" && Read(report.recovery.items[1].saved)=="OLD LEFT","recovery preserves only original target content");
            auto log=Read(report.journal); Check(log.find("\"operation\":\"overwrite\"")!=std::string::npos && log.find("\"status\":\"replaced\"")!=std::string::npos &&
                log.find("\"status\":\"complete\"")!=std::string::npos,"overwrite results and completion separately logged");
            auto restored=f.root+L"\\restored-old";
            Check(CopyFileW(ApiPath(report.recovery.items[0].saved).c_str(),ApiPath(restored).c_str(),TRUE)!=FALSE &&
                Read(restored)=="OLD MAIN" && Read(restored+L":old-only")=="OLD ADS","manual restore original to unused name");
        } else Check(report.recovery.items.empty() && report.recovery.journal.empty() && report.journal.empty(),"disabled recovery needs no folder or persistent journal");
        f.NoStaging(f.left); f.NoStaging(f.right);
    }
}
void OverwriteFailures() {
    for(bool keep:{false,true}) for(int mode=0;mode<16;++mode) {
        if(!keep && (mode==3 || mode==12 || mode==13)) continue;
        Fixture f; auto recovery=f.root+L"\\recovery"; f.Dir(recovery);
        Put(f.left+L"\\a",std::string(2*1024*1024,'N')); Put(f.left+L"\\a:new","NEW ADS");
        Put(f.right+L"\\a","OLD"); Put(f.right+L"\\a:old","OLD ADS"); Put(f.left+L"\\b","NEW B"); Put(f.right+L"\\b","OLD B");
        Acl(f.right+L"\\a",L"D:P(A;;FA;;;OW)(A;;FR;;;BU)");
        if(mode==1) Check(SetFileAttributesW(ApiPath(f.right+L"\\a").c_str(),FILE_ATTRIBUTE_READONLY)!=FALSE,"read-only target fixture");
        const DWORD originalAttributes=GetFileAttributesW(ApiPath(f.right+L"\\a").c_str());
        auto plan=OverwritePlan(f); HANDLE lock=INVALID_HANDLE_VALUE; bool injected=false,published=false;
        if(mode==0) Put(f.right+L"\\a","CHANGED",false);
        if(mode==2) { lock=CreateFileW(ApiPath(f.right+L"\\a").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr); Check(lock!=INVALID_HANDLE_VALUE,"exclusive overwrite target lock"); }
        if(mode==7) dirsync_test::SetSpaceFault(dirsync_test::SpaceFault::Publish,ERROR_DISK_FULL,keep?3:1);
        if(mode==15) dirsync_test::SetSpaceFault(dirsync_test::SpaceFault::MainData,ERROR_DISK_FULL,keep?3:1);
        SetCopyHook([&](CopyPhase phase,const std::wstring& path) {
            bool replacement=path.rfind(f.right+L"\\",0)==0;
            if(phase==CopyPhase::Published && replacement) published=true;
            if(injected) return;
            if(mode==3 && !replacement && phase==CopyPhase::Copying && f.progress.copied>0) { injected=true; f.cancel=true; }
            if(mode==4 && replacement && phase==CopyPhase::Copying && f.progress.copied>(keep?8u:0u)) { injected=true; f.cancel=true; }
            if(mode==5 && replacement && phase==CopyPhase::Verifying && f.progress.verified>(keep?32u:0u)) { injected=true; f.cancel=true; }
            if(mode==6 && replacement && phase==CopyPhase::Verifying) { injected=true; Put(path,"BAD",false); }
            if(mode==8 && replacement && phase==CopyPhase::Publishing) { injected=true; Put(f.right+L"\\a","CHANGED",false); }
            if(mode==9 && replacement && phase==CopyPhase::Publishing) { injected=true; Acl(f.right+L"\\a",L"D:P(A;;FA;;;OW)(A;;FR;;;WD)"); }
            if(mode==10 && replacement && phase==CopyPhase::Published) { injected=true; Acl(path,L"D:P(D;;0x100;;;WD)(A;;FA;;;OW)"); }
            if(mode==11 && replacement && phase==CopyPhase::Published) { injected=true; f.cancel=true; }
            if(mode==14 && replacement && phase==CopyPhase::Preparing) { injected=true; Acl(path,L"D:P(D;;0x40000;;;WD)(A;;FA;;;OW)"); }
        });
        if(mode==12 || mode==13) SetRecoveryLogHook([&](const std::wstring& line)->DWORD {
            if((mode==12 && line.find(L"\"event\":\"planned\"")!=std::wstring::npos && line.find(L"\"target\":")!=std::wstring::npos) ||
               (mode==13 && published && line.find(L"\"event\":\"result\"")!=std::wstring::npos)) { injected=true; return ERROR_WRITE_FAULT; } return 0;
        });
        auto report=Replace(f,plan,keep,recovery); SetCopyHook({}); SetRecoveryLogHook({});
        if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock);
        Check(!report.complete && (report.error || report.stale || report.cancelled),"overwrite error never reported complete");
        Check(Read(f.right+L"\\b")=="OLD B" && !report.items[1].published,"later file not replaced after error or Stop");
        const bool changed=mode==0 || mode==8; const bool replaced=mode==10 || mode==11 || mode==13;
        if(mode==10) Acl(f.right+L"\\a",L"D:P(A;;FA;;;OW)");
        Check(Read(f.right+L"\\a")== (changed?"CHANGED":replaced?std::string(2*1024*1024,'N'):"OLD"),"pre-publication errors retain original; post-publication outcomes explicit");
        Check(Read(f.left+L"\\a")==std::string(2*1024*1024,'N'),"overwrite errors never change source");
        if(!replaced) Check(Read(f.right+L"\\a:old")=="OLD ADS","old ADS survive failed replacement");
        if(mode==0) Check(report.stale,"changed target invalidates preview");
        if(mode==2) Check(report.stale || report.error==ERROR_SHARING_VIOLATION || report.error==ERROR_ACCESS_DENIED,"exclusively locked target refuses overwrite");
        if(mode==1) Check(report.error==ERROR_ACCESS_DENIED && report.recovery.items.empty() &&
            GetFileAttributesW(ApiPath(f.right+L"\\a").c_str())==originalAttributes,"read-only target refused without clearing attributes or backing up");
        if(mode==3) Check(injected && report.recovery.cancelled && report.items[0].status==CopyStatus::NotStarted,"backup Stop prevents all overwrites");
        if(mode==4 || mode==5) Check(injected && report.cancelled && !report.items[0].published,"Stop during replacement copy or Verify prevents publication");
        if(mode==7) Check(dirsync_test::SpaceFaultReached() && report.error==ERROR_DISK_FULL,"publish error preserved without removing original");
        if(mode==15) Check(dirsync_test::SpaceFaultReached() && report.error==ERROR_DISK_FULL,"partial overwrite transfer disk full preserves original");
        if(mode==8 || mode==9) Check(injected && report.error==ERROR_FILE_INVALID,"observed target data or DACL change prevents replacement");
        if(mode==10) Check(injected && report.items[0].published && report.items[0].status==CopyStatus::Failed,"post-publication failure reports replacement left in place");
        if(mode==11) Check(injected && report.items[0].status==CopyStatus::Copied && report.items[1].status==CopyStatus::Cancelled,"late Stop preserves completed replacement and prevents next");
        if(mode==12 || mode==13) Check(injected && report.journalError==ERROR_WRITE_FAULT &&
            Read(report.journal).find("\"event\":\"end\"")==std::string::npos,"execution log write failure leaves incomplete run and stops batch");
        if(mode==14) Check(injected && report.error==ERROR_ACCESS_DENIED && !report.items[0].published,"required target DACL failure prevents publication");
        if(keep && mode>=4) Check(report.recovery.complete && Read(report.recovery.items[0].saved)=="OLD" && Read(report.recovery.items[0].saved+L":old")=="OLD ADS","original recovery remains usable after overwrite failure");
        f.NoStaging(f.right);
    }
    Fixture f; auto root=f.root+L"\\missing-recovery";
    Put(f.left+L"\\a","NEW"); Put(f.right+L"\\a","OLD"); auto report=Replace(f,OverwritePlan(f),true,root);
    Check(!report.complete && report.error && Read(f.right+L"\\a")=="OLD" && !report.items[0].published,"unavailable recovery folder prevents overwrite");
    Fixture backup; auto recovery=backup.root+L"\\recovery"; backup.Dir(recovery);
    Put(backup.left+L"\\a","NEW A"); Put(backup.right+L"\\a","OLD A"); Put(backup.left+L"\\b","NEW B"); Put(backup.right+L"\\b",std::string(2*1024*1024,'O'));
    dirsync_test::SetSpaceFault(dirsync_test::SpaceFault::MainData,ERROR_DISK_FULL,2);
    auto incomplete=Replace(backup,OverwritePlan(backup),true,recovery);
    Check(dirsync_test::SpaceFaultReached() && !incomplete.complete && incomplete.error==ERROR_DISK_FULL,"second original cannot be fully saved");
    Check(incomplete.items[0].status==CopyStatus::NotStarted && incomplete.items[1].status==CopyStatus::NotStarted &&
        Read(backup.right+L"\\a")=="OLD A" && Read(backup.right+L"\\b")==std::string(2*1024*1024,'O'),"incomplete recovery set prevents every overwrite");
    Check(!incomplete.recovery.complete && incomplete.recovery.items[0].logged && Read(incomplete.recovery.items[0].saved)=="OLD A","completed earlier recovery retained without replacing its original");
}
void OverwritePaths() {
    for(bool keep:{false,true}) {
        Fixture f; auto a=f.left,b=f.right,recovery=f.root+L"\\recovery-漢字"; f.Dir(recovery);
        for(int i=0;i<5;++i) { auto part=L"\\漢字-"+std::wstring(40,L'階')+std::to_wstring(i); a+=part; b+=part; f.Dir(a); f.Dir(b); }
        auto name=L"\\"+std::wstring(180,L'x')+L"-😀.txt";
        Put(a+name,"NEW"); Put(a+name+L":nový","NEW ADS"); Put(b+name,""); Put(b+name+L":starý","OLD ADS");
        auto report=Replace(f,OverwritePlan(f),keep,recovery);
        Check(report.complete && report.items.size()==1 && report.items[0].target.size()>260,"overwrite long Unicode paths and basename");
        Check(Read(b+name)=="NEW" && Read(b+name+L":nový")=="NEW ADS","long replacement data and ADS");
        if(keep) Check(Read(report.recovery.items[0].saved).empty() && Read(report.recovery.items[0].saved+L":starý")=="OLD ADS","long empty original and old ADS retained");
    }
    Fixture f; auto root=f.root+L"\\recovery"; f.Dir(root);
    Put(f.left+L"\\a","NEW A"); Put(f.right+L"\\a","OLD A"); Put(f.left+L"\\b","NEW B"); Put(f.right+L"\\b","OLD B");
    auto selected=Replace(f,OverwritePlan(f),true,root,{0,0,size_t(-1)});
    Check(selected.complete && selected.items.size()==1 && Read(f.right+L"\\a")=="NEW A" && Read(f.right+L"\\b")=="OLD B","only frozen visible rows replaced and duplicates ignored");
}
DeleteReport RemoveFiles(Fixture& f,const Result& plan,bool keep,const std::wstring& recovery,const std::vector<size_t>& selected={}) {
    std::vector<size_t> rows=selected; if(rows.empty()) for(size_t i=0;i<plan.rows.size();++i) rows.push_back(i);
    return DeleteFiles(plan,rows,keep,recovery,f.cancel,f.checking,f.progress);
}
bool Exists(const std::wstring& path) { return GetFileAttributesW(ApiPath(path).c_str())!=INVALID_FILE_ATTRIBUTES; }
void DeletionSuccess() {
    for(bool keep:{false,true}) {
        Fixture f; const auto recovery=f.root+L"\\recovery"; f.Dir(recovery);
        Put(f.right+L"\\a","OLD A"); Put(f.right+L"\\a:漢字","OLD ADS"); Put(f.right+L"\\a:empty","");
        Put(f.right+L"\\b",""); Put(f.right+L"\\b:note","EMPTY MAIN");
        Put(f.left+L"\\new","NEW"); Put(f.left+L"\\same","SAME"); Put(f.right+L"\\same","SAME");
        f.Dir(f.right+L"\\empty-dir"); f.Dir(f.right+L"\\sub"); Put(f.right+L"\\sub\\c","CHILD");
        Check(CreateHardLinkW(ApiPath(f.root+L"\\alias").c_str(),ApiPath(f.right+L"\\a").c_str(),nullptr)!=FALSE,"delete hardlink fixture");
        Check(SetFileAttributesW(ApiPath(f.right+L"\\a").c_str(),FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)!=FALSE,"delete hidden/system fixture");
        const auto acl=Dacl(f.root+L"\\alias"); const auto attrs=GetFileAttributesW(ApiPath(f.root+L"\\alias").c_str());
        Options o; o.asymmetric=true; o.emptyDirectories=true;
        auto plan=Compare(f.left,f.right,o,f.cancel,f.checking);
        auto report=RemoveFiles(f,plan,keep,keep?recovery:L"invalid unused recovery");
        Check(report.complete && !report.error && report.items.size()==3,"asymmetric regular-file deletions complete");
        for(const auto& item:report.items) Check(item.status==DeleteStatus::Deleted && item.removed && !Exists(item.target),"only accepted file names deleted");
        Check(Exists(f.right+L"\\empty-dir") && Exists(f.right+L"\\sub") && !Exists(f.right+L"\\new") && Read(f.left+L"\\new")=="NEW" &&
            Read(f.right+L"\\same")=="SAME","deletion leaves directories, source and other action types intact");
        Check(Read(f.root+L"\\alias")=="OLD A" && Read(f.root+L"\\alias:漢字")=="OLD ADS" && Dacl(f.root+L"\\alias")==acl &&
            GetFileAttributesW(ApiPath(f.root+L"\\alias").c_str())==attrs,"other hardlink data, ADS, attributes and DACL unchanged");
        Check(f.progress.completed==(keep?6u:3u),"delete progress includes only backups and accepted deletions");
        if(keep) {
            Check(report.recovery.complete && report.recovery.items.size()==3,"all deletion originals backed up");
            for(const auto& item:report.recovery.items) {
                Check(item.logged && item.copy.status==CopyStatus::Copied,"each saved original verified and logged");
                if(plan.rows[item.row].path==L"a") {
                    Check(Read(item.saved)=="OLD A" && Read(item.saved+L":漢字")=="OLD ADS" && Read(item.saved+L":empty").empty(),"deleted main and full ADS set retained");
                    const auto restore=f.root+L"\\manual-restore";
                    Check(CopyFileW(ApiPath(item.saved).c_str(),ApiPath(restore).c_str(),TRUE)!=FALSE && Read(restore)=="OLD A" &&
                        Read(restore+L":漢字")=="OLD ADS","manual restore to a new name");
                }
            }
            const auto log=Read(report.journal);
            Check(log.find("\"operation\":\"delete_right\"")!=std::string::npos && log.find("\"status\":\"deleted\"")!=std::string::npos &&
                log.find("\"removed\":true")!=std::string::npos && log.find("\"status\":\"complete\"")!=std::string::npos,"complete deletion protocol");
        } else Check(report.recovery.items.empty() && report.recovery.runDirectory.empty() && report.journal.empty(),"OFF has no backup or persistent deletion log");
        f.NoStaging(f.right);
    }
    Fixture f; Put(f.right+L"\\a","A"); Put(f.right+L"\\b","B"); auto plan=f.Scan(true);
    auto report=RemoveFiles(f,plan,false,L"",{0,0,9999});
    Check(report.complete && report.items.size()==1 && !Exists(f.right+L"\\a") && Read(f.right+L"\\b")=="B","frozen visible set deduplicated and hidden file retained");
    Fixture symmetric; Put(symmetric.right+L"\\a","KEEP"); plan=symmetric.Scan(); plan.rows[0].action=Action::DeleteRight;
    report=RemoveFiles(symmetric,plan,false,L"");
    Check(report.stale && !report.complete && Read(symmetric.right+L"\\a")=="KEEP","symmetric snapshot cannot authorize manually forged deletion");
    for(int mode=0;mode<3;++mode) {
        Fixture blocked; Put(blocked.right+L"\\a","KEEP"); const auto recovery=blocked.root+L"\\recovery"; blocked.Dir(recovery);
        auto invalid=RemoveFiles(blocked,blocked.Scan(true),true,mode==0?L"":mode==1?blocked.right:blocked.root+L"\\missing");
        Check(!invalid.complete && invalid.error && !invalid.items[0].removed && Read(blocked.right+L"\\a")=="KEEP","invalid, overlapping or missing recovery blocks deletion");
    }
    Fixture excluded; excluded.Dir(excluded.left+L"\\private"); excluded.Dir(excluded.right+L"\\private");
    Put(excluded.left+L"\\private\\a","SOURCE"); Put(excluded.right+L"\\private\\a","KEEP");
    Options options; options.asymmetric=true; options.mask=L"*.* | private\\";
    auto filtered=Compare(excluded.left,excluded.right,options,excluded.cancel,excluded.checking);
    auto omitted=RemoveFiles(excluded,filtered,false,L"");
    Check(omitted.items.empty() && Read(excluded.right+L"\\private\\a")=="KEEP","excluded subtree never deleted");
}
void DeletionFailures() {
    for(bool keep:{false,true}) for(int mode=0;mode<15;++mode) {
        if(!keep && mode>=9) continue;
        Fixture f; const auto recovery=f.root+L"\\recovery"; f.Dir(recovery);
        const std::string data(2*1024*1024,'O'); Put(f.right+L"\\a",data); Put(f.right+L"\\a:old","ADS"); Put(f.right+L"\\b","B");
        if(mode==1) Check(SetFileAttributesW(ApiPath(f.right+L"\\a").c_str(),FILE_ATTRIBUTE_READONLY)!=FALSE,"readonly delete fixture");
        if(mode==8) { Acl(f.right,L"D:P(D;;0x40;;;WD)(A;OICI;FA;;;OW)"); Acl(f.right+L"\\a",L"D:P(D;;SD;;;WD)(A;;FA;;;OW)"); }
        auto plan=f.Scan(true); HANDLE lock=INVALID_HANDLE_VALUE; bool reached=false,removed=false;
        if(mode==0) Put(f.right+L"\\a",data+"CHANGED",false);
        if(mode==2) { lock=CreateFileW(ApiPath(f.right+L"\\a").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr); Check(lock!=INVALID_HANDLE_VALUE,"exclusive delete lock"); }
        SetCopyHook([&](CopyPhase phase,const std::wstring& path) {
            if(phase==CopyPhase::Deleted) removed=true;
            if(reached) return;
            if(phase==CopyPhase::Deleting) {
                if(mode==3) { reached=true; f.cancel=true; }
                if(mode==5) { reached=true; Put(f.left+L"\\a","NEW COUNTERPART"); }
                if(mode==6) { reached=true; Put(f.right+L"\\a",data+"CHANGED",false); }
                if(mode==7) { reached=true; Check(MoveFileW(ApiPath(f.right).c_str(),ApiPath(f.root+L"\\old-right").c_str())!=FALSE,"move owned parent"); f.Junction(f.right,f.root+L"\\old-right"); }
            }
            if(mode==4 && phase==CopyPhase::Deleted) { reached=true; f.cancel=true; }
            if(mode==9 && phase==CopyPhase::Copying && f.progress.copied>0) { reached=true; f.cancel=true; }
            if(mode==10 && phase==CopyPhase::Verifying && f.progress.verified>0) { reached=true; f.cancel=true; }
            if(mode==11 && phase==CopyPhase::Verifying) { reached=true; Put(path,"BAD",false); }
        });
        if(mode==12) dirsync_test::SetSpaceFault(dirsync_test::SpaceFault::MainData,ERROR_DISK_FULL,2);
        if(mode==13 || mode==14) SetRecoveryLogHook([&](const std::wstring& line)->DWORD {
            if((mode==13 && line.find(L"\"event\":\"planned\"")!=std::wstring::npos && line.find(L"\"target\":")!=std::wstring::npos) ||
               (mode==14 && removed && line.find(L"\"event\":\"result\"")!=std::wstring::npos)) { reached=true; return ERROR_WRITE_FAULT; } return 0;
        });
        auto report=RemoveFiles(f,plan,keep,recovery); SetCopyHook({}); SetRecoveryLogHook({});
        if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock);
        if(mode==8) { Acl(f.right,L"D:P(A;OICI;FA;;;OW)"); Acl(f.right+L"\\a",L"D:P(A;;FA;;;OW)"); }
        if(report.complete || !(report.error || report.stale || report.cancelled)) std::cerr<<"delete failure mode="<<mode<<", recovery="<<keep<<"\n";
        Check(!report.complete && (report.error || report.stale || report.cancelled),"delete failure never complete");
        Check(report.items.size()==2 && !report.items[1].removed && Read(f.right+L"\\b")=="B","first failure/cancel prevents subsequent deletion");
        if(mode==4 || mode==14) Check(report.items[0].removed && report.items[0].status==DeleteStatus::Deleted && !Exists(f.right+L"\\a"),"accepted deletion retained after late Stop/log failure");
        else Check(!report.items[0].removed && Read(f.right+L"\\a")==((mode==0 || mode==6)?data+"CHANGED":data) &&
            Read(f.right+L"\\a:old")=="ADS","failure before deletion preserves original main and ADS");
        if(mode==0) Check(report.stale,"post-scan target change makes deletion stale");
        if(mode==1) Check(report.error==ERROR_ACCESS_DENIED && report.recovery.items.empty() &&
            (GetFileAttributesW(ApiPath(f.right+L"\\a").c_str())&FILE_ATTRIBUTE_READONLY),"read-only refused before backup without attribute changes");
        if(mode==2) Check(report.stale || report.error==ERROR_SHARING_VIOLATION,"locked deletion refused");
        if(mode==3 || mode==4) Check(reached && report.cancelled,"Stop before/after DeleteFile observed");
        if(mode==5) Check(reached && report.error==ERROR_FILE_EXISTS && Read(f.left+L"\\a")=="NEW COUNTERPART","new counterpart prevents deletion");
        if(mode==6) Check(reached && report.error==ERROR_FILE_INVALID,"changed target before delete refused");
        if(mode==7) Check(reached && report.error==ERROR_NOT_SUPPORTED,"reparse parent before delete refused");
        if(mode==8) Check(report.error==ERROR_ACCESS_DENIED,"real delete permission denial preserved");
        if(mode==9 || mode==10) Check(reached && report.recovery.cancelled && report.items[0].status==DeleteStatus::NotStarted,"Stop in recovery Copy/Verify prevents every deletion");
        if(mode==11) Check(reached && report.recovery.error==ERROR_CRC,"damaged recovery prevents every deletion");
        if(mode==12) Check(dirsync_test::SpaceFaultReached() && report.recovery.items[0].logged && report.items[0].status==DeleteStatus::NotStarted,"failed second backup prevents first deletion");
        if(mode==13 || mode==14) Check(reached && report.journalError==ERROR_WRITE_FAULT && Read(report.journal).find("\"event\":\"end\"")==std::string::npos,"delete journal error retains incomplete evidence");
        if(keep && mode>=3 && mode<=8) Check(report.recovery.complete && Read(report.recovery.items[0].saved)==data && Read(report.recovery.items[0].saved+L":old")=="ADS","verified recovery retained after later failure/Stop");
    }
}
void DeletionPaths() {
    for(bool keep:{false,true}) for(bool wide:{false,true}) {
        Fixture f; const auto recovery=f.root+L"\\recovery-漢字"; f.Dir(recovery);
        for(int i=0;i<(wide?4:1);++i) { auto part=L"\\"+std::wstring(wide?70:90,L'階'); f.left+=part; f.right+=part; f.Dir(f.left); f.Dir(f.right); }
        const auto name=L"\\žluťoučký-😀.txt"; Put(f.right+name,"OLD"); Put(f.right+name+L":漢字","ADS");
        Check(wide?f.right.size()>260:WideCharToMultiByte(CP_UTF8,0,f.right.c_str(),-1,nullptr,0,nullptr,nullptr)>260,"long Unicode deletion ancestors");
        auto report=RemoveFiles(f,f.Scan(true),keep,recovery);
        Check(report.complete && report.items.size()==1 && report.items[0].removed && !Exists(f.right+name),"long Unicode deletion succeeds");
        if(keep) Check(Read(report.recovery.items[0].saved)=="OLD" && Read(report.recovery.items[0].saved+L":漢字")=="ADS","long Unicode deletion recovery succeeds");
    }
}
void DiskSpace() {
    using dirsync_test::SpaceFault;
    for(auto fault:{SpaceFault::Reserve,SpaceFault::MainData,SpaceFault::NamedStream,SpaceFault::RetainStage,SpaceFault::Publish}) {
        for(DWORD error:{DWORD(ERROR_DISK_FULL),DWORD(ERROR_HANDLE_DISK_FULL)}) {
            Fixture f; const bool prior=fault!=SpaceFault::Reserve;
            const std::string data(8*1024*1024,'F');
            if(prior) { Put(f.left+L"\\0-done","DONE"); Put(f.left+L"\\0-done:note","DONE-ADS"); }
            Put(f.left+L"\\a",fault==SpaceFault::NamedStream?"MAIN":data);
            Put(f.left+L"\\a:漢字",fault==SpaceFault::NamedStream?data:"ADS"); Put(f.left+L"\\z-next","NEXT");
            auto plan=f.Scan(); dirsync_test::SetSpaceFault(fault,error,prior?2:1);
            auto report=f.Run(plan); const size_t index=prior?1:0;
            Check(dirsync_test::SpaceFaultReached(),"disk-full fault reached requested I/O boundary");
            dirsync_test::ReleaseSpaceFault();
            Check(!report.stale && !report.cancelled && !f.cancel,"disk full is failure, not cancellation or stale plan");
            Check(report.items.size()==index+2 && report.items[index].status==CopyStatus::Failed && report.items[index].error==error,"disk-full OS error preserved");
            Check(!report.items[index].published && report.items[index+1].status==CopyStatus::NotStarted,"disk full does not publish or start next file");
            Check(GetFileAttributesW(ApiPath(f.right+L"\\a").c_str())==INVALID_FILE_ATTRIBUTES &&
                GetFileAttributesW(ApiPath(f.right+L"\\z-next").c_str())==INVALID_FILE_ATTRIBUTES,"disk-full targets absent");
            Check(f.progress.completed==index,"only earlier verified copies count as complete");
            if(fault==SpaceFault::MainData || fault==SpaceFault::NamedStream || fault==SpaceFault::RetainStage)
                Check(f.progress.copied>1024*1024 && f.progress.copied<8*1024*1024 && f.progress.verified==24,
                    "partial transfer reports bytes without verifying incomplete file");
            if(prior) Check(report.items[0].status==CopyStatus::Copied && Read(f.right+L"\\0-done")=="DONE" &&
                Read(f.right+L"\\0-done:note")=="DONE-ADS","completed main data and ADS survive disk full");
            if(fault==SpaceFault::RetainStage) {
                const auto& stage=report.items[index].remainingStage;
                Check(!stage.empty() && ApiPath(stage).rfind(ApiPath(f.right)+L"\\.dirsync-",0)==0,"blocked cleanup reports owned stage path");
                auto partial=Read(stage); Check(!partial.empty() && partial.size()<data.size(),"retained stage contains real partial main data");
                Check(DeleteFileW(ApiPath(stage).c_str())!=FALSE,"remove only owned retained fixture after releasing lock");
            } else Check(report.items[index].remainingStage.empty(),"disk-full staging cleanup succeeded");
            f.NoStaging(f.right);
            Check(Read(f.left+L"\\a")== (fault==SpaceFault::NamedStream?"MAIN":data) &&
                Read(f.left+L"\\a:漢字")== (fault==SpaceFault::NamedStream?data:"ADS"),"source main and ADS unchanged after disk full");
            HANDLE source=CreateFileW(ApiPath(f.left+L"\\a").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
            Check(source!=INVALID_HANDLE_VALUE,"disk-full source handles released"); if(source!=INVALID_HANDLE_VALUE) CloseHandle(source);
            dirsync_test::SetSpaceFault(SpaceFault::None,0,1);
            f.progress.copied=0; f.progress.verified=0; f.progress.completed=0;
            auto retry=f.Run(f.Scan());
            Check(retry.items.size()==2 && retry.items[0].status==CopyStatus::Copied && retry.items[1].status==CopyStatus::Copied,"fresh compare and retry succeeds after space fault removed");
            Check(Read(f.right+L"\\a")==Read(f.left+L"\\a") && Read(f.right+L"\\a:漢字")==Read(f.left+L"\\a:漢字") &&
                Read(f.right+L"\\z-next")=="NEXT","retry copies exact main data and ADS"); f.NoStaging(f.right);
        }
    }
}
}
int main() {
    try { Success(); Scope(); ModifyOnlyDestination(); Boundaries(); Paths(); AfterPublication(); DiskSpace();
        RecoverySuccess(); RecoveryFailures(); RecoveryPaths(); OverwriteSuccess(); OverwriteFailures(); OverwritePaths();
        DeletionSuccess(); DeletionFailures(); DeletionPaths();
        std::cout<<checks<<" copy/recovery/overwrite/delete assertions passed\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"FAILED: "<<e.what()<<"\n"; return 1; }
}
