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
    return !probe->saving || (!probe->recoveryReport.journalError && !probe->recoveryReport.complete &&
        std::all_of(probe->recoveryReport.items.begin(),probe->recoveryReport.items.end(),
            [](const dirsync::RecoveryItem& item) { return item.logged; }));
}
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
        plugin.SaveConfiguration(nullptr,key,&registry);
        settings={}; plugin.LoadConfiguration(nullptr,key,&registry);
        check(!settings.keepOriginals && settings.recoveryRoot==root);
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
// Test-only mode bits: 1 = Verify, 2 = interrupt after one completed file, 4 = save originals.
extern "C" __declspec(dllexport) BOOL WINAPI ProbeCopyStart(const wchar_t* left,const wchar_t* right,BOOL mode)
{
    try {
        if(probe) return FALSE;
        probe=std::make_unique<Preview>(); probe->left=left; probe->right=right;
        probe->result=dirsync::Compare(left,right,probe->options,probe->cancel,probe->progress);
        if(probe->result.rows.size()!=((mode&2)?3u:2u)) { probe.reset(); return FALSE; }
        if(mode&4) {
            for(auto& row:probe->result.rows) row.action=dirsync::Action::CopyRight;
            probe->recoveryRoot=probe->left.substr(0,probe->left.find_last_of(L'\\'))+L"\\recovery";
        }
        struct Template { DLGTEMPLATE dialog; WORD menu=0,cls=0,title=0; } dialog{};
        dialog.dialog.style=WS_POPUP; dialog.dialog.cx=10; dialog.dialog.cy=10;
        probe->window=CreateDialogIndirectParamW(GetModuleHandleW(nullptr),&dialog.dialog,nullptr,EmptyDialog,0);
        if(!probe->window) { probe.reset(); return FALSE; }
        SetWindowLongPtrW(probe->window,DWLP_USER,reinterpret_cast<LONG_PTR>(probe.get()));
        copyBoundary=false; finished=false; active=probe.get(); probe->saving=(mode&4)!=0; probe->copying=!probe->saving; probe->running=true;
        probe->worker=std::thread([mode] {
            try {
                dirsync::SetCopyHook([mode,stage=std::wstring{},copiedBase=uint64_t{0},verifiedBase=uint64_t{0}]
                    (dirsync::CopyPhase phase,const std::wstring& path) mutable {
                    if(phase==dirsync::CopyPhase::Copying && stage!=path) {
                        stage=path; copiedBase=probe->copyProgress.copied.load(); verifiedBase=probe->copyProgress.verified.load();
                    }
                    if((mode&2) && !probe->copyProgress.completed) return;
                    bool reached=(mode&1) ? phase==dirsync::CopyPhase::Verifying && probe->copyProgress.verified>verifiedBase :
                        phase==dirsync::CopyPhase::Copying && probe->copyProgress.copied>copiedBase;
                    if(!reached || copyBoundary.exchange(true)) return;
                    ULONGLONG deadline=GetTickCount64()+5000;
                    while(!probe->cancel && GetTickCount64()<deadline) Sleep(1);
                });
                std::vector<size_t> rows{0,1}; if(mode&2) rows.push_back(2);
                if(mode&4) {
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
extern "C" __declspec(dllexport) unsigned WINAPI ProbeCopyRelease(BOOL force)
{
    if(!probe) return plugin.Release(nullptr,force)?1u:0u;
    const size_t completed=static_cast<size_t>(probe->copyProgress.completed.load());
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
    if(probe->copyProgress.completed==completed) status|=16;
    if(IsWindow(probe->window)) DestroyWindow(probe->window);
    probe->window=nullptr; active=nullptr; probe.reset();
    return status;
}
extern "C" __declspec(dllexport) unsigned WINAPI ProbeCopyCancel(BOOL close)
{
    if(!probe) return 0;
    // Same handler called by the dialog message loop, on its owner thread.
    DialogBody(probe->window,close?WM_CLOSE:WM_COMMAND,close?0:IDC_STOP,0);
    unsigned status=probe->cancel?1u:0u;
    if(probe->closing==(close!=FALSE)) status|=2;
    probe->Join();
    if(!probe->worker.joinable() && finished && !probe->failed) status|=4;
    const auto& r=probe->copyReport;
    if(RecoveryLogged() && r.cancelled && r.items.size()==2 && r.items[0].status==dirsync::CopyStatus::Cancelled &&
       r.items[1].status==dirsync::CopyStatus::NotStarted) status|=8;
    if(r.items.size()==2 && !r.items[0].published && r.items[0].remainingStage.empty() && !probe->copyProgress.completed) status|=16;
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
    if(probe && (probe->copying || probe->saving)) return ProbeCopyRelease(force);
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
