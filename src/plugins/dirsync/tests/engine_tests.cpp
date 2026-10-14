// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "../engine.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <winioctl.h>
using namespace dirsync;
namespace
{
int assertions = 0;
void Check(bool value, const char* name)
{ ++assertions; if (!value) throw std::runtime_error(name); }
Entry File(const wchar_t* path, uint64_t size, uint64_t time)
{ Entry e; e.path = path; e.size = size; e.time = time; return e; }
void Matrix()
{
    constexpr uint64_t second = 10000000, base = 134000000000000000;
    struct Pair { uint64_t leftSize, rightSize, leftTime, rightTime; bool same; };
    Pair pairs[] = {{4,4,base,base,true},{4,4,base+120*second,base,true},
        {4,4,base,base+120*second,true},{4,4,base,base,false},
        {4,4,base+120*second,base,false},{4,4,base,base+120*second,false},
        {4,6,base,base,false},{4,6,base+120*second,base,false},
        {4,6,base,base+120*second,false},{4,4,base+second,base,true},
        {4,4,base+2*second,base,true},{4,4,base+3600*second,base,true}};
    for (int asymmetric = 0; asymmetric < 2; ++asymmetric)
    for (int content = 0; content < 2; ++content)
    for (int ignore = 0; ignore < 2; ++ignore)
    {
        Options o; o.asymmetric = asymmetric != 0; o.content = content != 0; o.ignoreDate = ignore != 0;
        int equal = 0;
        for (const auto& p : pairs)
        {
            Entry a = File(L"pair", p.leftSize, p.leftTime), b = File(L"pair", p.rightSize, p.rightTime);
            Row row = Classify(&a, &b, o, p.same);
            if (row.state == State::Equal) ++equal;
            if (!ignore && p.rightTime > p.leftTime + o.tolerance)
                Check(row.action == (asymmetric ? Action::None : Action::CopyLeft), "newer right in asymmetry");
            if (!ignore && p.leftTime > p.rightTime + o.tolerance)
                Check(row.action == Action::CopyRight, "date wins even over identical content");
        }
        const int expected = ignore ? (content ? 6 : 9) : (content ? 3 : 4);
        Check(equal == expected, "eight TC combinations");
        Entry a = File(L"single", 4, base);
        Check(Classify(&a, nullptr, o).action == Action::CopyRight, "left only");
        Check(Classify(nullptr, &a, o).action == (asymmetric ? Action::DeleteRight : Action::CopyLeft), "right only");
    }
    Options o;
    Entry a = File(L"boundary", 4, base+3*second), b = File(L"boundary",4,base);
    Check(Classify(&a,&b,o).state == State::Equal,"inclusive positive tolerance");
    ++a.time; Check(Classify(&a,&b,o).action == Action::CopyRight,"one tick past positive tolerance");
    a.time=base-3*second; Check(Classify(&a,&b,o).state==State::Equal,"inclusive negative tolerance");
    --a.time; Check(Classify(&a,&b,o).action==Action::CopyLeft,"one tick past negative tolerance");
    a.error=ERROR_ACCESS_DENIED; a.reason=Reason::ScanError;
    Check(Classify(nullptr,&a,o).action==Action::Blocked,"error is not absence");
    a.error=0; a.directory=true;
    Check(Classify(&a,&b,o).state==State::Conflict,"file folder collision");
}
void Masks()
{
    Check(Included(L"*.*", L"README"), "star-dot-star includes extensionless");
    Check(Included(L"*.txt | selB\\", L"outer\\keep.txt"), "ordinary inclusion");
    Check(!Included(L"*.txt | selB\\", L"outer\\selB\\child.txt"), "unanchored directory exclusion");
    Check(Included(L"*.txt | \\selB\\", L"outer\\selB\\child.txt"), "anchored exclusion keeps nested name");
    Check(!Included(L"*.txt | \\selB\\", L"selB\\child.txt"), "anchored exclusion removes root");
    Check(Included(L"\\*.txt", L"root.txt"), "root-only mask");
    Check(!Included(L"\\*.txt", L"outer\\child.txt"), "root-only rejects nested");
    Check(Included(L"selA\\**\\*.txt", L"selA\\child.txt"), "double star zero levels");
    Check(Included(L"selA\\**\\*.txt", L"selA\\deep\\nested.txt"), "double star deeper levels");
    Check(!Included(L"selA\\*.txt", L"selA\\deep\\nested.txt"), "single star stays in component");
    Check(Included(L"\"two words.txt\";*.bin | *.old", L"two words.txt"), "quoted multi-mask");
    Check(!ValidateMask(L"<regex"),"regex explicitly rejected");
    Check(!ValidateMask(L"*.txt | a | b"),"multiple exclusions separators rejected");
    Check(!ValidateMask(L"\"unterminated"),"bad quotation rejected");
    Check(Matches(L"*.TXT",L"žluťoučký\\漢字.txt"),"Unicode path and insensitive matching");
}
struct Fixture
{
    std::wstring root, left, right;
    std::vector<std::wstring> files, directories;
    Fixture()
    {
        DWORD size=GetTempPathW(0,nullptr); std::vector<wchar_t> temp(size+1);
        Check(GetTempPathW(DWORD(temp.size()),temp.data()) != 0,"temp path");
        root=std::wstring(temp.data())+L"dirsync-tests-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
        Dir(root); left=root+L"\\left"; right=root+L"\\right"; Dir(left); Dir(right);
    }
    void Dir(const std::wstring& path)
    { Check(CreateDirectoryW(ApiPath(path).c_str(),nullptr)!=FALSE,"fixture directory"); directories.push_back(path); }
    void Write(const std::wstring& path, const std::string& data, uint64_t time=134000000000000000)
    {
        HANDLE file=CreateFileW(ApiPath(path).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        Check(file!=INVALID_HANDLE_VALUE,"fixture file"); DWORD written=0;
        bool ok=WriteFile(file,data.data(),DWORD(data.size()),&written,nullptr)!=FALSE;
        FILETIME ft{DWORD(time),DWORD(time>>32)}; ok &= SetFileTime(file,nullptr,nullptr,&ft)!=FALSE;
        CloseHandle(file); files.push_back(path); Check(ok && written==data.size(),"fixture contents");
    }
    void Junction(const std::wstring& path, const std::wstring& target)
    {
        Dir(path);
        std::wstring substitute=L"\\??\\"+target;
        std::vector<unsigned char> buffer(16+(substitute.size()+1+target.size()+1)*sizeof(wchar_t));
        struct Header { DWORD tag; WORD length,reserved,subOffset,subLength,printOffset,printLength; };
        auto* header=reinterpret_cast<Header*>(buffer.data());
        header->tag=IO_REPARSE_TAG_MOUNT_POINT;
        header->length=WORD(buffer.size()-8);
        header->subLength=WORD(substitute.size()*sizeof(wchar_t));
        header->printOffset=WORD((substitute.size()+1)*sizeof(wchar_t));
        header->printLength=WORD(target.size()*sizeof(wchar_t));
        memcpy(buffer.data()+16,substitute.c_str(),(substitute.size()+1)*sizeof(wchar_t));
        memcpy(buffer.data()+16+header->printOffset,target.c_str(),(target.size()+1)*sizeof(wchar_t));
        HANDLE handle=CreateFileW(ApiPath(path).c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
                                  FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        Check(handle!=INVALID_HANDLE_VALUE,"junction fixture handle");
        DWORD returned=0;
        bool ok=DeviceIoControl(handle,FSCTL_SET_REPARSE_POINT,buffer.data(),DWORD(buffer.size()),nullptr,0,&returned,nullptr)!=FALSE;
        CloseHandle(handle); Check(ok,"junction fixture creation");
    }
    ~Fixture()
    {
        for (auto& f: files) DeleteFileW(ApiPath(f).c_str());
        for (auto it=directories.rbegin();it!=directories.rend();++it) RemoveDirectoryW(ApiPath(*it).c_str());
    }
};
const Row* Find(const Result& result,const wchar_t* path)
{ for (const auto& row:result.rows) if(row.path==path) return &row; return nullptr; }
void Filesystem()
{
    Fixture f;
    f.Write(f.left+L"\\same.txt","AAAA"); f.Write(f.right+L"\\same.txt","AAAA");
    f.Write(f.left+L"\\different.txt","BBBB"); f.Write(f.right+L"\\different.txt","CCCC");
    f.Write(f.right+L"\\unique.txt","right");
    f.Dir(f.left+L"\\selA"); f.Dir(f.right+L"\\selA");
    f.Write(f.left+L"\\selA\\child.txt","left"); f.Write(f.right+L"\\selA\\child.txt","right");
    f.Dir(f.left+L"\\selA\\deep"); f.Dir(f.right+L"\\selA\\deep");
    f.Write(f.left+L"\\selA\\deep\\nested.txt","left"); f.Write(f.right+L"\\selA\\deep\\nested.txt","right");
    f.Dir(f.left+L"\\collision"); f.Write(f.right+L"\\collision","file");
    f.Write(f.left+L"\\collision\\child.txt","protected");
    Options o; o.content=true; o.asymmetric=true;
    std::atomic<bool> cancel{false}; Progress progress;
    Result r=Compare(f.left,f.right,o,cancel,progress);
    Check(Find(r,L"same.txt")->state==State::Equal,"read binary equality");
    Check(Find(r,L"different.txt")->action==Action::Decide,"content difference needs direction");
    Check(Find(r,L"unique.txt")->action==Action::DeleteRight,"asymmetric single preview");
    Check(Find(r,L"collision")->state==State::Conflict,"type conflict visible");
    Check(Find(r,L"collision\\child.txt")->action==Action::Blocked,"conflict protects descendants");
    f.Dir(f.root+L"\\outside"); f.Write(f.root+L"\\outside\\secret.txt","not scanned");
    f.Junction(f.left+L"\\link",f.root+L"\\outside");
    f.Dir(f.right+L"\\link"); f.Write(f.right+L"\\link\\child.txt","protected counterpart");
    r=Compare(f.left,f.right,o,cancel,progress);
    Check(Find(r,L"link") && Find(r,L"link")->state==State::Blocked,"junction is visibly skipped");
    Check(!Find(r,L"link\\secret.txt"),"junction target not traversed");
    Check(Find(r,L"link\\child.txt") && Find(r,L"link\\child.txt")->action==Action::Blocked,"skipped junction protects counterpart children");
    HANDLE locked=CreateFileW(ApiPath(f.left+L"\\same.txt").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
    Check(locked!=INVALID_HANDLE_VALUE,"fixture lock");
    r=Compare(f.left,f.right,o,cancel,progress); CloseHandle(locked);
    Check(Find(r,L"same.txt")->state==State::Blocked,"unreadable content blocks action");
    o.onlySelected=true; o.recursive=false; o.selection={{L"selA",true},{L"unique.txt",false}};
    r=Compare(f.left,f.right,o,cancel,progress);
    Check(Find(r,L"selA\\child.txt")!=nullptr,"selected directory includes direct children without recursion");
    Check(Find(r,L"selA\\deep\\nested.txt")==nullptr,"no deeper selection traversal without recursion");
    Check(Find(r,L"unique.txt")!=nullptr && Find(r,L"same.txt")==nullptr,"selection union restricts scan");
    o.recursive=true; o.mask=L"*.txt | selA\\";
    r=Compare(f.left,f.right,o,cancel,progress);
    Check(r.rows.size()==1 && r.rows[0].path==L"unique.txt","excluded selected subtree stays protected");
    o=Options{}; o.emptyDirectories=true;
    f.Dir(f.left+L"\\empty");
    r=Compare(f.left,f.right,o,cancel,progress);
    Check(Find(r,L"empty") && Find(r,L"empty")->action==Action::CopyRight,"actual empty directory preview");
    o.recursive=false; r=Compare(f.left,f.right,o,cancel,progress);
    Check(Find(r,L"empty") && Find(r,L"empty")->action==Action::CopyRight,"empty directories without recursion");
    Check(!Find(r,L"selA"),"nonempty filtered folder never becomes empty");
    f.Write(f.left+L"\\masked.bin","excluded file"); f.Dir(f.right+L"\\masked.bin");
    o.mask=L"*.txt"; o.asymmetric=true;
    r=Compare(f.left,f.right,o,cancel,progress);
    Check(Find(r,L"masked.bin") && Find(r,L"masked.bin")->reason==Reason::Excluded &&
          Find(r,L"masked.bin")->action==Action::Blocked,"excluded file protects opposite empty directory");
    f.Dir(f.left+L"\\excluded"); f.Write(f.right+L"\\excluded","protected file");
    o.mask=L"*.* | excluded\\";
    r=Compare(f.left,f.right,o,cancel,progress);
    Check(Find(r,L"excluded") && Find(r,L"excluded")->action==Action::Blocked,"excluded directory protects opposite file");
    r=Compare(f.left,f.left,o,cancel,progress);
    Check(r.rows.size()==1 && r.rows[0].reason==Reason::InvalidRoot,"equal roots rejected");
    r=Compare(f.left,f.left+L"\\selA",o,cancel,progress);
    Check(r.rows.size()==1 && r.rows[0].reason==Reason::InvalidRoot,"nested roots rejected");
    cancel=true; r=Compare(f.left,f.right,o,cancel,progress);
    Check(r.cancelled,"cancellation marked");
    for(const auto& row:r.rows) Check(row.action==Action::Blocked,"partial result cannot propose destructive action");
    cancel=false;
    std::wstring a=f.left,b=f.right;
    for(int i=0;i<5;++i)
    { std::wstring component=std::wstring(45,L'漢')+L"ž"; a+=L"\\"+component; b+=L"\\"+component; f.Dir(a); f.Dir(b); }
    f.Write(a+L"\\žluťoučký.txt","long Unicode"); f.Write(b+L"\\žluťoučký.txt","long Unicode");
    Check(a.size()>260,"long UTF16 fixture");
    o=Options{}; o.content=true;
    r=Compare(f.left,f.right,o,cancel,progress);
    bool seen=false;
    for(const auto& row:r.rows) if(row.path.find(L"žluťoučký.txt")!=std::wstring::npos) { seen=true; Check(row.state==State::Equal,"long Unicode read"); }
    Check(seen,"long path enumerated");
    std::wstring decoded;
    Check(DecodePath(u8"C:\\žluťoučký\\漢字",decoded) && decoded==L"C:\\žluťoučký\\漢字","SDK UTF8 decoding");
    Check(!NormalizeRoot(L"relative\\path",decoded),"relative root rejected");
    Check(!NormalizeRoot(L"\\\\.\\PhysicalDrive0",decoded),"device root rejected");
}
}
int main()
{
    try { Matrix(); Masks(); Filesystem(); std::cout<<assertions<<" assertions passed\n"; return 0; }
    catch(const std::exception& error) { std::cerr<<"FAILED: "<<error.what()<<"\n"; return 1; }
}
