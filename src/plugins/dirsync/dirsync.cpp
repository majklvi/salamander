// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// Plugin lifecycle/resources and darkmode integration follow the repository's
// Split & Combine plugin. The comparison engine is independent of the SDK.
#include "precomp.h"
#include "engine.h"
#include "copy.h"
#include "recovery.h"
#include "overwrite.h"
#include "delete.h"
#include "resource.h"
#include <thread>
#include <memory>
#include <algorithm>
#include <exception>
#include <utility>

CSalamanderDebugAbstract* SalamanderDebug = nullptr;
namespace
{
HINSTANCE instance = nullptr, language = nullptr;
CSalamanderGeneralAbstract* general = nullptr;
constexpr UINT Done = WM_APP + 41;
HBRUSH backgroundBrush = nullptr;
bool dark = false;
struct Settings {
    bool keepOriginals=true;
    std::wstring recoveryRoot;
} settings;
void LoadSettings(HKEY key,CSalamanderRegistryAbstract* registry)
{
    settings={};
    if(!key || !registry) return;
    DWORD enabled=1,bytes=0;
    if(registry->GetValue(key,"KeepOriginals",REG_DWORD,&enabled,sizeof(enabled)) && enabled<=1)
        settings.keepOriginals=enabled!=0;
    // Explicit UTF-16 binary avoids the legacy SDK's byte-string/ACP boundary.
    if(registry->GetSize(key,"RecoveryRootUtf16",REG_BINARY,bytes) && bytes>=sizeof(wchar_t) &&
        bytes<=32760*sizeof(wchar_t) && bytes%sizeof(wchar_t)==0) {
        std::vector<wchar_t> value(bytes/sizeof(wchar_t));
        if(registry->GetValue(key,"RecoveryRootUtf16",REG_BINARY,value.data(),bytes) && value.back()==0 &&
            std::find(value.begin(),value.end()-1,L'\0')==value.end()-1 &&
            WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),-1,nullptr,0,nullptr,nullptr)>0)
            settings.recoveryRoot=value.data();
    }
}
bool StoreSettings(HKEY key,CSalamanderRegistryAbstract* registry)
{
    if(!key || !registry || settings.recoveryRoot.size()>=32760) return false;
    DWORD enabled=settings.keepOriginals?1:0;
    bool mode=registry->SetValue(key,"KeepOriginals",REG_DWORD,&enabled,sizeof(enabled))!=FALSE;
    bool path=registry->SetValue(key,"RecoveryRootUtf16",REG_BINARY,settings.recoveryRoot.c_str(),
        DWORD((settings.recoveryRoot.size()+1)*sizeof(wchar_t)))!=FALSE;
    return mode && path;
}
void WINAPI SettingsCallback(BOOL load,HKEY key,CSalamanderRegistryAbstract* registry,void* param)
{
    if(load) LoadSettings(key,registry);
    else *static_cast<bool*>(param)=StoreSettings(key,registry);
}
std::wstring Text(int id)
{
    const wchar_t* value = nullptr;
    int count = LoadStringW(language, id, reinterpret_cast<LPWSTR>(&value), 0);
    return count > 0 ? std::wstring(value, count) : std::wstring();
}
void Message(HWND parent, int id)
{ DarkModeMessageBoxW(parent, Text(id).c_str(), Text(IDS_NAME).c_str(), MB_OK | MB_ICONINFORMATION); }
void PersistSettings(HWND parent)
{
    bool saved=false;
    general->CallLoadOrSaveConfiguration(FALSE,SettingsCallback,&saved);
    if(!saved) Message(parent,IDS_SETTINGS_FAILED);
}
void NotifyPath(const std::wstring& path)
{
    // Snapshot paths are wide; preserve UTF-8 at the existing SDK boundary.
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path.c_str(),-1,nullptr,0,nullptr,nullptr);
    if(n>0) {
        std::vector<char> encoded(static_cast<size_t>(n));
        if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path.c_str(),-1,encoded.data(),n,nullptr,nullptr))
            general->PostChangeOnPathNotification(encoded.data(),TRUE);
    }
}
std::wstring ControlText(HWND dialog, int id)
{
    HWND edit = GetDlgItem(dialog, id);
    int count = GetWindowTextLengthW(edit);
    std::vector<wchar_t> text(size_t(count) + 1);
    GetWindowTextW(edit, text.data(), int(text.size()));
    return text.data();
}
bool Checked(HWND dialog, int id) { return IsDlgButtonChecked(dialog, id) == BST_CHECKED; }
void ConfigureDark(HWND dialog)
{
    BOOL enabled = FALSE;
    general->GetConfigParameter(SALCFG_USEWINDOWSDARKMODE, &enabled, sizeof(enabled), nullptr);
    dark = enabled != FALSE;
    COLORREF bg = dark ? general->GetCurrentColor(SALCOL_ITEM_BK_NORMAL) : GetSysColor(COLOR_BTNFACE);
    COLORREF fg = dark ? general->GetCurrentColor(SALCOL_ITEM_FG_NORMAL) : GetSysColor(COLOR_BTNTEXT);
    if (backgroundBrush) DeleteObject(backgroundBrush);
    backgroundBrush = CreateSolidBrush(bg);
    DarkModeSetConfiguredColors(fg, bg, GetSysColor(COLOR_BTNTEXT), GetSysColor(COLOR_BTNFACE));
    DarkModeConfigureDialogColors(fg, bg, backgroundBrush);
    DarkModeSetEnabled(dark);
    if (dialog) { DarkModeApplyWindow(dialog); DarkModeRefreshTitleBar(dialog); DarkModeApplyTree(dialog); }
}
using InfoRows = std::vector<std::pair<std::wstring, std::wstring>>;
void Field(InfoRows& rows, int label, const std::wstring& value)
{ rows.push_back({Text(label), value}); }
void Field(InfoRows& rows, int label, uint64_t value)
{ Field(rows, label, std::to_wstring(value)); }
struct Information
{
    int title;
    InfoRows rows;
    std::wstring notes;
    bool confirm;
    SIZE initial{}, minimum{};
    std::vector<std::pair<HWND, RECT>> controls;
};
std::wstring EditLines(const std::wstring& text)
{
    std::wstring result;
    for (size_t i=0; i<text.size(); ++i) {
        if (text[i]==L'\n' && (i==0 || text[i-1]!=L'\r')) result+=L'\r';
        result+=text[i];
    }
    return result;
}
void InformationColumns(HWND window)
{
    HWND list=GetDlgItem(window,IDC_INFO_TABLE); RECT r{}; GetClientRect(list,&r);
    RECT units{0,0,195,0}; MapDialogRect(window,&units);
    ListView_SetColumnWidth(list,0,units.right);
    ListView_SetColumnWidth(list,1,std::max(80,int(r.right-units.right)-GetSystemMetrics(SM_CXVSCROLL)-4));
}
INT_PTR CALLBACK InformationDialog(HWND window,UINT message,WPARAM w,LPARAM l)
{
    auto* info=reinterpret_cast<Information*>(GetWindowLongPtrW(window,DWLP_USER));
    try {
        if(message==WM_INITDIALOG) {
            info=reinterpret_cast<Information*>(l);
            SetWindowLongPtrW(window,DWLP_USER,reinterpret_cast<LONG_PTR>(info));
            SetWindowTextW(window,Text(info->title).c_str());
            HWND list=GetDlgItem(window,IDC_INFO_TABLE);
            ListView_SetUnicodeFormat(list,TRUE);
            ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
            for(int i=0;i<2;++i) {
                auto title=Text(i?IDS_UI_VALUE:IDS_UI_FIELD);
                LVCOLUMNW column{}; column.mask=LVCF_TEXT|LVCF_WIDTH; column.pszText=title.data(); column.cx=180;
                SendMessageW(list,LVM_INSERTCOLUMNW,i,LPARAM(&column));
            }
            for(size_t i=0;i<info->rows.size();++i) {
                auto& row=info->rows[i]; LVITEMW item{}; item.mask=LVIF_TEXT; item.iItem=int(i); item.pszText=row.first.data();
                SendMessageW(list,LVM_INSERTITEMW,0,LPARAM(&item));
                item.iSubItem=1; item.pszText=row.second.data(); SendMessageW(list,LVM_SETITEMTEXTW,i,LPARAM(&item));
            }
            for(int id:{IDC_INFO_VALUE,IDC_INFO_NOTES}) SendDlgItemMessageW(window,id,EM_SETLIMITTEXT,0x7ffffffe,0);
            SetDlgItemTextW(window,IDC_INFO_NOTES,EditLines(info->notes).c_str());
            if(!info->confirm) {
                RECT r{};GetWindowRect(GetDlgItem(window,IDCANCEL),&r);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);
                MoveWindow(GetDlgItem(window,IDOK),r.left,r.top,r.right-r.left,r.bottom-r.top,FALSE);
                ShowWindow(GetDlgItem(window,IDCANCEL),SW_HIDE);
                SetDlgItemTextW(window,IDOK,Text(IDS_UI_SUMMARY).c_str());
                SendMessageW(window,DM_SETDEFID,IDOK,0);
            } else {
                int label=info->title==IDS_UI_DELETE_CONFIRM?IDS_UI_START_DELETE:
                    info->title==IDS_UI_OVERWRITE_CONFIRM?IDS_UI_START_OVERWRITE:
                    info->title==IDS_UI_SAVE_CONFIRM?IDS_UI_START_SAVE:IDS_UI_START_COPY;
                SetDlgItemTextW(window,IDOK,Text(label).c_str());
            }
            if(!info->rows.empty()) ListView_SetItemState(list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
            RECT client{},outer{}; GetClientRect(window,&client); GetWindowRect(window,&outer);
            info->initial={client.right,client.bottom}; info->minimum={outer.right-outer.left,outer.bottom-outer.top};
            for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
                RECT r{};GetWindowRect(child,&r);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);
                info->controls.push_back({child,r});
            }
            InformationColumns(window);
            DarkModeApplyWindow(window);DarkModeRefreshTitleBar(window);DarkModeApplyTree(window);
            return TRUE;
        }
        if(!info) return FALSE;
        LRESULT brush=0;
        if(dark && DarkModeHandleCtlColor(message,w,l,brush)) return brush;
        switch(message) {
        case WM_GETMINMAXINFO:
            if(info->minimum.cx) reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={info->minimum.cx,info->minimum.cy};
            return TRUE;
        case WM_SIZE:
            if(info->initial.cx && w!=SIZE_MINIMIZED) {
                int dx=LOWORD(l)-info->initial.cx,dy=HIWORD(l)-info->initial.cy;
                for(const auto& c:info->controls) {
                    auto r=c.second;int id=GetDlgCtrlID(c.first);
                    if(id==IDC_INFO_TABLE) {r.right+=dx;r.bottom+=dy;}
                    else {r.top+=dy;r.bottom+=dy;r.right+=dx;if(id==IDOK || id==IDCANCEL)r.left+=dx;}
                    MoveWindow(c.first,r.left,r.top,r.right-r.left,r.bottom-r.top,TRUE);
                }
                InformationColumns(window);
            }
            return TRUE;
        case WM_NOTIFY:
            if(reinterpret_cast<NMHDR*>(l)->idFrom==IDC_INFO_TABLE && reinterpret_cast<NMHDR*>(l)->code==LVN_ITEMCHANGED) {
                int index=ListView_GetNextItem(GetDlgItem(window,IDC_INFO_TABLE),-1,LVNI_SELECTED);
                SetDlgItemTextW(window,IDC_INFO_VALUE,index>=0 && size_t(index)<info->rows.size()?EditLines(info->rows[size_t(index)].second).c_str():L"");
            }
            return TRUE;
        case WM_SETTINGCHANGE: case WM_THEMECHANGED:
            DarkModeApplyTree(window);DarkModeRefreshTitleBar(window);return TRUE;
        case WM_COMMAND:
            if(LOWORD(w)==IDOK || LOWORD(w)==IDCANCEL) {EndDialog(window,LOWORD(w));return TRUE;}
            break;
        case WM_CLOSE: EndDialog(window,IDCANCEL);return TRUE;
        }
    } catch(...) {EndDialog(window,IDCANCEL);return TRUE;}
    return FALSE;
}
bool InformationBox(HWND parent,int title,InfoRows rows,const std::wstring& notes,bool confirm=false)
{
    Information info{title,std::move(rows),notes,confirm,{}, {}, {}};
    INT_PTR result=DialogBoxParamW(language,MAKEINTRESOURCEW(IDD_INFORMATION),parent,InformationDialog,LPARAM(&info));
    if(result==-1) Message(parent,IDS_FAILED);
    return result==IDOK;
}
struct Preview
{
    enum class Intent { None, Apply, Summary, Copy, Save, Overwrite, Delete };
    HWND window = nullptr;
    std::wstring left, right;
    std::wstring panelLeft, panelRight;
    dirsync::Options options;
    dirsync::Result result;
    dirsync::Result checkedResult;
    Intent intent = Intent::None;
    std::vector<size_t> requestedRows;
    int requestedAction = 0;
    std::vector<size_t> visible;
    std::atomic<bool> cancel{false};
    dirsync::Progress progress;
    dirsync::CopyProgress copyProgress;
    dirsync::CopyReport copyReport;
    dirsync::RecoveryReport recoveryReport;
    dirsync::OverwriteReport overwriteReport;
    dirsync::DeleteReport deleteReport;
    std::wstring recoveryRoot;
    std::vector<size_t> copyRows;
    bool copying = false, hasCopyReport = false;
    bool saving = false, hasRecoveryReport = false;
    bool overwriting=false,hasOverwriteReport=false;
    bool deleting=false,hasDeleteReport=false;
    uint64_t transferCount=0;
    std::thread worker;
    SIZE initialClient{}, minimumWindow{};
    std::vector<std::pair<HWND, RECT>> controls;
    bool running = false, closing = false, failed = false, validating = false;
    ~Preview() { Stop(); Join(); }
    void Stop()
    {
        cancel = true;
        if (worker.joinable()) CancelSynchronousIo(worker.native_handle());
    }
    void Join() { if (worker.joinable()) worker.join(); }
};
Preview* active = nullptr;
std::wstring Number(uint64_t n) { return std::to_wstring(n); }
std::wstring Modified(uint64_t ticks)
{
    FILETIME utc{DWORD(ticks), DWORD(ticks >> 32)}, local{};
    SYSTEMTIME time{};
    if (!FileTimeToLocalFileTime(&utc, &local) || !FileTimeToSystemTime(&local, &time)) return L"";
    wchar_t date[80]{}, clock[80]{};
    GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &time, nullptr, date, 80, nullptr);
    GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &time, nullptr, clock, 80);
    return std::wstring(date) + L" " + clock;
}
std::wstring Cell(const dirsync::Row& row, int column)
{
    switch (column)
    {
    case 0: return row.path + (((row.hasLeft && row.left.directory) || (row.hasRight && row.right.directory)) ? L"\\" : L"");
    case 1: return row.hasLeft && !row.left.directory ? Number(row.left.size) : L"";
    case 2: return row.hasLeft ? Modified(row.left.time) : L"";
    case 3: return Text(IDS_ACTION + int(row.action));
    case 4: return row.hasRight && !row.right.directory ? Number(row.right.size) : L"";
    case 5: return row.hasRight ? Modified(row.right.time) : L"";
    case 6: return Text(IDS_STATE + int(row.state));
    case 7: return Text(IDS_REASON + int(row.reason));
    case 8: return row.error ? Number(row.error) : L"";
    }
    return L"";
}
void StatusPane(HWND window,int part,const std::wstring& text)
{
    SendDlgItemMessageW(window,IDC_STATUS,SB_SETTEXTW,part,LPARAM(text.c_str()));
    SendDlgItemMessageW(window,IDC_STATUS,SB_SETTIPTEXTW,part,LPARAM(text.c_str()));
}
void SizeStatus(HWND window)
{
    HWND bar=GetDlgItem(window,IDC_STATUS);
    SendMessageW(bar,WM_SIZE,0,0);
    RECT client{};GetClientRect(window,&client);
    int parts[]={client.right*44/100,-1};
    SendMessageW(bar,SB_SETPARTS,2,LPARAM(parts));
}
void Status(Preview& p)
{
    std::wstring state; wchar_t counts[512]{};
    if(p.running) {
        if(p.copying || p.saving || p.overwriting || p.deleting) {
            state=Text(p.deleting?IDS_UI_OP_DELETE:p.overwriting?IDS_UI_OP_OVERWRITE:p.saving?IDS_UI_OP_SAVE:IDS_UI_OP_COPY);
            wchar_t item[128]{};uint64_t total=(p.overwriting || p.deleting)?p.transferCount:uint64_t(p.copyRows.size());
            swprintf_s(item,Text(IDS_UI_TRANSFER).c_str(),std::min(p.copyProgress.item.load()+1,total),total);
            state+=L" | "+std::wstring(item);
            swprintf_s(counts,Text(IDS_UI_TRANSFER_STATS).c_str(),p.copyProgress.copied.load(),p.copyProgress.verified.load(),p.copyProgress.completed.load());
        } else {
            uint64_t bytes=p.progress.readBytes.load();
            state=Text(p.validating?IDS_UI_VALIDATE:bytes?IDS_UI_COMPARE:IDS_UI_SCAN);
            if(bytes) swprintf_s(counts,Text(IDS_UI_READ_STATS).c_str(),bytes,p.progress.scanned.load(),p.progress.compared.load());
            else swprintf_s(counts,Text(IDS_UI_SCAN_STATS).c_str(),p.progress.scanned.load(),p.progress.compared.load());
        }
        if(p.cancel) state=Text(IDS_UI_STOPPING);
    } else {
        if(p.hasCopyReport || p.hasRecoveryReport || p.hasOverwriteReport || p.hasDeleteReport) state=Text(IDS_UI_FINISHED);
        else if(p.result.stale && !p.result.cancelled) state=Text(IDS_STALE);
        else if(p.failed || p.result.cancelled) state=Text(p.failed?IDS_FAILED:IDS_CANCELLED);
        else state=Text(p.result.snapshot?IDS_UI_COMPLETE:IDS_UI_READY);
        uint64_t blocked=0;
        for(const auto& row:p.result.rows) if(row.state==dirsync::State::Blocked || row.state==dirsync::State::Conflict) ++blocked;
        swprintf_s(counts,Text(IDS_UI_ROWS).c_str(),uint64_t(p.result.rows.size()),uint64_t(p.visible.size()),uint64_t(p.result.rows.size()-p.visible.size()),blocked);
    }
    StatusPane(p.window,0,state);StatusPane(p.window,1,counts);
}
void Filter(Preview& p)
{
    p.visible.clear();
    for (size_t i = 0; i < p.result.rows.size(); ++i)
    {
        const auto& row = p.result.rows[i];
        bool show = true;
        if (row.state != dirsync::State::Blocked && row.state != dirsync::State::Conflict)
        {
            if (row.state == dirsync::State::Equal) show = Checked(p.window, IDC_EQUAL);
            else if (row.state == dirsync::State::LeftOnly || row.state == dirsync::State::RightOnly) show = Checked(p.window, IDC_SINGLE);
            else show = Checked(p.window, IDC_DIFFERENT);
            if (row.action == dirsync::Action::CopyRight) show &= Checked(p.window, IDC_TORIGHT);
            if (row.action == dirsync::Action::CopyLeft) show &= Checked(p.window, IDC_TOLEFT);
        }
        if (show) p.visible.push_back(i);
    }
    ListView_SetItemCountEx(GetDlgItem(p.window, IDC_LIST), int(p.visible.size()), LVSICF_NOSCROLL);
    InvalidateRect(GetDlgItem(p.window, IDC_LIST), nullptr, TRUE);
    Status(p);
}
void Busy(Preview& p, bool busy)
{
    for (int id : {IDC_MASK, IDC_RECURSIVE, IDC_CONTENT, IDC_IGNORE, IDC_ASYMMETRIC, IDC_SELECTED,
                   IDC_EMPTY, IDC_COMPARE, IDC_ACTION, IDC_APPLY, IDC_SUMMARY, IDC_EQUAL,
                   IDC_DIFFERENT, IDC_SINGLE, IDC_TORIGHT, IDC_TOLEFT, IDC_COPY_NEW, IDC_KEEP_ORIGINALS, IDC_OVERWRITE, IDC_DELETE_FILES})
        EnableWindow(GetDlgItem(p.window, id), !busy);
    for(int id:{IDC_RECOVERY_ROOT,IDC_RECOVERY_LABEL,IDC_SAVE_ORIGINALS})
        EnableWindow(GetDlgItem(p.window,id),!busy && settings.keepOriginals);
    SetDlgItemTextW(p.window,IDC_RECOVERY_SPACE,Text(settings.keepOriginals?IDS_RECOVERY_SPACE_ON:IDS_RECOVERY_SPACE_OFF).c_str());
    EnableWindow(GetDlgItem(p.window, IDC_SELECTED), !busy && !p.options.selection.empty());
    bool locked = Checked(p.window, IDC_SELECTED);
    EnableWindow(GetDlgItem(p.window, IDC_LEFT), !busy && !locked);
    EnableWindow(GetDlgItem(p.window, IDC_RIGHT), !busy && !locked);
    EnableWindow(GetDlgItem(p.window, IDC_STOP), busy);
}
void InvalidatePlan(Preview& p)
{
    if (p.running) return;
    p.result = {}; p.visible.clear(); p.failed = false;
    p.hasCopyReport = false;
    p.hasRecoveryReport = false;
    p.hasOverwriteReport = false;
    p.hasDeleteReport = false;
    ListView_SetItemCount(GetDlgItem(p.window, IDC_LIST), 0);
    Status(p);
}
void Start(Preview& p)
{
    if (p.running) return;
    p.left = ControlText(p.window, IDC_LEFT); p.right = ControlText(p.window, IDC_RIGHT);
    p.options.mask = ControlText(p.window, IDC_MASK);
    p.options.recursive = Checked(p.window, IDC_RECURSIVE);
    p.options.content = Checked(p.window, IDC_CONTENT);
    p.options.ignoreDate = Checked(p.window, IDC_IGNORE);
    p.options.asymmetric = Checked(p.window, IDC_ASYMMETRIC);
    p.options.emptyDirectories = Checked(p.window, IDC_EMPTY);
    p.options.onlySelected = Checked(p.window, IDC_SELECTED);
    InvalidatePlan(p);
    p.validating = false; p.intent = Preview::Intent::None;
    p.cancel = false; p.progress.scanned = 0; p.progress.compared = 0; p.progress.readBytes = 0;
    p.running = true; Busy(p, true);
    // Disabling the focused input/Compare otherwise leaves the modal dialog
    // without a keyboard target. Keep cancellation reachable during work.
    SendMessageW(p.window, WM_NEXTDLGCTL, WPARAM(GetDlgItem(p.window, IDC_STOP)), TRUE);
    try
    {
        p.worker = std::thread([&p]() {
            try { p.result = dirsync::Compare(p.left, p.right, p.options, p.cancel, p.progress); }
            catch (...) { p.failed = true; p.result = {}; }
            PostMessageW(p.window, Done, 0, 0);
        });
        SetTimer(p.window, 1, 150, nullptr);
    }
    catch (...)
    {
        p.failed = true; p.running = false; Busy(p, false); Status(p);
        SendMessageW(p.window, WM_NEXTDLGCTL, WPARAM(GetDlgItem(p.window, IDC_COMPARE)), TRUE);
    }
}
void ValidatePreview(Preview& p, Preview::Intent intent)
{
    if (p.running) return;
    if (!p.result.snapshot || p.result.cancelled || p.result.stale || p.failed)
    { Message(p.window, IDS_STALE); return; }
    p.requestedRows.clear();
    p.requestedAction = int(SendDlgItemMessageW(p.window, IDC_ACTION, CB_GETCURSEL, 0, 0));
    HWND list = GetDlgItem(p.window, IDC_LIST);
    for (int i = ListView_GetNextItem(list, -1, LVNI_SELECTED); i >= 0; i = ListView_GetNextItem(list, i, LVNI_SELECTED))
        p.requestedRows.push_back(p.visible[size_t(i)]);
    p.cancel = false; p.progress.scanned = 0; p.progress.compared = 0; p.progress.readBytes = 0;
    p.validating = true; p.intent = intent; p.running = true; p.checkedResult = {};
    Busy(p, true);
    SendMessageW(p.window, WM_NEXTDLGCTL, WPARAM(GetDlgItem(p.window, IDC_STOP)), TRUE);
    try
    {
        p.worker = std::thread([&p]() {
            // Existing rows stay readable by LVN_GETDISPINFO on the UI thread.
            // Work on a separate copy; publish only after Join in Done.
            try { p.checkedResult = p.result; dirsync::Revalidate(p.checkedResult, p.cancel, p.progress); }
            catch (...) { p.failed = true; p.checkedResult = {}; }
            PostMessageW(p.window, Done, 0, 0);
        });
        SetTimer(p.window, 1, 150, nullptr);
    }
    catch (...)
    {
        p.failed = true; p.running = false; p.validating = false; p.intent = Preview::Intent::None;
        Busy(p, false); Status(p);
        SendMessageW(p.window, WM_NEXTDLGCTL, WPARAM(GetDlgItem(p.window, IDC_COMPARE)), TRUE);
    }
}
void Apply(Preview& p)
{
    int value = p.requestedAction;
    if (value < 0 || value > 4) return;
    auto action = static_cast<dirsync::Action>(value);
    bool found = false;
    for (size_t index : p.requestedRows)
    {
        found = true;
        auto& row = p.result.rows[index];
        if (row.state == dirsync::State::Blocked || row.state == dirsync::State::Conflict) continue;
        if (action == dirsync::Action::CopyRight && !row.hasLeft) continue;
        if (action == dirsync::Action::CopyLeft && (!row.hasRight || p.options.asymmetric)) continue;
        if (action == dirsync::Action::DeleteRight && (!p.options.asymmetric || !row.hasRight || row.hasLeft)) continue;
        row.action = action;
    }
    if (!found) Message(p.window, IDS_SELECT_ROW);
    Filter(p);
}
void Summary(Preview& p)
{
    uint64_t nr = 0, nl = 0, br = 0, bl = 0, deletes = 0, unresolved = 0, blocked = 0;
    for (const auto& row : p.result.rows) if (row.state == dirsync::State::Blocked || row.state == dirsync::State::Conflict) ++blocked;
    for (size_t index : p.visible)
    {
        const auto& row = p.result.rows[index];
        if (row.action == dirsync::Action::CopyRight) { ++nr; br += row.left.size; }
        if (row.action == dirsync::Action::CopyLeft) { ++nl; bl += row.right.size; }
        if (row.action == dirsync::Action::DeleteRight) ++deletes;
        if (row.action == dirsync::Action::Decide) ++unresolved;
    }
    InfoRows rows;
    Field(rows,IDS_UI_TO_RIGHT,Number(nr)+L" / "+Number(br));
    Field(rows,IDS_UI_TO_LEFT,Number(nl)+L" / "+Number(bl));
    Field(rows,IDS_UI_DELETE_RIGHT,deletes);Field(rows,IDS_UI_UNRESOLVED,unresolved);
    Field(rows,IDS_UI_BLOCKED,blocked);Field(rows,IDS_UI_HIDDEN,uint64_t(p.result.rows.size()-p.visible.size()));
    InformationBox(p.window,IDS_UI_PLAN,std::move(rows),Text(IDS_UI_PLAN_NOTE));
}
void CopySummary(Preview& p)
{
    uint64_t copied=0, failed=0, pending=0; std::wstring details;
    for (const auto& item:p.copyReport.items) {
        if (item.status==dirsync::CopyStatus::Copied) ++copied;
        else if (item.status==dirsync::CopyStatus::NotStarted) ++pending;
        else {
            ++failed;
            if (details.empty()) details=L"\n\n"+item.target+L"\n"+Text(IDS_COPY_ERROR)+Number(item.error)+
                (item.published ? L"\n"+Text(IDS_COPY_PUBLISHED_ERROR):L"");
        }
        if (!item.remainingStage.empty()) details+=L"\n"+Text(IDS_COPY_REMAINDER)+item.remainingStage;
    }
    InfoRows rows;
    Field(rows,IDS_UI_COPIED,copied);Field(rows,IDS_UI_ERRORS,failed);Field(rows,IDS_UI_PENDING,pending);
    Field(rows,IDS_UI_OMITTED,uint64_t(p.copyReport.omitted));
    std::wstring message=Text(IDS_UI_RESULT_NOTE);
    if (p.copyReport.stale) message+=L"\n"+Text(IDS_STALE);
    if (p.copyReport.cancelled) message+=L"\n"+Text(IDS_COPY_STOPPED);
    if(!details.empty()) Field(rows,IDS_UI_ERROR_DETAILS,details);
    InformationBox(p.window,IDS_UI_COPY_RESULT,std::move(rows),message);
}
void CopyNew(Preview& p)
{
    p.copyRows.clear(); uint64_t bytes=0,omitted=0;
    for (size_t i:p.visible) {
        const auto& r=p.result.rows[i];
        if (dirsync::IsNewFileCopy(r)) { p.copyRows.push_back(i); bytes+=(r.action==dirsync::Action::CopyRight?r.left:r.right).size; }
        else if (r.action!=dirsync::Action::None) ++omitted;
    }
    if (p.copyRows.empty()) { Message(p.window,IDS_COPY_EMPTY); return; }
    InfoRows fields;Field(fields,IDS_UI_FILES,uint64_t(p.copyRows.size()));Field(fields,IDS_UI_BYTES,bytes);Field(fields,IDS_UI_OMITTED,omitted);
    if(!InformationBox(p.window,IDS_UI_COPY_CONFIRM,std::move(fields),Text(IDS_UI_COPY_POLICY),true)) return;
    p.cancel=false; p.failed=false; p.copying=true; p.running=true; p.hasCopyReport=false;
    p.copyProgress.copied=0; p.copyProgress.verified=0; p.copyProgress.completed=0; p.copyProgress.item=0;
    p.copyProgress.phase=dirsync::CopyPhase::Checking; p.copyReport={};
    Busy(p,true); SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,IDC_STOP)),TRUE);
    try {
        // Freeze all visible rows, including omitted actions for an accurate report.
        const auto rows=p.visible;
        p.worker=std::thread([&p,rows]() {
            try { p.copyReport=dirsync::CopyNewFiles(p.result,rows,p.cancel,p.progress,p.copyProgress); }
            catch (...) { p.failed=true; }
            PostMessageW(p.window,Done,0,0);
        });
        SetTimer(p.window,1,150,nullptr);
    } catch (...) {
        p.failed=true; p.running=false; p.copying=false; Busy(p,false); Status(p);
        SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,IDC_COMPARE)),TRUE);
    }
}
void RecoverySummary(Preview& p)
{
    uint64_t copied=0,failed=0,pending=0,logged=0; std::wstring details;
    for (const auto& item:p.recoveryReport.items) {
        if (item.copy.status==dirsync::CopyStatus::Copied) ++copied;
        else if (item.copy.status==dirsync::CopyStatus::NotStarted) ++pending;
        else ++failed;
        if (item.logged) ++logged;
        if (details.empty() && item.copy.error)
            details=L"\n\n"+item.original+L"\n"+item.saved+L"\n"+Text(IDS_COPY_ERROR)+Number(item.copy.error)+
                (item.copy.published?L"\n"+Text(IDS_SAVE_PUBLISHED_ERROR):L"");
        if (!item.copy.remainingStage.empty()) details+=L"\n"+Text(IDS_COPY_REMAINDER)+item.copy.remainingStage;
    }
    InfoRows rows;Field(rows,IDS_UI_COPIED,copied);Field(rows,IDS_UI_ERRORS,failed);Field(rows,IDS_UI_PENDING,pending);
    Field(rows,IDS_UI_LOGGED,logged);Field(rows,IDS_UI_OMITTED,uint64_t(p.recoveryReport.omitted));
    Field(rows,IDS_UI_MANIFEST,p.recoveryReport.journal);
    std::wstring message=Text(IDS_UI_RESULT_NOTE)+L"\n\n"+Text(IDS_UI_SAVE_POLICY);
    if (!p.recoveryReport.complete) message+=L"\n"+Text(IDS_SAVE_INCOMPLETE);
    if (p.recoveryReport.error) message+=L"\n"+Text(IDS_COPY_ERROR)+Number(p.recoveryReport.error);
    if (p.recoveryReport.stale) message+=L"\n"+Text(IDS_STALE);
    if(!details.empty()) Field(rows,IDS_UI_ERROR_DETAILS,details);
    InformationBox(p.window,IDS_UI_SAVE_RESULT,std::move(rows),message);
}
void SaveOriginals(Preview& p)
{
    if(!settings.keepOriginals) { Message(p.window,IDS_RECOVERY_DISABLED); return; }
    p.recoveryRoot=ControlText(p.window,IDC_RECOVERY_ROOT);
    if (p.recoveryRoot.empty()) { Message(p.window,IDS_SAVE_ROOT); return; }
    p.copyRows.clear(); uint64_t bytes=0,omitted=0;
    for (size_t i:p.visible) {
        const auto& r=p.result.rows[i];
        if (dirsync::IsOriginalToSave(r)) { p.copyRows.push_back(i); bytes+=(r.action==dirsync::Action::CopyRight?r.right:r.left).size; }
        else if (r.action!=dirsync::Action::None) ++omitted;
    }
    if (p.copyRows.empty()) { Message(p.window,IDS_SAVE_EMPTY); return; }
    InfoRows fields;Field(fields,IDS_UI_FILES,uint64_t(p.copyRows.size()));Field(fields,IDS_UI_ORIGINAL_BYTES,bytes);Field(fields,IDS_UI_OMITTED,omitted);
    Field(fields,IDS_UI_RECOVERY_FOLDER,p.recoveryRoot);
    if(!InformationBox(p.window,IDS_UI_SAVE_CONFIRM,std::move(fields),Text(IDS_UI_SAVE_POLICY),true)) return;
    p.cancel=false; p.failed=false; p.saving=true; p.running=true; p.hasCopyReport=false; p.hasRecoveryReport=false;
    p.copyProgress.copied=0; p.copyProgress.verified=0; p.copyProgress.completed=0; p.copyProgress.item=0;
    p.copyProgress.phase=dirsync::CopyPhase::Checking; p.recoveryReport={};
    Busy(p,true); SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,IDC_STOP)),TRUE);
    try {
        const auto rows=p.visible; const auto root=p.recoveryRoot;
        p.worker=std::thread([&p,rows,root]() {
            try { p.recoveryReport=dirsync::SaveOriginals(p.result,rows,root,p.cancel,p.progress,p.copyProgress); }
            catch (...) { p.failed=true; }
            PostMessageW(p.window,Done,0,0);
        });
        SetTimer(p.window,1,150,nullptr);
    } catch (...) {
        p.failed=true; p.running=false; p.saving=false; Busy(p,false); Status(p);
        SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,IDC_COMPARE)),TRUE);
    }
}
void OverwriteSummary(Preview& p)
{
    const auto& report=p.overwriteReport;
    uint64_t copied=0,failed=0,pending=0,recovered=0; std::wstring details;
    for(const auto& item:report.items) {
        if(item.status==dirsync::CopyStatus::Copied) ++copied;
        else if(item.status==dirsync::CopyStatus::NotStarted) ++pending;
        else ++failed;
        if(details.empty() && item.error) details=L"\n\n"+item.target+L"\n"+Text(IDS_COPY_ERROR)+Number(item.error)+
            (item.published?L"\n"+Text(IDS_OVERWRITE_PUBLISHED_ERROR):L"");
        if(!item.remainingStage.empty()) details+=L"\n"+Text(IDS_COPY_REMAINDER)+item.remainingStage;
    }
    for(const auto& item:report.recovery.items) {
        if(item.copy.status==dirsync::CopyStatus::Copied && item.logged) ++recovered;
        if(item.copy.error) {
            details+=L"\n\n"+Text(IDS_OVERWRITE_RECOVERY_ERROR)+L"\n"+item.original+L"\n"+Text(IDS_COPY_ERROR)+Number(item.copy.error);
            if(!item.copy.remainingStage.empty()) details+=L"\n"+Text(IDS_COPY_REMAINDER)+item.copy.remainingStage;
            break;
        }
    }
    InfoRows rows;Field(rows,IDS_UI_REPLACED,copied);Field(rows,IDS_UI_ERRORS,failed);Field(rows,IDS_UI_PENDING,pending);
    Field(rows,IDS_UI_OMITTED,uint64_t(report.omitted));Field(rows,IDS_UI_RECOVERY,Text(report.keepOriginals?IDS_UI_ON:IDS_UI_OFF));
    auto message=Text(IDS_UI_RESULT_NOTE)+L"\n\n"+Text(report.keepOriginals?IDS_OVERWRITE_ON:IDS_OVERWRITE_OFF);
    if(report.keepOriginals) {
        Field(rows,IDS_UI_RECOVERED,Number(recovered)+L" / "+Number(uint64_t(report.items.size())));
        Field(rows,IDS_UI_MANIFEST,report.recovery.journal);Field(rows,IDS_UI_JOURNAL,report.journal);
    }
    if(!report.complete) message+=L"\n"+Text(IDS_OVERWRITE_INCOMPLETE);
    if(report.error) message+=L"\n"+Text(IDS_COPY_ERROR)+Number(report.error);
    if(report.stale) message+=L"\n"+Text(IDS_STALE);
    if(!details.empty()) Field(rows,IDS_UI_ERROR_DETAILS,details);
    InformationBox(p.window,IDS_UI_OVERWRITE_RESULT,std::move(rows),message);
}
void Overwrite(Preview& p)
{
    p.copyRows.clear(); uint64_t bytes=0,originalBytes=0,omitted=0;
    for(size_t i:p.visible) {
        const auto& row=p.result.rows[i];
        if(dirsync::IsFileOverwrite(row)) {
            p.copyRows.push_back(i); bool toRight=row.action==dirsync::Action::CopyRight;
            bytes+=(toRight?row.left:row.right).size; originalBytes+=(toRight?row.right:row.left).size;
        } else if(row.action!=dirsync::Action::None) ++omitted;
    }
    if(p.copyRows.empty()) { Message(p.window,IDS_OVERWRITE_EMPTY); return; }
    const bool keep=settings.keepOriginals; const auto root=ControlText(p.window,IDC_RECOVERY_ROOT);
    if(keep && root.empty()) { Message(p.window,IDS_SAVE_ROOT); return; }
    InfoRows fields;Field(fields,IDS_UI_FILES,uint64_t(p.copyRows.size()));Field(fields,IDS_UI_BYTES,bytes);
    Field(fields,IDS_UI_ORIGINAL_BYTES,originalBytes);Field(fields,IDS_UI_OMITTED,omitted);
    Field(fields,IDS_UI_RECOVERY,Text(keep?IDS_UI_ON:IDS_UI_OFF));if(keep)Field(fields,IDS_UI_RECOVERY_FOLDER,root);
    auto message=Text(IDS_UI_OVERWRITE_POLICY)+L"\n\n"+Text(keep?IDS_OVERWRITE_ON:IDS_OVERWRITE_OFF);
    if(!InformationBox(p.window,IDS_UI_OVERWRITE_CONFIRM,std::move(fields),message,true)) return;
    p.cancel=false; p.failed=false; p.overwriting=true; p.running=true;
    p.hasCopyReport=p.hasRecoveryReport=p.hasOverwriteReport=p.hasDeleteReport=false; p.overwriteReport={};
    p.transferCount=uint64_t(p.copyRows.size())*(keep?2:1);
    p.copyProgress.copied=0; p.copyProgress.verified=0; p.copyProgress.completed=0; p.copyProgress.item=0;
    p.copyProgress.phase=dirsync::CopyPhase::Checking;
    Busy(p,true); SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,IDC_STOP)),TRUE);
    try {
        const auto rows=p.visible;
        p.worker=std::thread([&p,rows,keep,root]() {
            try { p.overwriteReport=dirsync::OverwriteFiles(p.result,rows,keep,root,p.cancel,p.progress,p.copyProgress); }
            catch(...) { p.failed=true; }
            PostMessageW(p.window,Done,0,0);
        });
        SetTimer(p.window,1,150,nullptr);
    } catch(...) {
        p.failed=true; p.running=false; p.overwriting=false; Busy(p,false); Status(p);
        SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,IDC_COMPARE)),TRUE);
    }
}
void DeleteSummary(Preview& p)
{
    const auto& report=p.deleteReport;
    uint64_t deleted=0,failed=0,pending=0,recovered=0; std::wstring details;
    for(const auto& item:report.items) {
        if(item.status==dirsync::DeleteStatus::Deleted) ++deleted;
        else if(item.status==dirsync::DeleteStatus::NotStarted) ++pending;
        else ++failed;
        if(details.empty() && item.error) details=L"\n\n"+item.target+L"\n"+Text(IDS_COPY_ERROR)+Number(item.error);
    }
    for(const auto& item:report.recovery.items) {
        if(item.copy.status==dirsync::CopyStatus::Copied && item.logged) ++recovered;
        if(item.copy.error) {
            details+=L"\n\n"+Text(IDS_DELETE_RECOVERY_ERROR)+L"\n"+item.original+L"\n"+Text(IDS_COPY_ERROR)+Number(item.copy.error);
            if(!item.copy.remainingStage.empty()) details+=L"\n"+Text(IDS_COPY_REMAINDER)+item.copy.remainingStage;
            break;
        }
    }
    InfoRows rows;Field(rows,IDS_UI_DELETED,deleted);Field(rows,IDS_UI_ERRORS,failed);Field(rows,IDS_UI_PENDING,pending);
    Field(rows,IDS_UI_OMITTED,uint64_t(report.omitted));Field(rows,IDS_UI_RECOVERY,Text(report.keepOriginals?IDS_UI_ON:IDS_UI_OFF));
    auto message=Text(IDS_UI_RESULT_NOTE)+L"\n\n"+Text(report.keepOriginals?IDS_DELETE_ON:IDS_DELETE_OFF);
    if(report.keepOriginals) {
        Field(rows,IDS_UI_RECOVERED,Number(recovered)+L" / "+Number(uint64_t(report.items.size())));
        Field(rows,IDS_UI_MANIFEST,report.recovery.journal);Field(rows,IDS_UI_JOURNAL,report.journal);
    }
    if(!report.complete) message+=L"\n"+Text(IDS_DELETE_INCOMPLETE);
    if(report.error) message+=L"\n"+Text(IDS_COPY_ERROR)+Number(report.error);
    if(report.stale) message+=L"\n"+Text(IDS_STALE);
    if(!details.empty()) Field(rows,IDS_UI_ERROR_DETAILS,details);
    InformationBox(p.window,IDS_UI_DELETE_RESULT,std::move(rows),message);
}
void DeleteFiles(Preview& p)
{
    p.copyRows.clear(); uint64_t bytes=0,omitted=0;
    if(!dirsync::SnapshotAsymmetric(p.result)) { Message(p.window,IDS_DELETE_EMPTY); return; }
    for(size_t i:p.visible) {
        const auto& row=p.result.rows[i];
        if(dirsync::IsFileDeletion(row)) { p.copyRows.push_back(i); bytes+=row.right.size; }
        else if(row.action!=dirsync::Action::None) ++omitted;
    }
    if(p.copyRows.empty()) { Message(p.window,IDS_DELETE_EMPTY); return; }
    const bool keep=settings.keepOriginals; const auto root=ControlText(p.window,IDC_RECOVERY_ROOT);
    if(keep && root.empty()) { Message(p.window,IDS_SAVE_ROOT); return; }
    InfoRows fields;Field(fields,IDS_UI_FILES,uint64_t(p.copyRows.size()));Field(fields,IDS_UI_ORIGINAL_BYTES,bytes);
    Field(fields,IDS_UI_OMITTED,omitted);Field(fields,IDS_UI_RECOVERY,Text(keep?IDS_UI_ON:IDS_UI_OFF));
    if(keep)Field(fields,IDS_UI_RECOVERY_FOLDER,root);
    auto message=Text(IDS_UI_DELETE_POLICY)+L"\n\n"+Text(keep?IDS_DELETE_ON:IDS_DELETE_OFF);
    if(!InformationBox(p.window,IDS_UI_DELETE_CONFIRM,std::move(fields),message,true)) return;
    p.cancel=false; p.failed=false; p.deleting=true; p.running=true;
    p.hasCopyReport=p.hasRecoveryReport=p.hasOverwriteReport=p.hasDeleteReport=false; p.deleteReport={};
    p.transferCount=uint64_t(p.copyRows.size())*(keep?2:1);
    p.copyProgress.copied=0; p.copyProgress.verified=0; p.copyProgress.completed=0; p.copyProgress.item=0;
    p.copyProgress.phase=dirsync::CopyPhase::Checking;
    Busy(p,true); SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,IDC_STOP)),TRUE);
    try {
        const auto rows=p.visible;
        p.worker=std::thread([&p,rows,keep,root]() {
            try { p.deleteReport=dirsync::DeleteFiles(p.result,rows,keep,root,p.cancel,p.progress,p.copyProgress); }
            catch(...) { p.failed=true; }
            PostMessageW(p.window,Done,0,0);
        });
        SetTimer(p.window,1,150,nullptr);
    } catch(...) {
        p.failed=true; p.running=false; p.deleting=false; Busy(p,false); Status(p);
        SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,IDC_COMPARE)),TRUE);
    }
}
INT_PTR CALLBACK DialogBody(HWND window, UINT message, WPARAM w, LPARAM l)
{
    Preview* p = reinterpret_cast<Preview*>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG)
    {
        p = reinterpret_cast<Preview*>(l); p->window = window;
        SetWindowLongPtrW(window, DWLP_USER, reinterpret_cast<LONG_PTR>(p));
        SetDlgItemTextW(window, IDC_LEFT, p->left.c_str()); SetDlgItemTextW(window, IDC_RIGHT, p->right.c_str());
        SendDlgItemMessageW(window,IDC_RECOVERY_ROOT,EM_SETLIMITTEXT,32759,0);
        SetDlgItemTextW(window,IDC_RECOVERY_ROOT,settings.recoveryRoot.c_str());
        CheckDlgButton(window,IDC_KEEP_ORIGINALS,settings.keepOriginals?BST_CHECKED:BST_UNCHECKED);
        SetDlgItemTextW(window, IDC_MASK, L"*.*");
        CheckDlgButton(window, IDC_RECURSIVE, BST_CHECKED);
        CheckDlgButton(window, IDC_SELECTED, p->options.selection.empty() ? BST_UNCHECKED : BST_CHECKED);
        for (int id : {IDC_EQUAL, IDC_DIFFERENT, IDC_SINGLE, IDC_TORIGHT, IDC_TOLEFT}) CheckDlgButton(window, id, BST_CHECKED);
        HWND list = GetDlgItem(window, IDC_LIST);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
        int widths[] = {240, 95, 150, 120, 95, 150, 95, 270, 70};
        HDC dc = GetDC(window);
        UINT dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(window, dc);
        ListView_SetUnicodeFormat(list, TRUE);
        // Column captions and actions always come from the language module.
        for (int i = 0; i < 9; ++i)
        { std::wstring title = Text(IDS_COLUMNS + i); LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = &title[0]; c.cx = MulDiv(widths[i], dpi, 96); SendMessageW(list, LVM_INSERTCOLUMNW, i, LPARAM(&c)); }
        for (int i = 0; i < 5; ++i) SendDlgItemMessageW(window, IDC_ACTION, CB_ADDSTRING, 0, LPARAM(Text(IDS_ACTION + i).c_str()));
        SendDlgItemMessageW(window, IDC_ACTION, CB_SETCURSEL, 0, 0);
        RECT client{}, outer{}; GetClientRect(window, &client); GetWindowRect(window, &outer);
        p->initialClient = {client.right, client.bottom};
        p->minimumWindow = {outer.right - outer.left, outer.bottom - outer.top};
        for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        {
            if(GetDlgCtrlID(child)==IDC_STATUS) continue;
            RECT bounds{}; GetWindowRect(child, &bounds); MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&bounds), 2);
            p->controls.push_back({child, bounds});
        }
        ConfigureDark(window); SizeStatus(window); Busy(*p, false); Status(*p); return TRUE;
    }
    if (!p) return FALSE;
    LRESULT brush = 0;
    if (dark && DarkModeHandleCtlColor(message, w, l, brush)) return brush;
    switch (message)
    {
    case WM_GETMINMAXINFO:
        if (p->minimumWindow.cx)
        { auto* limits = reinterpret_cast<MINMAXINFO*>(l); limits->ptMinTrackSize = {p->minimumWindow.cx, p->minimumWindow.cy}; }
        return TRUE;
    case WM_SIZE:
        if (p->initialClient.cx && w != SIZE_MINIMIZED)
        {
            int dx = LOWORD(l) - p->initialClient.cx, dy = HIWORD(l) - p->initialClient.cy;
            for (const auto& control : p->controls)
            {
                RECT r = control.second;
                int id = GetDlgCtrlID(control.first);
                if (id == IDC_LIST) { r.right += dx; r.bottom += dy; }
                else if (id == IDC_LEFT || id == IDC_RIGHT) r.right += dx;
                else if (id == IDC_RECOVERY_ROOT) { r.top += dy; r.bottom += dy; r.right += dx; }
                else if (id == IDC_SAVE_ORIGINALS) { r.top += dy; r.bottom += dy; r.left += dx; r.right += dx; }
                else if (id == IDC_RECOVERY_LABEL) { r.top += dy; r.bottom += dy; }
                else if (id == IDC_KEEP_ORIGINALS || id == IDC_RECOVERY_SPACE || id == IDC_DELETE_FILES || id == IDC_DELETE_SCOPE) { r.top += dy; r.bottom += dy; }
                else if (id == IDC_PROPOSAL_LABEL || id == IDC_ACTION || id == IDC_APPLY || id == IDC_SUMMARY || id == IDC_COPY_NEW || id == IDC_OVERWRITE || id == IDCANCEL)
                { r.top += dy; r.bottom += dy; if (id == IDCANCEL) { r.left += dx; r.right += dx; } }
                MoveWindow(control.first, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
            }
        }
        SizeStatus(window);
        return TRUE;
    case WM_TIMER: Status(*p); return TRUE;
    case Done:
        p->Join(); p->running = false; KillTimer(window, 1);
        if (p->validating) { p->result = std::move(p->checkedResult); p->validating = false; }
        if (p->copying) {
            p->copying=false; p->hasCopyReport=true; p->result.stale=true; p->result.snapshot.reset();
            Status(*p);
            for (const auto& path:{p->left,p->right}) NotifyPath(path);
            if (!p->closing) { if (p->failed) Message(window,IDS_COPY_FAILED); else CopySummary(*p); }
        }
        if (p->saving) {
            p->saving=false; p->hasRecoveryReport=true; p->result.stale=true; p->result.snapshot.reset();
            Status(*p);
            if (!p->recoveryReport.runDirectory.empty()) NotifyPath(p->recoveryRoot);
            if (!p->closing) { if (p->failed) Message(window,IDS_SAVE_FAILED); else RecoverySummary(*p); }
        }
        if(p->overwriting) {
            p->overwriting=false; p->hasOverwriteReport=true; p->result.stale=true; p->result.snapshot.reset();
            Status(*p);
            for(const auto& path:{p->left,p->right}) NotifyPath(path);
            if(!p->overwriteReport.recovery.runDirectory.empty()) NotifyPath(ControlText(window,IDC_RECOVERY_ROOT));
            if(!p->closing) { if(p->failed) Message(window,IDS_OVERWRITE_FAILED); else OverwriteSummary(*p); }
        }
        if(p->deleting) {
            p->deleting=false; p->hasDeleteReport=true; p->result.stale=true; p->result.snapshot.reset();
            Status(*p);
            NotifyPath(p->right);
            if(!p->deleteReport.recovery.runDirectory.empty()) NotifyPath(ControlText(window,IDC_RECOVERY_ROOT));
            if(!p->closing) { if(p->failed) Message(window,IDS_DELETE_FAILED); else DeleteSummary(*p); }
        }
        if (p->closing) { PersistSettings(window); EndDialog(window, 0); }
        else
        {
            HWND focused = GetFocus();
            bool restoreFocus = !focused || focused == GetDlgItem(window, IDC_STOP);
            Busy(*p, false); Filter(*p);
            if (restoreFocus)
                SendMessageW(window, WM_NEXTDLGCTL,
                    WPARAM(GetDlgItem(window, p->visible.empty() ? IDC_COMPARE : IDC_LIST)), TRUE);
            Preview::Intent intent = p->intent; p->intent = Preview::Intent::None;
            if (!p->failed && !p->result.stale && !p->result.cancelled && !p->cancel)
            {
                if (intent == Preview::Intent::Apply) Apply(*p);
                else if (intent == Preview::Intent::Summary) Summary(*p);
                else if (intent == Preview::Intent::Copy) CopyNew(*p);
                else if (intent == Preview::Intent::Save) SaveOriginals(*p);
                else if (intent == Preview::Intent::Overwrite) Overwrite(*p);
                else if (intent == Preview::Intent::Delete) DeleteFiles(*p);
            }
        }
        return TRUE;
    case WM_CLOSE:
        if (p->running) { p->closing = true; p->Stop(); }
        else { PersistSettings(window); EndDialog(window, 0); }
        return TRUE;
    case WM_SETTINGCHANGE: case WM_THEMECHANGED: ConfigureDark(window); return TRUE;
    case WM_COMMAND:
        switch (LOWORD(w))
        {
        case IDC_COMPARE: Start(*p); return TRUE;
        case IDC_STOP: p->Stop(); Status(*p); return TRUE;
        case IDCANCEL: SendMessageW(window, WM_CLOSE, 0, 0); return TRUE;
        case IDC_APPLY: ValidatePreview(*p, Preview::Intent::Apply); return TRUE;
        case IDC_SUMMARY: ValidatePreview(*p, Preview::Intent::Summary); return TRUE;
        case IDC_COPY_NEW: ValidatePreview(*p, Preview::Intent::Copy); return TRUE;
        case IDC_SAVE_ORIGINALS: ValidatePreview(*p, Preview::Intent::Save); return TRUE;
        case IDC_OVERWRITE: ValidatePreview(*p,Preview::Intent::Overwrite); return TRUE;
        case IDC_DELETE_FILES: ValidatePreview(*p,Preview::Intent::Delete); return TRUE;
        case IDC_KEEP_ORIGINALS:
            if(!p->running) { settings.keepOriginals=Checked(window,IDC_KEEP_ORIGINALS); Busy(*p,false); PersistSettings(window); }
            return TRUE;
        case IDC_RECOVERY_ROOT:
            if(HIWORD(w)==EN_CHANGE && !p->running) settings.recoveryRoot=ControlText(window,IDC_RECOVERY_ROOT);
            return TRUE;
        case IDC_EQUAL: case IDC_DIFFERENT: case IDC_SINGLE: case IDC_TORIGHT: case IDC_TOLEFT:
            if (!p->running) Filter(*p); return TRUE;
        case IDC_SELECTED:
            if (Checked(window, IDC_SELECTED))
            { SetDlgItemTextW(window, IDC_LEFT, p->panelLeft.c_str()); SetDlgItemTextW(window, IDC_RIGHT, p->panelRight.c_str()); }
            Busy(*p, false); InvalidatePlan(*p); return TRUE;
        case IDC_RECURSIVE: case IDC_CONTENT: case IDC_IGNORE: case IDC_ASYMMETRIC: case IDC_EMPTY:
            InvalidatePlan(*p); return TRUE;
        case IDC_LEFT: case IDC_RIGHT: case IDC_MASK:
            if (HIWORD(w) == EN_CHANGE) InvalidatePlan(*p); return TRUE;
        }
        break;
    case WM_NOTIFY:
        if (reinterpret_cast<NMHDR*>(l)->idFrom == IDC_LIST && reinterpret_cast<NMHDR*>(l)->code == LVN_GETDISPINFOW)
        {
            auto* info = reinterpret_cast<NMLVDISPINFOW*>(l);
            if ((info->item.mask & LVIF_TEXT) && info->item.iItem >= 0 && size_t(info->item.iItem) < p->visible.size())
            {
                std::wstring text = Cell(p->result.rows[p->visible[size_t(info->item.iItem)]], info->item.iSubItem);
                if (info->item.cchTextMax > 0)
                {
                    size_t length = std::min(text.size(), size_t(info->item.cchTextMax - 1));
                    if (length && length < text.size() && text[length - 1] >= 0xd800 && text[length - 1] <= 0xdbff &&
                        text[length] >= 0xdc00 && text[length] <= 0xdfff) --length;
                    memcpy(info->item.pszText, text.data(), length * sizeof(wchar_t)); info->item.pszText[length] = 0;
                }
            }
            return TRUE;
        }
        break;
    }
    return FALSE;
}
INT_PTR CALLBACK Dialog(HWND window, UINT message, WPARAM w, LPARAM l)
{
    try { return DialogBody(window, message, w, l); }
    catch (...)
    {
        auto* p = reinterpret_cast<Preview*>(GetWindowLongPtrW(window, DWLP_USER));
        if (p) { p->Stop(); p->Join(); }
        wchar_t text[512]{}; LoadStringW(language, IDS_FAILED, text, 512);
        DarkModeMessageBoxW(window, text, nullptr, MB_OK | MB_ICONERROR);
        EndDialog(window, 0);
        return TRUE;
    }
}
bool Snapshot(Preview& preview)
{
    std::vector<char> buffer(SAL_MAX_PATH);
    for (int panel : {PANEL_LEFT, PANEL_RIGHT})
    {
        int type = 0;
        if (!general->GetPanelPath(panel, buffer.data(), int(buffer.size()), &type, nullptr) || type != PATH_TYPE_WINDOWS) return false;
        std::wstring path;
        if (!dirsync::DecodePath(buffer.data(), path)) return false;
        (panel == PANEL_LEFT ? preview.left : preview.right) = std::move(path);
        int index = 0; BOOL directory = FALSE;
        while (const CFileData* item = general->GetPanelSelectedItem(panel, &index, &directory))
        {
            std::wstring name;
            if (item->UseWideName()) name = item->NameW;
            else if (!dirsync::DecodePath(item->Name, name)) return false;
            if (name.empty() || name == L".." || name.find_first_of(L"\\/") != std::wstring::npos) return false;
            preview.options.selection.push_back({std::move(name), directory != FALSE});
        }
    }
    preview.panelLeft = preview.left; preview.panelRight = preview.right;
    return true;
}
class Menu final : public CPluginInterfaceForMenuExtAbstract
{
public:
    DWORD WINAPI GetMenuItemState(int, DWORD) override { return MENU_ITEM_STATE_ENABLED; }
    BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract*, HWND parent, int, DWORD) override
    {
        if (active) { SetForegroundWindow(active->window); return FALSE; }
        try
        {
            Preview preview;
            if (!Snapshot(preview)) { ConfigureDark(nullptr); Message(parent, IDS_INPUT_ERROR); return FALSE; }
            general->SetUserWorkedOnPanelPath(PANEL_LEFT); general->SetUserWorkedOnPanelPath(PANEL_RIGHT);
            active = &preview;
            DialogBoxParamW(language, MAKEINTRESOURCEW(IDD_PREVIEW), parent, Dialog, LPARAM(&preview));
            active = nullptr;
        }
        catch (...) { active = nullptr; Message(parent, IDS_FAILED); }
        return FALSE; // Preserve panel selections; completed copies refresh via notifications.
    }
    BOOL WINAPI HelpForMenuItem(HWND parent, int) override { Message(parent, IDS_HELP); return TRUE; }
    void WINAPI BuildMenu(HWND, CSalamanderBuildMenuAbstract*) override {}
} menu;
class Plugin final : public CPluginInterfaceAbstract
{
public:
    void WINAPI About(HWND parent) override { ConfigureDark(nullptr); Message(parent, IDS_DESCRIPTION); }
    BOOL WINAPI Release(HWND, BOOL force) override
    {
        if (active)
        {
            if (!force) return FALSE;
            active->Stop(); active->Join();
            if (active->window) EndDialog(active->window, 0);
        }
        DarkModeShutdown();
        if (backgroundBrush) { DeleteObject(backgroundBrush); backgroundBrush = nullptr; }
        return TRUE;
    }
    void WINAPI LoadConfiguration(HWND, HKEY key, CSalamanderRegistryAbstract* registry) override
    { try { LoadSettings(key,registry); } catch(...) { settings={}; } }
    void WINAPI SaveConfiguration(HWND, HKEY key, CSalamanderRegistryAbstract* registry) override
    { StoreSettings(key,registry); }
    void WINAPI Configuration(HWND) override {}
    void WINAPI Connect(HWND, CSalamanderConnectAbstract* connect) override
    { connect->AddMenuItem(-1, general->LoadStr(language, IDS_MENU), 0, 1, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL); }
    void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract*) override {}
    CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() override { return nullptr; }
    CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() override { return nullptr; }
    CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() override { return &menu; }
    CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() override { return nullptr; }
    CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() override { return nullptr; }
    void WINAPI Event(int, DWORD) override {}
    void WINAPI ClearHistory(HWND) override {}
    void WINAPI AcceptChangeOnPathNotification(const char*, BOOL) override {}
    void WINAPI PasswordManagerEvent(HWND, int) override {}
} plugin;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { instance = module; DisableThreadLibraryCalls(module); }
    return TRUE;
}
int WINAPI SalamanderPluginGetReqVer() { return LAST_VERSION_OF_SALAMANDER; }
CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* entry)
{
    if (entry->GetVersion() < LAST_VERSION_OF_SALAMANDER) return nullptr;
    SalamanderDebug = entry->GetSalamanderDebug();
    general = entry->GetSalamanderGeneral();
    language = entry->LoadLanguageModule(entry->GetParentWindow(), "Directory Synchronization");
    if (!language) return nullptr;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES}; InitCommonControlsEx(&controls);
    ConfigureDark(nullptr);
    entry->SetBasicPluginData(general->LoadStr(language, IDS_NAME), FUNCTION_LOADSAVECONFIGURATION, "0.1", "Copyright (c) 2026 Open Salamander Authors",
                              general->LoadStr(language, IDS_DESCRIPTION), "DirectorySync");
    return &plugin;
}
