// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// Never included in the shipped SPL. Exercise the real Release vtable method
// with a worker in this DLL, then let the caller immediately FreeLibrary it.
#include "../dirsync.cpp"
#include <map>

namespace
{
std::unique_ptr<Preview> probe;
std::atomic<bool> finished{false};
std::atomic<bool> copyBoundary{false};
INT_PTR CALLBACK EmptyDialog(HWND,UINT,WPARAM,LPARAM) { return FALSE; }
bool RecoveryLogged()
{
    if(probe->overwriting) return !probe->overwriteReport.keepOriginals || probe->overwriteReport.recovery.complete;
    return !probe->saving || (!probe->recoveryReport.journalError && !probe->recoveryReport.complete &&
        std::all_of(probe->recoveryReport.items.begin(),probe->recoveryReport.items.end(),
            [](const dirsync::RecoveryItem& item) { return item.logged; }));
}
}
// Exercise the same Unicode path expansion used by the native folder picker.
extern "C" __declspec(dllexport) unsigned WINAPI ProbeFolderPath(const wchar_t* expected)
{
    std::wstring full;
    if(!FolderSelectionPath(expected,full) || full!=expected) return 0;
    auto api=dirsync::ApiPath(expected);
    DWORD size=GetShortPathNameW(api.c_str(),nullptr,0);
    if(!size) return 0;
    std::vector<wchar_t> alias(size);
    DWORD count=GetShortPathNameW(api.c_str(),alias.data(),size);
    if(!count || count>=size || !FolderSelectionPath(alias.data(),full) || full!=expected) return 0;
    if(FolderSelectionPath(std::wstring(expected)+L"\\missing-picker-folder",full) || !full.empty()) return 0;
    return 3;
}
// Check the production proposal/path helpers without executing file operations.
extern "C" __declspec(dllexport) unsigned WINAPI ProbeProposalRules(const wchar_t* left,const wchar_t* right)
{
    Preview p;p.left=left;p.right=right;
    unsigned checks=0;auto check=[&](bool ok) {if(!ok) throw std::runtime_error("proposal rule");++checks;};
    try {
        using A=dirsync::Action;using S=dirsync::State;
        dirsync::Row pair;pair.hasLeft=pair.hasRight=true;pair.state=S::Different;pair.action=A::CopyRight;
        pair.path=std::wstring(90,L'階')+L"\\"+std::wstring(90,L'長')+L"\\"+std::wstring(90,L'文')+L"\\žluťoučký-漢字.txt";
        auto onlyLeft=pair;onlyLeft.hasRight=false;onlyLeft.state=S::LeftOnly;onlyLeft.path=L"left-漢字.txt";
        auto onlyRight=pair;onlyRight.hasLeft=false;onlyRight.state=S::RightOnly;onlyRight.action=A::CopyLeft;onlyRight.path=L"right-ž.txt";
        auto equal=pair;equal.state=S::Equal;equal.action=A::None;equal.path=L"equal.txt";
        auto blocked=pair;blocked.state=S::Blocked;blocked.action=A::Blocked;
        auto folder=onlyLeft;folder.left.directory=true;
        p.result.rows={pair,onlyLeft,onlyRight,equal,blocked,folder};
        for(const auto& row:p.result.rows) p.defaults.push_back(row.action);
        A action{};
        // Rows: two-sided, left-only, right-only, equal, blocked, directory.
        const bool allowed[6][7]={{true,true,true,false,true,true,true},
            {true,true,false,false,true,true,false},{true,false,true,false,true,true,false},
            {true,true,true,false,true,true,false},{false,false,false,false,false,false,false},
            {false,false,false,false,false,false,false}};
        for(size_t row=0;row<6;++row) for(int command=0;command<7;++command)
            check(Proposal(p,row,command,action)==allowed[row][command]);
        check(!Proposal(p,99,0,action));
        auto counts=ChangeProposals(p,{0,4,5},0);
        check(counts.first==1 && counts.second==2);
        check(p.result.rows[0].action==A::None && p.result.rows[1].action==A::CopyRight && p.result.rows[4].action==A::Blocked);
        counts=ChangeProposals(p,{0},RestoreProposal);check(counts.first==1 && p.result.rows[0].action==A::CopyRight);
        counts=ChangeProposals(p,{0},ReverseProposal);check(counts.first==1 && p.result.rows[0].action==A::CopyLeft);
        p.options.asymmetric=true;
        check(!Proposal(p,0,2,action) && !Proposal(p,0,ReverseProposal,action));
        check(Proposal(p,2,3,action) && action==A::DeleteRight);
        check(!Proposal(p,1,3,action) && !Proposal(p,0,3,action));
        check(!Proposal(p,2,RestoreProposal,action)); // a mode change must discard old results
        auto relative=RowPaths(p,{0,1,2},0);check(relative==pair.path+L"\r\n"+onlyLeft.path+L"\r\n"+onlyRight.path);
        std::wstring root;check(dirsync::NormalizeRoot(left,root));
        check(RowPaths(p,{0,1,2},1)==root+L"\\"+pair.path+L"\r\n"+root+L"\\"+onlyLeft.path);
        check(dirsync::NormalizeRoot(right,root));
        check(RowPaths(p,{0,1,2},2)==root+L"\\"+pair.path+L"\r\n"+root+L"\\"+onlyRight.path);
        check(RowPaths(p,{},0).empty() && RowPaths(p,{99},1).empty() && RowPaths(p,{1},2).empty());
        check(ChangeProposals(p,{},0).first==0);
        INITCOMMONCONTROLSEX classes{sizeof(classes),ICC_LISTVIEW_CLASSES};check(InitCommonControlsEx(&classes)!=FALSE);
        struct OwnedWindow {HWND value;~OwnedWindow(){if(value) DestroyWindow(value);}};
        OwnedWindow list{CreateWindowExW(0,WC_LISTVIEWW,L"",WS_POPUP|LVS_REPORT|LVS_OWNERDATA,0,0,400,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
        check(list.value!=nullptr);LVCOLUMNW column{};column.mask=LVCF_WIDTH;column.cx=400;
        SendMessageW(list.value,LVM_INSERTCOLUMNW,0,LPARAM(&column));ListView_SetItemCount(list.value,6);
        RECT bounds{};check(ListView_GetItemRect(list.value,5,&bounds,LVIR_BOUNDS)!=FALSE);
        check(PreviewHit(list.value,{10,(bounds.top+bounds.bottom)/2})==5);
        check(PreviewHit(list.value,{10,bounds.bottom+10})==-1);
        check(PreviewHit(list.value,{-1,bounds.top})==-1);
        check(!PlanEnabled(p,PlanSummary));
        p.result.snapshot=dirsync::Compare(left,right,p.options,p.cancel,p.progress).snapshot;
        check(PlanEnabled(p,PlanSummary));
        check(!PlanEnabled(p,PlanCopy) && !PlanEnabled(p,PlanOverwrite));
        for(auto& row:p.result.rows) row.left.identified=row.right.identified=true;
        p.visible={1};check(PlanEnabled(p,PlanCopy));
        check(!PlanEnabled(p,PlanOverwrite) && !PlanEnabled(p,PlanSave));
        p.visible={0};check(PlanEnabled(p,PlanOverwrite));
        const bool keep=settings.keepOriginals;settings.keepOriginals=true;
        bool saveOn=PlanEnabled(p,PlanSave);settings.keepOriginals=false;
        bool saveOff=PlanEnabled(p,PlanSave);settings.keepOriginals=keep;
        check(saveOn && !saveOff);
        p.visible={2};p.result.rows[2].action=A::DeleteRight;
        check(PlanEnabled(p,PlanDelete) && !PlanEnabled(p,PlanCopy));
        p.result.stale=true;check(!PlanEnabled(p,PlanSummary) && !PlanEnabled(p,PlanDelete));p.result.stale=false;
        p.result.cancelled=true;check(!PlanEnabled(p,PlanSummary));p.result.cancelled=false;
        p.failed=true;check(!PlanEnabled(p,PlanDelete));p.failed=false;
        p.running=true;check(!PlanEnabled(p,PlanSummary));p.running=false;
        p.options.asymmetric=false;p.result.snapshot=dirsync::Compare(left,right,p.options,p.cancel,p.progress).snapshot;
        check(!PlanEnabled(p,PlanDelete));
    } catch(...) {return 0;}
    return checks;
}
// Real native toggle states, option handlers and list/status geometry; no executor.
extern "C" __declspec(dllexport) BOOL WINAPI ProbePreviewControls(const wchar_t* left,const wchar_t* right)
{
    struct Template {DLGTEMPLATE dialog;WORD menu=0,cls=0,title=0;} dialog{};
    dialog.dialog.style=WS_POPUP;dialog.dialog.cx=610;dialog.dialog.cy=470;
    auto procedure=+[](HWND window,UINT message,WPARAM w,LPARAM l)->INT_PTR {
        return message==WM_COMMAND?DialogBody(window,message,w,l):FALSE;
    };
    HWND window=CreateDialogIndirectParamW(GetModuleHandleW(nullptr),&dialog.dialog,nullptr,procedure,0);
    if(!window) return FALSE;
    bool ok=true;Preview p;p.window=window;p.left=left;p.right=right;p.panelLeft=L"panel-left";p.panelRight=L"panel-right";
    SetWindowLongPtrW(window,DWLP_USER,reinterpret_cast<LONG_PTR>(&p));
    const int filters[]={IDC_EQUAL,IDC_DIFFERENT,IDC_SINGLE,IDC_TORIGHT,IDC_TOLEFT};
    for(size_t i=0;i<5;++i) {
        HWND button=CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|BS_CHECKBOX|BS_PUSHLIKE|BS_ICON,0,0,24,24,window,HMENU(INT_PTR(filters[i])),instance,nullptr);
        ok &= button!=nullptr;
        p.filterChecks[i]=true;CheckDlgButton(window,filters[i],BST_CHECKED);
    }
    for(int id:{IDC_RECURSIVE,IDC_CONTENT,IDC_IGNORE,IDC_ASYMMETRIC,IDC_EMPTY,IDC_SELECTED})
        ok &= CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|BS_AUTOCHECKBOX,0,0,20,20,window,HMENU(INT_PTR(id)),instance,nullptr)!=nullptr;
    for(int id:{IDC_LEFT,IDC_RIGHT})
        ok &= CreateWindowExW(0,L"EDIT",id==IDC_LEFT?left:right,WS_CHILD,0,0,20,20,window,HMENU(INT_PTR(id)),instance,nullptr)!=nullptr;
    HWND list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|LVS_REPORT|LVS_OWNERDATA,8,100,500,300,window,HMENU(INT_PTR(IDC_LIST)),instance,nullptr);
    HWND bar=CreateWindowExW(0,STATUSCLASSNAMEW,L"",WS_CHILD,0,440,610,30,window,HMENU(INT_PTR(IDC_STATUS)),instance,nullptr);
    ok &= list!=nullptr && bar!=nullptr;
    p.result=dirsync::Compare(left,right,p.options,p.cancel,p.progress);auto snapshot=p.result.snapshot;
    dirsync::Row row;row.path=L"skipped-漢字.txt";row.state=dirsync::State::Different;row.action=dirsync::Action::None;
    p.result.rows={row};p.defaults={dirsync::Action::CopyRight};p.visible={0};ListView_SetItemCount(list,1);
    CheckDlgButton(window,IDC_RECURSIVE,BST_CHECKED);p.options.selection={{L"item",false}};
    for(int id:{IDC_RECURSIVE,IDC_CONTENT,IDC_IGNORE,IDC_ASYMMETRIC,IDC_EMPTY,IDC_SELECTED}) {
        CheckDlgButton(window,id,Checked(window,id)?BST_UNCHECKED:BST_CHECKED);
        DialogBody(window,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),0);
        ok &= p.result.snapshot==snapshot && p.result.rows.size()==1 && p.result.rows[0].action==dirsync::Action::None &&
            p.defaults[0]==dirsync::Action::CopyRight && ListView_GetItemCount(list)==1;
    }
    ok &= PendingOptions(p) && !p.options.asymmetric && !p.options.content && !p.options.onlySelected && p.options.recursive;
    ok &= ControlText(window,IDC_LEFT)==left && ControlText(window,IDC_RIGHT)==right;
    for(unsigned bits=0;bits<32;++bits) {
        // Reach every combination through native click notification delivery,
        // not by assigning the desired model/check state directly.
        for(size_t i=0;i<5;++i) if(p.filterChecks[i]!=((bits&(1u<<i))!=0))
            SendDlgItemMessageW(window,filters[i],BM_CLICK,0,0);
        for(size_t i=0;i<5;++i) {
            HWND button=GetDlgItem(window,filters[i]);SendMessageW(button,WM_KILLFOCUS,0,0);SendMessageW(button,WM_PAINT,0,0);
            ok &= Checked(window,filters[i])==((bits&(1u<<i))!=0);
            ok &= p.filterChecks[i]==((bits&(1u<<i))!=0);
            ok &= (SendMessageW(button,BM_GETSTATE,0,0)&BST_PUSHED)==0;
        }
        ok &= p.visible.size()==((bits&2u)?1u:0u) && ListView_GetItemCount(list)==int(p.visible.size());
    }
    for(int pass=0;pass<3;++pass) for(bool enabled:{false,true}) for(int id:filters) {
        SendDlgItemMessageW(window,id,BM_CLICK,0,0);
        ok &= Checked(window,id)==enabled;
    }
    for(bool enabled:p.filterChecks) ok &= enabled;
    // Categories combine by union; direction switches further restrict copies.
    std::array<bool,5> mixed{false,true,true,true,false};
    row.action=dirsync::Action::CopyRight;ok &= RowVisible(row,mixed);
    row.state=dirsync::State::LeftOnly;ok &= RowVisible(row,mixed);
    row.action=dirsync::Action::CopyLeft;ok &= !RowVisible(row,mixed);
    mixed[4]=true;ok &= RowVisible(row,mixed);
    row.state=dirsync::State::Equal;row.action=dirsync::Action::None;ok &= !RowVisible(row,mixed);
    mixed[0]=true;ok &= RowVisible(row,mixed);
    row.state=dirsync::State::Blocked;mixed.fill(false);ok &= RowVisible(row,mixed);
    for(COLORREF bg:{RGB(255,255,255),RGB(32,32,32)}) {
        row.state=dirsync::State::Different;row.action=dirsync::Action::None;
        ok &= PreviewColor(row,0,bg,RGB(1,2,3))==MarkColor(Mark::Skip);
        COLORREF base=MarkColor(Mark::Different),faded=PreviewColor(row,6,bg,RGB(1,2,3));
        ok &= GetRValue(faded)==(GetRValue(base)+GetRValue(bg))/2 &&
            GetGValue(faded)==(GetGValue(base)+GetGValue(bg))/2 && GetBValue(faded)==(GetBValue(base)+GetBValue(bg))/2;
        row.action=dirsync::Action::CopyRight;ok &= PreviewColor(row,6,bg,RGB(1,2,3))==base;
    }
    for(int width:{900,1200}) {
        SetWindowPos(window,nullptr,0,0,width,600,SWP_NOMOVE|SWP_NOZORDER);SizeStatus(window);
        RECT client{},bounds{},status{};GetClientRect(window,&client);GetWindowRect(list,&bounds);GetWindowRect(bar,&status);
        MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&bounds),2);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&status),2);
        ok &= bounds.left==0 && bounds.right==client.right && bounds.bottom==status.top;
    }
    DestroyWindow(window);p.window=nullptr;return ok;
}
// Exercise the real SDK configuration callbacks without touching user registry data.
extern "C" __declspec(dllexport) BOOL WINAPI ProbeStopControls()
{
    struct Template {DLGTEMPLATE dialog;WORD menu=0,cls=0,title=0;} dialog{};
    dialog.dialog.style=WS_POPUP;dialog.dialog.cx=100;dialog.dialog.cy=80;
    HWND window=CreateDialogIndirectParamW(GetModuleHandleW(nullptr),&dialog.dialog,nullptr,EmptyDialog,0);
    if(!window) return FALSE;
    bool ok=true;Preview p;p.window=window;
    SetWindowLongPtrW(window,DWLP_USER,reinterpret_cast<LONG_PTR>(&p));
    for(int id:{IDC_STOP,IDC_STOP_SYNC,IDC_COMPARE,IDC_SYNCHRONIZE})
        ok &= CreateWindowExW(0,L"BUTTON",L"",WS_CHILD,0,0,30,15,window,HMENU(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr)!=nullptr;
    auto enabled=[&](int id) {return IsWindowEnabled(GetDlgItem(window,id))!=FALSE;};
    Busy(p,false);ok &= !enabled(IDC_STOP) && !enabled(IDC_STOP_SYNC) && enabled(IDC_COMPARE) && enabled(IDC_SYNCHRONIZE);
    for(auto intent:{Preview::Intent::None,Preview::Intent::Apply,Preview::Intent::Summary,Preview::Intent::Copy,
        Preview::Intent::Save,Preview::Intent::Overwrite,Preview::Intent::Delete}) {
        p.running=true;p.validating=intent!=Preview::Intent::None;p.intent=intent;p.cancel=false;
        Busy(p,true);int activeStop=StopControl(p),inactiveStop=activeStop==IDC_STOP?IDC_STOP_SYNC:IDC_STOP;
        const bool files=intent==Preview::Intent::Copy || intent==Preview::Intent::Save || intent==Preview::Intent::Overwrite || intent==Preview::Intent::Delete;
        ok &= activeStop==(files?IDC_STOP_SYNC:IDC_STOP) && enabled(activeStop) && !enabled(inactiveStop);
        ok &= !enabled(IDC_COMPARE) && !enabled(IDC_SYNCHRONIZE);
        DialogBody(window,WM_COMMAND,inactiveStop,0);ok &= !p.cancel;
        DialogBody(window,WM_COMMAND,activeStop,0);ok &= p.cancel;
    }
    p.validating=false;p.intent=Preview::Intent::None;
    for(bool* flag:{&p.copying,&p.saving,&p.overwriting,&p.deleting}) {
        *flag=true;Busy(p,true);ok &= enabled(IDC_STOP_SYNC) && !enabled(IDC_STOP);*flag=false;
    }
    p.running=false;p.cancel=false;Busy(p,false);
    DialogBody(window,WM_COMMAND,IDC_STOP_SYNC,0);DialogBody(window,WM_COMMAND,IDC_STOP,0);
    ok &= !p.cancel && !enabled(IDC_STOP) && !enabled(IDC_STOP_SYNC);
    DestroyWindow(window);p.window=nullptr;return ok;
}
// Exercise the real SDK configuration callbacks without touching user registry data.
extern "C" __declspec(dllexport) unsigned WINAPI ProbeSettingsTest()
{
    struct Registry : CSalamanderRegistryAbstract {
        struct Value { DWORD type; std::vector<BYTE> bytes; };
        std::map<std::string,Value> values;
        bool fail=false;
        BOOL WINAPI ClearKey(HKEY) override { values.clear(); return TRUE; }
        BOOL WINAPI CreateKey(HKEY,const char*,HKEY&) override { return FALSE; }
        BOOL WINAPI OpenKey(HKEY,const char*,HKEY&) override { return FALSE; }
        void WINAPI CloseKey(HKEY) override {}
        BOOL WINAPI DeleteKey(HKEY,const char*) override { return FALSE; }
        BOOL WINAPI GetValue(HKEY,const char* name,DWORD type,void* data,DWORD size) override {
            auto at=values.find(name); if(at==values.end() || at->second.type!=type || at->second.bytes.size()!=size) return FALSE;
            memcpy(data,at->second.bytes.data(),size); return TRUE;
        }
        BOOL WINAPI SetValue(HKEY,const char* name,DWORD type,const void* data,DWORD size) override {
            if(fail) return FALSE;
            auto bytes=static_cast<const BYTE*>(data); values[name]={type,std::vector<BYTE>(bytes,bytes+size)}; return TRUE;
        }
        BOOL WINAPI DeleteValue(HKEY,const char* name) override { return values.erase(name)!=0; }
        BOOL WINAPI GetSize(HKEY,const char* name,DWORD type,DWORD& size) override {
            auto at=values.find(name); if(at==values.end() || at->second.type!=type) return FALSE;
            size=DWORD(at->second.bytes.size()); return TRUE;
        }
    } registry;
    const auto previous=settings; unsigned checks=0;
    auto check=[&](bool ok) { if(!ok) throw std::runtime_error("settings round trip"); ++checks; };
    try {
        HKEY key=reinterpret_cast<HKEY>(UINT_PTR(1)); // opaque key used only by the fake SDK registry
        plugin.LoadConfiguration(nullptr,nullptr,&registry);
        check(settings.keepOriginals && settings.recoveryRoot.empty());
        std::wstring root=L"C:\\obnova-漢字\\"+std::wstring(200,L'階')+L"\\😀";
        settings.keepOriginals=false; settings.recoveryRoot=root;
        settings.actionLabels=true;settings.colors[0][2]=RGB(12,34,56);settings.colors[1][6]=RGB(98,76,54);
        plugin.SaveConfiguration(nullptr,key,&registry);
        settings={}; plugin.LoadConfiguration(nullptr,key,&registry);
        check(!settings.keepOriginals && settings.recoveryRoot==root);
        check(settings.actionLabels && settings.colors[0][2]==RGB(12,34,56) && settings.colors[1][6]==RGB(98,76,54));
        registry.values["ActionLabels"].bytes={2,0,0,0};
        registry.values["DarkColorsV1"].bytes={0,0,0,0};
        registry.values["LightColorsV1"].bytes.back()=255;
        plugin.LoadConfiguration(nullptr,key,&registry);
        check(!settings.actionLabels && settings.colors[0]==DefaultPalette(false) && settings.colors[1]==DefaultPalette(true));
        check(registry.values["KeepOriginals"].type==REG_DWORD && registry.values["RecoveryRootUtf16"].type==REG_BINARY);
        check(WideCharToMultiByte(CP_UTF8,0,root.c_str(),-1,nullptr,0,nullptr,nullptr)>260);
        settings.recoveryRoot=L"C:\\"+std::wstring(300,L'x'); settings.keepOriginals=true;
        plugin.SaveConfiguration(nullptr,key,&registry); settings={}; plugin.LoadConfiguration(nullptr,key,&registry);
        check(settings.keepOriginals && settings.recoveryRoot.size()>260);
        bool saved=false; SettingsCallback(FALSE,key,&registry,&saved); check(saved);
        registry.fail=true; saved=true; SettingsCallback(FALSE,key,&registry,&saved); check(!saved);
        registry.fail=false;
        registry.values["KeepOriginals"].bytes={2,0,0,0};
        registry.values["RecoveryRootUtf16"].bytes={0,0,0};
        plugin.LoadConfiguration(nullptr,key,&registry); check(settings.keepOriginals && settings.recoveryRoot.empty());
        for(const std::vector<BYTE>& invalid:{std::vector<BYTE>{'C',0},std::vector<BYTE>{0,0,'a',0,0,0},std::vector<BYTE>{0,0xd8,0,0}}) {
            registry.values["RecoveryRootUtf16"].bytes=invalid;
            plugin.LoadConfiguration(nullptr,key,&registry); check(settings.recoveryRoot.empty());
        }
        registry.values["RecoveryRootUtf16"].bytes.resize(65524);
        plugin.LoadConfiguration(nullptr,key,&registry); check(settings.recoveryRoot.empty());
        registry.values.clear(); plugin.LoadConfiguration(nullptr,key,&registry);
        check(settings.keepOriginals && settings.recoveryRoot.empty());
        settings=previous; return checks;
    } catch(...) { settings=previous; return 0; }
}
// Drive the real Stop/Close handlers without accessibility enumeration timing.
// Test-only mode bits: 1 = Verify, 2 = after one completed file, 4 = save originals,
// 8 = overwrite, 16 = disable recovery, 32 = deletion, 64 = deletion backup.
extern "C" __declspec(dllexport) BOOL WINAPI ProbeCopyStart(const wchar_t* left,const wchar_t* right,BOOL mode)
{
    try {
        if(probe) return FALSE;
        probe=std::make_unique<Preview>(); probe->left=left; probe->right=right;
        probe->options.asymmetric=(mode&32)!=0;
        probe->result=dirsync::Compare(left,right,probe->options,probe->cancel,probe->progress);
        if(probe->result.rows.size()!=((mode&2)?3u:2u)) { probe.reset(); return FALSE; }
        if(mode&(4|8|32)) {
            if(!(mode&32)) for(auto& row:probe->result.rows) row.action=dirsync::Action::CopyRight;
            probe->recoveryRoot=probe->left.substr(0,probe->left.find_last_of(L'\\'))+L"\\recovery";
        }
        struct Template { DLGTEMPLATE dialog; WORD menu=0,cls=0,title=0; } dialog{};
        dialog.dialog.style=WS_POPUP; dialog.dialog.cx=10; dialog.dialog.cy=10;
        probe->window=CreateDialogIndirectParamW(GetModuleHandleW(nullptr),&dialog.dialog,nullptr,EmptyDialog,0);
        if(!probe->window) { probe.reset(); return FALSE; }
        SetWindowLongPtrW(probe->window,DWLP_USER,reinterpret_cast<LONG_PTR>(probe.get()));
        copyBoundary=false; finished=false; active=probe.get(); probe->saving=(mode&4)!=0;
        probe->overwriting=(mode&8)!=0; probe->deleting=(mode&32)!=0;
        probe->copying=!probe->saving && !probe->overwriting && !probe->deleting; probe->running=true;
        probe->transferCount=(probe->overwriting || probe->deleting) && !(mode&16)?probe->result.rows.size():0;
        probe->worker=std::thread([mode] {
            try {
                dirsync::SetCopyHook([mode,stage=std::wstring{},copiedBase=uint64_t{0},verifiedBase=uint64_t{0}]
                    (dirsync::CopyPhase phase,const std::wstring& path) mutable {
                    if(phase==dirsync::CopyPhase::Copying && stage!=path) {
                        stage=path; copiedBase=probe->copyProgress.copied.load(); verifiedBase=probe->copyProgress.verified.load();
                    }
                    if((mode&2) && probe->copyProgress.completed<=probe->transferCount) return;
                    if((mode&8) && path.rfind(probe->right+L"\\",0)!=0) return;
                    bool reached=(mode&1) ? phase==dirsync::CopyPhase::Verifying && probe->copyProgress.verified>verifiedBase :
                        phase==dirsync::CopyPhase::Copying && probe->copyProgress.copied>copiedBase;
                    if((mode&32) && !(mode&64)) reached=phase==dirsync::CopyPhase::Deleting;
                    if(!reached || copyBoundary.exchange(true)) return;
                    ULONGLONG deadline=GetTickCount64()+5000;
                    while(!probe->cancel && GetTickCount64()<deadline) Sleep(1);
                });
                std::vector<size_t> rows{0,1}; if(mode&2) rows.push_back(2);
                if(mode&32) {
                    probe->deleteReport=dirsync::DeleteFiles(probe->result,rows,!(mode&16),probe->recoveryRoot,probe->cancel,probe->progress,probe->copyProgress);
                } else if(mode&8) {
                    probe->overwriteReport=dirsync::OverwriteFiles(probe->result,rows,!(mode&16),probe->recoveryRoot,probe->cancel,probe->progress,probe->copyProgress);
                    probe->copyReport.cancelled=probe->overwriteReport.cancelled;
                    probe->copyReport.items=probe->overwriteReport.items;
                } else if(mode&4) {
                    probe->recoveryReport=dirsync::SaveOriginals(probe->result,rows,probe->recoveryRoot,probe->cancel,probe->progress,probe->copyProgress);
                    probe->copyReport.cancelled=probe->recoveryReport.cancelled;
                    for(const auto& item:probe->recoveryReport.items) probe->copyReport.items.push_back(item.copy);
                } else probe->copyReport=dirsync::CopyNewFiles(probe->result,rows,probe->cancel,probe->progress,probe->copyProgress);
                dirsync::SetCopyHook({});
            } catch(...) { probe->failed=true; }
            finished=true;
        });
        return TRUE;
    } catch(...) { if(probe && probe->window) DestroyWindow(probe->window); active=nullptr; probe.reset(); return FALSE; }
}
extern "C" __declspec(dllexport) unsigned WINAPI ProbeCopyStatus()
{ return (copyBoundary?1u:0u) | (finished?2u:0u); }
// Same real dialog handlers and Release contract, with deletion-specific reports.
extern "C" __declspec(dllexport) unsigned WINAPI ProbeDeleteFinish(unsigned mode)
{
    if(!probe || !probe->deleting) return 0;
    unsigned status=0; const bool force=(mode&128)!=0,prior=(mode&2)!=0,backup=(mode&64)!=0;
    if(force) {
        if(!plugin.Release(nullptr,FALSE) && !probe->cancel && !finished) status|=1;
        if(plugin.Release(nullptr,TRUE)) status|=2;
    } else {
        DialogBody(probe->window,(mode&256)?WM_CLOSE:WM_COMMAND,(mode&256)?0:IDC_STOP_SYNC,0);
        if(probe->cancel) status|=1;
        if(probe->closing==((mode&256)!=0)) status|=2;
        probe->Join();
    }
    if(!probe->worker.joinable() && finished && !probe->failed) status|=4;
    const auto& r=probe->deleteReport; size_t at=prior?1:0;
    bool ok=r.cancelled && !r.complete && r.items.size()==at+2;
    if(backup) ok=ok && r.recovery.cancelled && r.items[0].status==dirsync::DeleteStatus::NotStarted;
    else ok=ok && (!r.keepOriginals || r.recovery.complete) && r.items[at].status==dirsync::DeleteStatus::Cancelled;
    if(ok && r.items[at+1].status==dirsync::DeleteStatus::NotStarted && !r.items[at].removed && !r.items[at+1].removed) status|=8;
    if((!prior || (r.items[0].status==dirsync::DeleteStatus::Deleted && r.items[0].removed)) &&
        probe->copyProgress.completed==(backup?0:probe->transferCount+at)) status|=16;
    DestroyWindow(probe->window); probe->window=nullptr; active=nullptr; probe.reset();
    return status;
}
extern "C" __declspec(dllexport) unsigned WINAPI ProbeCopyRelease(BOOL force)
{
    if(!probe) return plugin.Release(nullptr,force)?1u:0u;
    const size_t backups=probe->overwriting?static_cast<size_t>(probe->transferCount):0;
    const size_t total=static_cast<size_t>(probe->copyProgress.completed.load());
    const size_t completed=total>backups?total-backups:0;
    unsigned status=plugin.Release(nullptr,force)?1u:0u;
    if(!force) return status;
    if(probe->cancel && !probe->worker.joinable() && finished && !probe->failed) status|=2;
    const auto& r=probe->copyReport;
    if(RecoveryLogged() && r.cancelled && r.items.size()==completed+2 &&
       r.items[completed].status==dirsync::CopyStatus::Cancelled &&
       r.items[completed+1].status==dirsync::CopyStatus::NotStarted) status|=4;
    bool preserved=r.items.size()==completed+2;
    for(size_t i=0;i<completed && preserved;++i)
        preserved=r.items[i].status==dirsync::CopyStatus::Copied && r.items[i].published;
    if(preserved && !r.items[completed].published && r.items[completed].remainingStage.empty()) status|=8;
    if(probe->copyProgress.completed==completed+backups) status|=16;
    if(IsWindow(probe->window)) DestroyWindow(probe->window);
    probe->window=nullptr; active=nullptr; probe.reset();
    return status;
}
extern "C" __declspec(dllexport) unsigned WINAPI ProbeCopyCancel(BOOL close)
{
    if(!probe) return 0;
    // Same handler called by the dialog message loop, on its owner thread.
    DialogBody(probe->window,close?WM_CLOSE:WM_COMMAND,close?0:IDC_STOP_SYNC,0);
    unsigned status=probe->cancel?1u:0u;
    if(probe->closing==(close!=FALSE)) status|=2;
    probe->Join();
    if(!probe->worker.joinable() && finished && !probe->failed) status|=4;
    const auto& r=probe->copyReport;
    if(RecoveryLogged() && r.cancelled && r.items.size()==2 && r.items[0].status==dirsync::CopyStatus::Cancelled &&
       r.items[1].status==dirsync::CopyStatus::NotStarted) status|=8;
    if(r.items.size()==2 && !r.items[0].published && r.items[0].remainingStage.empty() &&
        probe->copyProgress.completed==(probe->overwriting?probe->transferCount:0)) status|=16;
    DestroyWindow(probe->window); probe->window=nullptr; active=nullptr; probe.reset();
    return status;
}
extern "C" __declspec(dllexport) BOOL WINAPI ProbeStart(const wchar_t* left, const wchar_t* right, BOOL content)
{
    try
    {
        if (probe) return FALSE;
        probe = std::make_unique<Preview>();
        probe->left = left; probe->right = right;
        probe->options.content = content != FALSE;
        if (!content)
        {
            probe->options.mask = L"*.* | ";
            for (int i = 0; i < 2000; ++i) probe->options.mask += L"never" + std::to_wstring(i) + L"*;";
        }
        finished = false; active = probe.get(); probe->running = true;
        probe->worker = std::thread([] {
            try { probe->result = dirsync::Compare(probe->left, probe->right, probe->options, probe->cancel, probe->progress); }
            catch (...) { probe->failed = true; }
            finished = true;
        });
        return TRUE;
    }
    catch (...) { active = nullptr; probe.reset(); return FALSE; }
}
extern "C" __declspec(dllexport) unsigned WINAPI ProbeStatus()
{
    if (!probe) return 0;
    return (!finished ? 1u : 0u) | (probe->progress.scanned > 0 ? 2u : 0u) |
        (probe->progress.readBytes > 0 ? 4u : 0u) | (probe->cancel ? 8u : 0u);
}
extern "C" __declspec(dllexport) unsigned WINAPI ProbeRelease(BOOL force)
{
    if(probe && probe->deleting) {
        unsigned status=plugin.Release(nullptr,force)?1u:0u;
        if(force) { DestroyWindow(probe->window); active=nullptr; probe.reset(); }
        return status;
    }
    if(probe && (probe->copying || probe->saving || probe->overwriting)) return ProbeCopyRelease(force);
    unsigned status = plugin.Release(nullptr, force) ? 1u : 0u;
    if (force && probe)
    {
        if (!probe->worker.joinable()) status |= 32;
        if (probe->result.cancelled) status |= 64;
        if (!probe->failed && std::all_of(probe->result.rows.begin(), probe->result.rows.end(),
            [](const dirsync::Row& row) { return row.action == dirsync::Action::Blocked; })) status |= 128;
        active = nullptr; probe.reset();
    }
    return status;
}
