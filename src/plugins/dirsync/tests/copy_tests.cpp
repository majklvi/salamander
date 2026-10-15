// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "../copy.h"
#include "../recovery.h"
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
        RecoverySuccess(); RecoveryFailures(); RecoveryPaths(); std::cout<<checks<<" copy/recovery assertions passed\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"FAILED: "<<e.what()<<"\n"; return 1; }
}
