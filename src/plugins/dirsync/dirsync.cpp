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
#include "spl_gui.h"
#include "../../third_party/darkmodelib/include/Darkmodelib.h"
#include <thread>
#include <memory>
#include <algorithm>
#include <exception>
#include <utility>
#include <array>
#include <climits>
#include <shlobj.h>
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")

CSalamanderDebugAbstract* SalamanderDebug = nullptr;
namespace
{
HINSTANCE instance = nullptr, language = nullptr;
CSalamanderGeneralAbstract* general = nullptr;
CSalamanderGUIAbstract* gui = nullptr;
constexpr UINT Done = WM_APP + 41;
HBRUSH backgroundBrush = nullptr;
bool dark = false;
using Palette = std::array<COLORREF,7>;
Palette DefaultPalette(bool night)
{
    return night?Palette{RGB(109,220,144),RGB(244,209,105),RGB(109,220,144),RGB(112,190,255),RGB(185,185,185),RGB(255,148,133),RGB(255,148,133)}:
        Palette{RGB(0,112,52),RGB(139,91,0),RGB(0,112,52),RGB(0,92,175),RGB(96,96,96),RGB(178,42,28),RGB(178,42,28)};
}
struct Settings {
    bool keepOriginals=true;
    bool actionLabels=false;
    std::array<Palette,2> colors{DefaultPalette(false),DefaultPalette(true)};
    std::wstring recoveryRoot;
} settings;
void LoadSettings(HKEY key,CSalamanderRegistryAbstract* registry)
{
    settings={};
    if(!key || !registry) return;
    DWORD enabled=1,bytes=0;
    if(registry->GetValue(key,"KeepOriginals",REG_DWORD,&enabled,sizeof(enabled)) && enabled<=1)
        settings.keepOriginals=enabled!=0;
    enabled=0;
    if(registry->GetValue(key,"ActionLabels",REG_DWORD,&enabled,sizeof(enabled)) && enabled<=1)
        settings.actionLabels=enabled!=0;
    for(int theme=0;theme<2;++theme) {
        Palette palette{};DWORD size=0;const char* name=theme?"DarkColorsV1":"LightColorsV1";
        if(registry->GetSize(key,name,REG_BINARY,size) && size==sizeof(palette) &&
            registry->GetValue(key,name,REG_BINARY,palette.data(),sizeof(palette)) &&
            std::all_of(palette.begin(),palette.end(),[](COLORREF c){return c<=0xffffff;})) settings.colors[theme]=palette;
    }
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
    DWORD labels=settings.actionLabels?1:0;
    bool appearance=registry->SetValue(key,"ActionLabels",REG_DWORD,&labels,sizeof(labels))!=FALSE;
    for(int theme=0;theme<2;++theme)
        appearance=(registry->SetValue(key,theme?"DarkColorsV1":"LightColorsV1",REG_BINARY,settings.colors[theme].data(),sizeof(Palette))!=FALSE) && appearance;
    return mode && path && appearance;
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
struct FolderChoice {HWND parent;std::wstring initial,title;};
bool FolderSelectionPath(const std::wstring& selected,std::wstring& result)
{
    result.clear();
    auto api=dirsync::ApiPath(selected);
    DWORD size=GetLongPathNameW(api.c_str(),nullptr,0);
    if(!size || size>32768) return false;
    std::vector<wchar_t> buffer(size);
    DWORD count=GetLongPathNameW(api.c_str(),buffer.data(),size);
    if(!count || count>=size) return false;
    result.assign(buffer.data(),count);
    if(result.rfind(L"\\\\?\\UNC\\",0)==0) result=L"\\\\"+result.substr(8);
    else if(result.rfind(L"\\\\?\\",0)==0) result.erase(0,4);
    return !result.empty() && result.size()<32760;
}
int CALLBACK FolderCallback(HWND window,UINT message,LPARAM,LPARAM data)
{
    if(message==BFFM_INITIALIZED) {
        auto& choice=*reinterpret_cast<FolderChoice*>(data);
        general->MultiMonCenterWindow(window,choice.parent,FALSE);
        SetWindowTextW(window,choice.title.c_str());ConfigureDark(window);
        if(!choice.initial.empty()) SendMessageW(window,BFFM_SETSELECTIONW,TRUE,LPARAM(choice.initial.c_str()));
    }
    return 0;
}
void BrowseFolder(HWND parent,int edit,int title)
{
    if(!IsWindowEnabled(GetDlgItem(parent,edit))) return;
    struct Apartment {
        HRESULT result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        ~Apartment() {if(SUCCEEDED(result)) CoUninitialize();}
    } apartment;
    if(FAILED(apartment.result)) {Message(parent,IDS_BROWSE_FAILED);return;}
    // Use the same native tree picker as the host, through wide APIs so that
    // the legacy SDK's narrow/MAX_PATH browse boundary cannot truncate paths.
    FolderChoice choice{parent,ControlText(parent,edit),Text(title)};
    BROWSEINFOW browse{};browse.hwndOwner=parent; browse.lpszTitle=choice.title.c_str();
    browse.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE; browse.lpfn=FolderCallback; browse.lParam=LPARAM(&choice);
    struct FreePidl {using pointer=PIDLIST_ABSOLUTE;void operator()(pointer p) const {ILFree(p);}};
    std::unique_ptr<ITEMIDLIST_ABSOLUTE,FreePidl> item(SHBrowseForFolderW(&browse));
    ConfigureDark(parent);
    if(!item) return;
    PWSTR value=nullptr;
    HRESULT resolved=SHGetNameFromIDList(item.get(),SIGDN_FILESYSPATH,&value);
    std::unique_ptr<wchar_t,decltype(&CoTaskMemFree)> path(value,CoTaskMemFree);
    std::wstring full;
    // The shell can return an 8.3 alias for a long path, even through wide APIs.
    if(FAILED(resolved) || !path || !FolderSelectionPath(path.get(),full)) {Message(parent,IDS_BROWSE_FAILED);return;}
    if(choice.initial!=full) SetDlgItemTextW(parent,edit,full.c_str());
}
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
    RECT units{0,0,145,0}; MapDialogRect(window,&units);
    ListView_SetColumnWidth(list,0,units.right);
    ListView_SetColumnWidth(list,1,std::max(80,int(r.right-units.right)-GetSystemMetrics(SM_CXVSCROLL)-4));
}
void CompactInformation(HWND window,const Information& info)
{
    RECT units{7,5,0,96};MapDialogRect(window,&units);
    const int margin=units.left,gap=units.top;
    HWND list=GetDlgItem(window,IDC_INFO_TABLE),notes=GetDlgItem(window,IDC_INFO_NOTES);
    RECT listBox{},header{},item{},label{},button{},client{},outer{};
    GetWindowRect(list,&listBox);GetWindowRect(ListView_GetHeader(list),&header);
    ListView_GetItemRect(list,0,&item,LVIR_BOUNDS);
    int rowHeight=std::max(1,int(item.bottom-item.top));
    int tableHeight=int(header.bottom-header.top)+rowHeight*int(std::min(size_t(10),std::max(size_t(1),info.rows.size())))+4;
    GetWindowRect(GetDlgItem(window,IDC_INFO_NOTES_LABEL),&label);
    GetWindowRect(GetDlgItem(window,IDOK),&button);
    int width=int(listBox.right-listBox.left);
    HDC dc=GetDC(notes);auto font=SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(notes,WM_GETFONT,0,0)));
    RECT text{0,0,width-GetSystemMetrics(SM_CXVSCROLL)-8,0};
    DrawTextW(dc,info.notes.c_str(),int(info.notes.size()),&text,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX|DT_EDITCONTROL);
    TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);SelectObject(dc,font);ReleaseDC(notes,dc);
    int notesHeight=std::min(int(units.bottom),std::max(int(metrics.tmHeight)*2+6,int(text.bottom)+8));
    int labelY=margin+tableHeight+gap,notesY=labelY+int(label.bottom-label.top)+2;
    int buttonY=notesY+notesHeight+gap;
    MoveWindow(list,margin,margin,width,tableHeight,FALSE);
    MoveWindow(GetDlgItem(window,IDC_INFO_NOTES_LABEL),margin,labelY,width,int(label.bottom-label.top),FALSE);
    MoveWindow(notes,margin,notesY,width,notesHeight,FALSE);
    for(int id:{IDOK,IDCANCEL}) {
        RECT r{};GetWindowRect(GetDlgItem(window,id),&r);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);
        MoveWindow(GetDlgItem(window,id),r.left,buttonY,r.right-r.left,r.bottom-r.top,FALSE);
    }
    GetClientRect(window,&client);GetWindowRect(window,&outer);
    int height=buttonY+int(button.bottom-button.top)+margin+int(outer.bottom-outer.top-client.bottom);
    SetWindowPos(window,nullptr,outer.left,outer.top+int(outer.bottom-outer.top-height)/2,
        outer.right-outer.left,height,SWP_NOZORDER|SWP_NOACTIVATE);
}
void CopyInformation(HWND window,const Information& info,bool all)
{
    std::wstring text;
    if(all) {
        text=Text(info.title)+L"\r\n";
        for(const auto& row:info.rows) text+=row.first+L"\t"+row.second+L"\r\n";
        text+=L"\r\n"+EditLines(info.notes);
    } else {
        int index=ListView_GetNextItem(GetDlgItem(window,IDC_INFO_TABLE),-1,LVNI_SELECTED);
        if(index<0 || size_t(index)>=info.rows.size()) return;
        text=info.rows[size_t(index)].second;
    }
    if(!general->CopyTextToClipboardW(text.c_str(),int(text.size()),FALSE,window)) Message(window,IDS_UI_CLIPBOARD_ERROR);
}
void InformationMenu(HWND window,const Information& info,LPARAM position)
{
    if(!gui) return;
    HWND list=GetDlgItem(window,IDC_INFO_TABLE);
    POINT point{static_cast<short>(LOWORD(position)),static_cast<short>(HIWORD(position))};
    if(position==LPARAM(-1)) {
        int index=ListView_GetNextItem(list,-1,LVNI_SELECTED);RECT r{};
        if(index>=0 && ListView_GetItemRect(list,index,&r,LVIR_BOUNDS)) point={r.left,r.bottom};
        else point={0,0};
        ClientToScreen(list,&point);
    } else {
        LVHITTESTINFO hit{};hit.pt=point;ScreenToClient(list,&hit.pt);
        int index=ListView_SubItemHitTest(list,&hit);
        if(index<0) return;
        ListView_SetItemState(list,index,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    }
    MENU_TEMPLATE_ITEM items[]={{MNTT_PB,0,0,0,-1,0,nullptr},
        {MNTT_IT,IDS_UI_COPY_VALUE,MNTS_B|MNTS_I|MNTS_A,1,-1,0,nullptr},
        {MNTT_IT,IDS_UI_COPY_ALL,MNTS_B|MNTS_I|MNTS_A,2,-1,0,nullptr},{MNTT_PE,0,0,0,-1,0,nullptr}};
    auto* menu=gui->CreateMenuPopup();if(!menu) return;
    DWORD command=0;
    if(menu->LoadFromTemplate(language,items,nullptr,nullptr,nullptr))
        command=menu->Track(MENU_TRACK_RETURNCMD|MENU_TRACK_RIGHTBUTTON|MENU_TRACK_NONOTIFY,point.x,point.y,window,nullptr);
    gui->DestroyMenuPopup(menu);
    if(command) CopyInformation(window,info,command==2);
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
            SendDlgItemMessageW(window,IDC_INFO_NOTES,EM_SETLIMITTEXT,0x7ffffffe,0);
            SetDlgItemTextW(window,IDC_INFO_NOTES,EditLines(info->notes).c_str());
            if(!info->confirm) {
                RECT r{};GetWindowRect(GetDlgItem(window,IDCANCEL),&r);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);
                MoveWindow(GetDlgItem(window,IDOK),r.left,r.top,r.right-r.left,r.bottom-r.top,FALSE);
                ShowWindow(GetDlgItem(window,IDCANCEL),SW_HIDE);
                SetDlgItemTextW(window,IDOK,Text(IDS_UI_SUMMARY).c_str());
                SendMessageW(window,DM_SETDEFID,IDOK,0);
            }
            if(!info->rows.empty()) ListView_SetItemState(list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
            CompactInformation(window,*info);
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
            if(reinterpret_cast<NMHDR*>(l)->idFrom==IDC_INFO_TABLE && reinterpret_cast<NMHDR*>(l)->code==LVN_KEYDOWN) {
                auto* key=reinterpret_cast<NMLVKEYDOWN*>(l);
                if(key->wVKey=='C' && (GetKeyState(VK_CONTROL)&0x8000)) CopyInformation(window,*info,(GetKeyState(VK_SHIFT)&0x8000)!=0);
            }
            return FALSE;
        case WM_CONTEXTMENU:
            if(reinterpret_cast<HWND>(w)==GetDlgItem(window,IDC_INFO_TABLE)) {InformationMenu(window,*info,l);return TRUE;}
            break;
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
    if(confirm) rows.insert(rows.begin(),{Text(IDS_UI_SCOPE_LABEL),Text(IDS_UI_SCOPE_VALUE)});
    Information info{title,std::move(rows),confirm?Text(IDS_UI_SCOPE_NOTE)+L"\n\n"+Text(IDS_UI_ADS_NOTE)+L"\n\n"+notes:notes,confirm,{}, {}, {}};
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
    std::vector<dirsync::Action> defaults;
    std::array<HICON,7> buttonIcons{};
    std::array<bool,5> filterChecks{};
    std::atomic<bool> cancel{false};
    dirsync::Progress progress;
    dirsync::CopyProgress copyProgress;
    dirsync::CopyReport copyReport;
    dirsync::RecoveryReport recoveryReport;
    dirsync::OverwriteReport overwriteReport;
    dirsync::DeleteReport deleteReport;
    std::wstring recoveryRoot,tipText;
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
    ~Preview() { Stop(); Join(); for(auto icon:buttonIcons) if(icon) DestroyIcon(icon); }
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
    case 3: return L"     " + Text(IDS_ACTION + int(row.action));
    case 4: return row.hasRight && !row.right.directory ? Number(row.right.size) : L"";
    case 5: return row.hasRight ? Modified(row.right.time) : L"";
    case 6: return L"     " + Text(IDS_STATE + int(row.state));
    case 7: return Text(IDS_REASON + int(row.reason));
    case 8: return row.error ? Number(row.error) : L"";
    }
    return L"";
}
// Draw simple geometric marks so their shape does not depend on font coverage.
// Text remains available to accessibility clients, and selection keeps native colors.
enum class Mark { Equal, Different, Right, Left, Skip, Delete, Warning, Single };
Mark RowMark(const dirsync::Row& row,bool action)
{
    if(!action) {
        if(row.state==dirsync::State::Equal) return Mark::Equal;
        if(row.state==dirsync::State::Blocked || row.state==dirsync::State::Conflict) return Mark::Warning;
        return Mark::Different;
    }
    switch(row.action) {
    case dirsync::Action::CopyRight: return Mark::Right;
    case dirsync::Action::CopyLeft: return Mark::Left;
    case dirsync::Action::DeleteRight: return Mark::Delete;
    case dirsync::Action::None: return Mark::Skip;
    default: return Mark::Warning;
    }
}
COLORREF MarkColor(Mark mark)
{
    return settings.colors[dark?1:0][size_t(mark)];
}
COLORREF PreviewColor(const dirsync::Row& row,int column,COLORREF background,COLORREF normal)
{
    const bool skip=row.action==dirsync::Action::None;
    if(column!=3 && column!=6) return skip?MarkColor(Mark::Skip):normal;
    Mark mark=RowMark(row,column==3);COLORREF color=MarkColor(mark);
    if(!skip || mark==Mark::Skip) return color;
    // Preserve the semantic hue while fading skipped rows toward the list background.
    return RGB((GetRValue(color)+GetRValue(background))/2,
        (GetGValue(color)+GetGValue(background))/2,(GetBValue(color)+GetBValue(background))/2);
}
void PaintMark(HDC dc,RECT cell,Mark mark,COLORREF color,bool centered=false)
{
    int dpi=GetDeviceCaps(dc,LOGPIXELSX);
    int x=cell.left+MulDiv(4,dpi,96),y=(cell.top+cell.bottom)/2;
    if(centered) x=(cell.left+cell.right-MulDiv(10,dpi,96))/2;
    HPEN pen=CreatePen(PS_SOLID,std::max(1,MulDiv(2,dpi,96)),color);
    if(!pen) return;
    HGDIOBJ old=SelectObject(dc,pen);
    auto line=[&](int a,int b,int c,int d) { MoveToEx(dc,x+MulDiv(a,dpi,96),y+MulDiv(b,dpi,96),nullptr); LineTo(dc,x+MulDiv(c,dpi,96),y+MulDiv(d,dpi,96)); };
    int saved=SaveDC(dc);
    if(!saved) { SelectObject(dc,old);DeleteObject(pen);return; }
    IntersectClipRect(dc,cell.left,cell.top,cell.right,cell.bottom);
    switch(mark) {
    case Mark::Right: line(0,0,10,0);line(6,-4,10,0);line(10,0,6,4);break;
    case Mark::Left: line(0,0,10,0);line(0,0,4,-4);line(0,0,4,4);break;
    case Mark::Equal: case Mark::Different:
        line(0,-2,10,-2);line(0,2,10,2);
        if(mark==Mark::Different) line(2,5,8,-5);
        break;
    case Mark::Delete: line(1,-4,9,4);line(1,4,9,-4);break;
    case Mark::Skip: line(3,-4,3,4);line(7,-4,7,4);break;
    case Mark::Warning: line(5,-5,5,1);line(5,3,5,5);break;
    case Mark::Single: line(2,-5,8,-5);line(8,-5,8,5);line(8,5,2,5);line(2,5,2,-5);break;
    }
    RestoreDC(dc,saved);
    SelectObject(dc,old);DeleteObject(pen);
}
void ButtonIcons(Preview& p)
{
    HWND window=p.window;HDC dc=GetDC(window);int size=MulDiv(16,GetDeviceCaps(dc,LOGPIXELSX),96);
    const int ids[]={IDC_EQUAL,IDC_DIFFERENT,IDC_SINGLE,IDC_TORIGHT,IDC_TOLEFT,IDC_STOP,IDC_STOP_SYNC};
    const Mark marks[]={Mark::Equal,Mark::Different,Mark::Single,Mark::Right,Mark::Left,Mark::Delete,Mark::Delete};
    for(size_t i=0;i<p.buttonIcons.size();++i) {
        // The host helper explicitly subclasses every checkbox. Its glyph/text
        // painter is for ordinary checkboxes, not native push-like icon toggles.
        if(i<5) dmlib::removeCheckboxOrRadioBtnCtrlSubclass(GetDlgItem(window,ids[i]));
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=size;
        info.bmiHeader.biHeight=-size;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* data=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);
        HDC memory=CreateCompatibleDC(dc);HBITMAP mask=CreateBitmap(size,size,1,1,nullptr);
        HICON icon=nullptr;
        if(bitmap && data && memory && mask) {
            memset(data,0,size_t(size)*size*4);auto old=SelectObject(memory,bitmap);
            RECT cell{0,0,size,size};PaintMark(memory,cell,marks[i],RGB(255,255,255),true);
            SelectObject(memory,old);
            // Cancellation crosses follow the host's neutral button text color,
            // independent of the configurable file-deletion color.
            COLORREF color=i<5?MarkColor(marks[i]==Mark::Single?Mark::Different:marks[i]):
                (dark?DarkModeGetColors().text:GetSysColor(COLOR_BTNTEXT));
            DWORD pixel=0xff000000|(DWORD(GetRValue(color))<<16)|(DWORD(GetGValue(color))<<8)|GetBValue(color);
            auto* pixels=static_cast<DWORD*>(data);
            for(int n=0;n<size*size;++n) if(pixels[n]&0xffffff) pixels[n]=pixel;
            ICONINFO ii{};ii.fIcon=TRUE;ii.hbmColor=bitmap;ii.hbmMask=mask;icon=CreateIconIndirect(&ii);
        }
        if(memory) DeleteDC(memory);if(bitmap) DeleteObject(bitmap);if(mask) DeleteObject(mask);
        if(icon) {
            SendDlgItemMessageW(window,ids[i],BM_SETIMAGE,IMAGE_ICON,LPARAM(icon));
            if(p.buttonIcons[i]) DestroyIcon(p.buttonIcons[i]);p.buttonIcons[i]=icon;
        }
    }
    ReleaseDC(window,dc);
}
struct Appearance {
    Settings draft=settings;
    int theme=0;
    std::array<CGUIColorArrowButtonAbstract*,7> buttons{};
};
void Tooltips(HWND window);
void RecoveryControls(HWND window)
{
    bool keep=Checked(window,IDC_KEEP_ORIGINALS);
    for(int id:{IDC_RECOVERY_ROOT,IDC_RECOVERY_LABEL,IDC_BROWSE_RECOVERY}) EnableWindow(GetDlgItem(window,id),keep);
}
void AppearanceSample(HWND window,Appearance& appearance)
{
    for(int item=0;item<7;++item) {
        if(appearance.buttons[item]) appearance.buttons[item]->SetColor(appearance.draft.colors[appearance.theme][item],
            appearance.theme?RGB(32,32,32):RGB(255,255,255));
        InvalidateRect(GetDlgItem(window,IDC_COLOR_SAMPLE_FIRST+item),nullptr,TRUE);
    }
}
INT_PTR AppearanceBody(HWND window,UINT message,WPARAM w,LPARAM l)
{
    auto* appearance=reinterpret_cast<Appearance*>(GetWindowLongPtrW(window,DWLP_USER));
    if(message==WM_INITDIALOG) {
        appearance=reinterpret_cast<Appearance*>(l);
        SetWindowLongPtrW(window,DWLP_USER,l);
        ConfigureDark(window);appearance->theme=dark?1:0;
        CheckDlgButton(window,IDC_ACTION_LABELS,appearance->draft.actionLabels?BST_CHECKED:BST_UNCHECKED);
        CheckDlgButton(window,IDC_KEEP_ORIGINALS,appearance->draft.keepOriginals?BST_CHECKED:BST_UNCHECKED);
        SendDlgItemMessageW(window,IDC_RECOVERY_ROOT,EM_SETLIMITTEXT,32759,0);
        SetDlgItemTextW(window,IDC_RECOVERY_ROOT,appearance->draft.recoveryRoot.c_str());
        RecoveryControls(window);Tooltips(window);
        for(int item=0;item<7;++item) {
            SetDlgItemTextW(window,IDC_COLOR_BUTTON_FIRST+item,Text(IDS_COLOR_FIRST+item).c_str());
            if(gui) appearance->buttons[item]=gui->AttachColorArrowButton(window,IDC_COLOR_BUTTON_FIRST+item,TRUE);
        }
        AppearanceSample(window,*appearance);return TRUE;
    }
    if(!appearance) return FALSE;
    LRESULT brush=0;if(dark && DarkModeHandleCtlColor(message,w,l,brush)) return brush;
    if(message==WM_HELP) {
        auto* help=reinterpret_cast<HELPINFO*>(l);
        int id=help?GetDlgCtrlID(reinterpret_cast<HWND>(help->hItemHandle)):0;
        if(id==IDC_KEEP_ORIGINALS || id==IDC_RECOVERY_ROOT) Message(window,id==IDC_KEEP_ORIGINALS?IDS_TIP_KEEP:IDS_SAVE_ROOT);
        return TRUE;
    }
    if(message==WM_DRAWITEM && w>=IDC_COLOR_SAMPLE_FIRST && w<IDC_COLOR_SAMPLE_FIRST+7) {
        auto* draw=reinterpret_cast<DRAWITEMSTRUCT*>(l);
        int item=int(w)-IDC_COLOR_SAMPLE_FIRST;
        HBRUSH bg=CreateSolidBrush(appearance->theme?RGB(32,32,32):RGB(255,255,255));
        FillRect(draw->hDC,&draw->rcItem,bg);DeleteObject(bg);
        PaintMark(draw->hDC,draw->rcItem,Mark(item),appearance->draft.colors[appearance->theme][item],true);return TRUE;
    }
    if(message==WM_CLOSE) {EndDialog(window,IDCANCEL);return TRUE;}
    if(message==WM_COMMAND) {
        int id=LOWORD(w);
        if(id>=IDC_COLOR_BUTTON_FIRST && id<IDC_COLOR_BUTTON_FIRST+7) {
            int item=id-IDC_COLOR_BUTTON_FIRST;
            COLORREF custom[16]{};CHOOSECOLOR choose{};choose.lStructSize=sizeof(choose);choose.hwndOwner=window;
            choose.rgbResult=appearance->draft.colors[appearance->theme][item];choose.lpCustColors=custom;choose.Flags=CC_FULLOPEN|CC_RGBINIT;
            DarkModePrepareChooseColor(&choose);
            if(ChooseColor(&choose)) appearance->draft.colors[appearance->theme][item]=choose.rgbResult;
            AppearanceSample(window,*appearance);return TRUE;
        }
        switch(id) {
        case IDC_BROWSE_RECOVERY: BrowseFolder(window,IDC_RECOVERY_ROOT,IDS_BROWSE_RECOVERY);return TRUE;
        case IDC_KEEP_ORIGINALS: RecoveryControls(window);return TRUE;
        case IDC_COLOR_RESET:
            appearance->draft.colors[appearance->theme]=DefaultPalette(appearance->theme!=0);
            AppearanceSample(window,*appearance);return TRUE;
        case IDOK:
            appearance->draft.recoveryRoot=ControlText(window,IDC_RECOVERY_ROOT);
            settings.keepOriginals=Checked(window,IDC_KEEP_ORIGINALS);
            settings.recoveryRoot=std::move(appearance->draft.recoveryRoot);
            settings.actionLabels=Checked(window,IDC_ACTION_LABELS);
            settings.colors[appearance->theme]=appearance->draft.colors[appearance->theme];
            PersistSettings(window);EndDialog(window,IDOK);return TRUE;
        case IDCANCEL: EndDialog(window,IDCANCEL);return TRUE;
        }
    }
    return FALSE;
}
INT_PTR CALLBACK AppearanceDialog(HWND window,UINT message,WPARAM w,LPARAM l)
{
    try {return AppearanceBody(window,message,w,l);}
    catch(...) {EndDialog(window,IDCANCEL);return TRUE;}
}
void ConfigureAppearance(HWND parent)
{
    Appearance appearance;
    DialogBoxParamW(language,MAKEINTRESOURCEW(IDD_APPEARANCE),parent,AppearanceDialog,LPARAM(&appearance));
    ConfigureDark(nullptr);
}
LRESULT CALLBACK TooltipLifetime(HWND window,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data)
{
    if(message==WM_NCDESTROY) {
        RemoveWindowSubclass(window,TooltipLifetime,id);
        delete reinterpret_cast<std::vector<std::wstring>*>(data);
    }
    return DefSubclassProc(window,message,w,l);
}
void Tooltips(HWND window)
{
    HWND tip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
        CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,window,nullptr,instance,nullptr);
    if(!tip) return;
    auto texts=std::make_unique<std::vector<std::wstring>>();texts->reserve(24);
    if(!SetWindowSubclass(tip,TooltipLifetime,1,reinterpret_cast<DWORD_PTR>(texts.get()))) {DestroyWindow(tip);return;}
    auto* storage=texts.release();
    HDC dc=GetDC(window);int dpi=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(window,dc);
    SendMessageW(tip,TTM_SETMAXTIPWIDTH,0,MulDiv(420,dpi,96));
    for(auto entry:{std::pair<int,int>{IDC_MASK,IDS_TIP_MASK},{IDC_KEEP_ORIGINALS,IDS_TIP_KEEP},{IDC_RECOVERY_ROOT,IDS_SAVE_ROOT},
        {IDC_SYNCHRONIZE,IDS_TIP_SCOPE},{IDC_STOP,IDS_TIP_STOP_COMPARE},{IDC_STOP_SYNC,IDS_TIP_STOP_SYNC},
        {IDC_SELECTED,IDS_TIP_SELECTION},{IDC_EMPTY,IDS_TIP_EMPTY},
        {IDC_RECURSIVE,IDS_TIP_SCAN_OPTIONS},{IDC_CONTENT,IDS_TIP_SCAN_OPTIONS},
        {IDC_IGNORE,IDS_TIP_SCAN_OPTIONS},{IDC_ASYMMETRIC,IDS_TIP_SCAN_OPTIONS},
        {IDC_BROWSE_LEFT,IDS_BROWSE_LEFT},{IDC_BROWSE_RIGHT,IDS_BROWSE_RIGHT},{IDC_BROWSE_RECOVERY,IDS_BROWSE_RECOVERY},
        {IDC_EQUAL,IDS_FILTER_EQUAL},{IDC_DIFFERENT,IDS_FILTER_DIFFERENT},{IDC_SINGLE,IDS_FILTER_SINGLE},
        {IDC_TORIGHT,IDS_FILTER_RIGHT},{IDC_TOLEFT,IDS_FILTER_LEFT},{IDC_LIST,0}}) {
        TOOLINFOW tool{};tool.cbSize=sizeof(tool);tool.uFlags=TTF_IDISHWND|TTF_SUBCLASS;
        tool.hwnd=window;tool.uId=UINT_PTR(GetDlgItem(window,entry.first));tool.hinst=language;
        if(!tool.uId) continue;
        // Resource-backed native tooltips can truncate long strings. Keep the
        // complete Unicode text alive until the tooltip itself is destroyed.
        if(entry.second) {storage->push_back(Text(entry.second));tool.lpszText=storage->back().data();}
        else tool.lpszText=LPSTR_TEXTCALLBACKW;
        SendMessageW(tip,TTM_ADDTOOLW,0,LPARAM(&tool));
    }
    DarkModeApplyWindow(tip);
}
void ActionColumn(HWND window)
{
    HDC dc=GetDC(window);int dpi=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(window,dc);
    ListView_SetColumnWidth(GetDlgItem(window,IDC_LIST),3,MulDiv(settings.actionLabels?145:48,dpi,96));
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
    // Native status-bar height can differ from the resource's nominal DU height.
    // Join the list to its actual top edge at every size, with no outer margins.
    HWND list=GetDlgItem(window,IDC_LIST);RECT bounds{},status{};
    if(list && GetWindowRect(list,&bounds) && GetWindowRect(bar,&status)) {
        MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&bounds),2);
        MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&status),2);
        MoveWindow(list,0,bounds.top,client.right,std::max(0L,status.top-bounds.top),TRUE);
    }
}
bool PendingOptions(const Preview& p)
{
    return p.result.snapshot && (Checked(p.window,IDC_RECURSIVE)!=p.options.recursive ||
        Checked(p.window,IDC_CONTENT)!=p.options.content || Checked(p.window,IDC_IGNORE)!=p.options.ignoreDate ||
        Checked(p.window,IDC_ASYMMETRIC)!=p.options.asymmetric || Checked(p.window,IDC_EMPTY)!=p.options.emptyDirectories ||
        Checked(p.window,IDC_SELECTED)!=p.options.onlySelected);
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
        else state=Text(PendingOptions(p)?IDS_OPTIONS_PENDING:p.result.snapshot?IDS_UI_COMPLETE:IDS_UI_READY);
        uint64_t blocked=0;
        for(const auto& row:p.result.rows) if(row.state==dirsync::State::Blocked || row.state==dirsync::State::Conflict) ++blocked;
        swprintf_s(counts,Text(IDS_UI_ROWS).c_str(),uint64_t(p.result.rows.size()),uint64_t(p.visible.size()),uint64_t(p.result.rows.size()-p.visible.size()),blocked);
    }
    StatusPane(p.window,0,state);StatusPane(p.window,1,counts);
}
bool RowVisible(const dirsync::Row& row,const std::array<bool,5>& filters)
{
    if(row.state==dirsync::State::Blocked || row.state==dirsync::State::Conflict) return true;
    bool show=filters[row.state==dirsync::State::Equal?0:
        (row.state==dirsync::State::LeftOnly || row.state==dirsync::State::RightOnly)?2:1];
    if(row.action==dirsync::Action::CopyRight) show &= filters[3];
    if(row.action==dirsync::Action::CopyLeft) show &= filters[4];
    return show;
}
void Filter(Preview& p)
{
    HWND list=GetDlgItem(p.window,IDC_LIST);
    SendMessageW(ListView_GetToolTips(list),TTM_POP,0,0);
    std::vector<size_t> selected;
    int focused=ListView_GetNextItem(list,-1,LVNI_FOCUSED);
    size_t focus=focused>=0 && size_t(focused)<p.visible.size()?p.visible[size_t(focused)]:SIZE_MAX;
    for(int i=ListView_GetNextItem(list,-1,LVNI_SELECTED);i>=0;i=ListView_GetNextItem(list,i,LVNI_SELECTED))
        if(size_t(i)<p.visible.size()) selected.push_back(p.visible[size_t(i)]);
    ListView_SetItemState(list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
    p.visible.clear();
    for (size_t i = 0; i < p.result.rows.size(); ++i)
    {
        if (RowVisible(p.result.rows[i],p.filterChecks)) p.visible.push_back(i);
    }
    ListView_SetItemCountEx(GetDlgItem(p.window, IDC_LIST), int(p.visible.size()), LVSICF_NOSCROLL);
    for(size_t i=0;i<p.visible.size();++i) {
        // Both lists retain ascending model indices; avoid quadratic Ctrl+A filtering.
        UINT state=(std::binary_search(selected.begin(),selected.end(),p.visible[i])?LVIS_SELECTED:0)|
            (p.visible[i]==focus?LVIS_FOCUSED:0);
        if(state) ListView_SetItemState(list,int(i),state,LVIS_SELECTED|LVIS_FOCUSED);
    }
    InvalidateRect(GetDlgItem(p.window, IDC_LIST), nullptr, TRUE);
    Status(p);
}
int StopControl(const Preview& p)
{
    const bool files=p.copying || p.saving || p.overwriting || p.deleting ||
        (p.validating && (p.intent==Preview::Intent::Copy || p.intent==Preview::Intent::Save ||
            p.intent==Preview::Intent::Overwrite || p.intent==Preview::Intent::Delete));
    return files?IDC_STOP_SYNC:IDC_STOP;
}
void Busy(Preview& p, bool busy)
{
    for (int id : {IDC_MASK, IDC_RECURSIVE, IDC_CONTENT, IDC_IGNORE, IDC_ASYMMETRIC, IDC_SELECTED,
                   IDC_EMPTY, IDC_COMPARE, IDC_SYNCHRONIZE, IDC_EQUAL,
                   IDC_DIFFERENT, IDC_SINGLE, IDC_TORIGHT, IDC_TOLEFT})
        EnableWindow(GetDlgItem(p.window, id), !busy);
    EnableWindow(GetDlgItem(p.window, IDC_SELECTED), !busy && !p.options.selection.empty());
    bool locked = Checked(p.window, IDC_SELECTED);
    EnableWindow(GetDlgItem(p.window, IDC_LEFT), !busy && !locked);
    EnableWindow(GetDlgItem(p.window, IDC_RIGHT), !busy && !locked);
    for(int id:{IDC_BROWSE_LEFT,IDC_BROWSE_RIGHT}) EnableWindow(GetDlgItem(p.window,id),!busy && !locked);
    for(int id:{IDC_STOP,IDC_STOP_SYNC}) EnableWindow(GetDlgItem(p.window,id),busy && id==StopControl(p));
}
void InvalidatePlan(Preview& p)
{
    if (p.running) return;
    p.result = {}; p.visible.clear(); p.defaults.clear(); p.failed = false;
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
    if(Checked(p.window,IDC_SELECTED)) {
        SetDlgItemTextW(p.window,IDC_LEFT,p.panelLeft.c_str());SetDlgItemTextW(p.window,IDC_RIGHT,p.panelRight.c_str());
    }
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
    SendMessageW(p.window, WM_NEXTDLGCTL, WPARAM(GetDlgItem(p.window, StopControl(p))), TRUE);
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
std::vector<size_t> SelectedRows(const Preview& p)
{
    std::vector<size_t> rows;HWND list=GetDlgItem(p.window,IDC_LIST);
    for(int i=ListView_GetNextItem(list,-1,LVNI_SELECTED);i>=0;i=ListView_GetNextItem(list,i,LVNI_SELECTED))
        if(size_t(i)<p.visible.size()) rows.push_back(p.visible[size_t(i)]);
    return rows;
}
void ValidatePreview(Preview& p, Preview::Intent intent,const std::vector<size_t>* rows=nullptr,int action=-1)
{
    if (p.running) return;
    if (!p.result.snapshot || p.result.cancelled || p.result.stale || p.failed)
    { Message(p.window, IDS_STALE); return; }
    p.requestedRows=rows?*rows:SelectedRows(p);
    p.requestedAction=action;
    p.cancel = false; p.progress.scanned = 0; p.progress.compared = 0; p.progress.readBytes = 0;
    p.validating = true; p.intent = intent; p.running = true; p.checkedResult = {};
    Busy(p, true);
    SendMessageW(p.window, WM_NEXTDLGCTL, WPARAM(GetDlgItem(p.window, StopControl(p))), TRUE);
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
// Values 0..4 match Action. All proposal changes are selection-scoped menu commands.
constexpr int RestoreProposal=5,ReverseProposal=6;
bool Proposal(const Preview& p,size_t index,int command,dirsync::Action& action)
{
    if(index>=p.result.rows.size()) return false;
    const auto& row=p.result.rows[index];
    if(row.state==dirsync::State::Blocked || row.state==dirsync::State::Conflict ||
        (row.hasLeft && row.left.directory) || (row.hasRight && row.right.directory)) return false;
    if(command==RestoreProposal) {
        if(index>=p.defaults.size()) return false;action=p.defaults[index];
    } else if(command==ReverseProposal) {
        if(p.options.asymmetric) return false;
        if(row.action==dirsync::Action::CopyRight) action=dirsync::Action::CopyLeft;
        else if(row.action==dirsync::Action::CopyLeft) action=dirsync::Action::CopyRight;
        else return false;
    } else {
        if(command<0 || command>4) return false;action=static_cast<dirsync::Action>(command);
    }
    if(action==dirsync::Action::CopyRight && !row.hasLeft) return false;
    if(action==dirsync::Action::CopyLeft && (!row.hasRight || p.options.asymmetric)) return false;
    if(action==dirsync::Action::DeleteRight && (!p.options.asymmetric || !row.hasRight || row.hasLeft)) return false;
    return action!=dirsync::Action::Blocked;
}
std::pair<uint64_t,uint64_t> ChangeProposals(Preview& p,const std::vector<size_t>& rows,int command)
{
    uint64_t changed=0,unchanged=0;
    for (size_t index : rows)
    {
        dirsync::Action action{};
        if(!Proposal(p,index,command,action) || p.result.rows[index].action==action) {++unchanged;continue;}
        p.result.rows[index].action=action;++changed;
    }
    return {changed,unchanged};
}
void Apply(Preview& p)
{
    const auto counts=ChangeProposals(p,p.requestedRows,p.requestedAction);
    if(p.requestedRows.empty()) Message(p.window, IDS_SELECT_ROW);
    Filter(p);
    wchar_t status[256]{};swprintf_s(status,Text(IDS_PROPOSALS_CHANGED).c_str(),counts.first,counts.second);StatusPane(p.window,0,status);
}
std::wstring RowPaths(const Preview& p,const std::vector<size_t>& rows,int side)
{
    std::wstring text,root;
    if(side && !dirsync::NormalizeRoot(side==1?p.left:p.right,root)) return text;
    for(size_t index:rows) {
        if(index>=p.result.rows.size()) continue;const auto& row=p.result.rows[index];
        if((side==1 && !row.hasLeft) || (side==2 && !row.hasRight)) continue;
        if(!text.empty()) text+=L"\r\n";
        if(side) {text+=root;if(root.back()!=L'\\') text+=L"\\";}
        text+=row.path;
    }
    return text;
}
// These commands always use the visible plan, even with no highlighted rows.
constexpr int PlanSummary=30,PlanSave=31,PlanCopy=32,PlanOverwrite=33,PlanDelete=34;
Preview::Intent PlanIntent(int command)
{
    switch(command) {
    case PlanSummary:return Preview::Intent::Summary;case PlanSave:return Preview::Intent::Save;
    case PlanCopy:return Preview::Intent::Copy;case PlanOverwrite:return Preview::Intent::Overwrite;
    case PlanDelete:return Preview::Intent::Delete;default:return Preview::Intent::None;
    }
}
bool PlanEnabled(const Preview& p,int command)
{
    if(p.running || !p.result.snapshot || p.result.stale || p.result.cancelled || p.failed) return false;
    if(command==PlanSummary) return true;
    if(command==PlanSave && !settings.keepOriginals) return false;
    for(size_t index:p.visible) if(index<p.result.rows.size()) {
        const auto& row=p.result.rows[index];
        if(command==PlanCopy && dirsync::IsNewFileCopy(row)) return true;
        if((command==PlanOverwrite || command==PlanSave) && dirsync::IsFileOverwrite(row)) return true;
        if(command==PlanDelete && dirsync::SnapshotAsymmetric(p.result) && dirsync::IsFileDeletion(row)) return true;
    }
    return false;
}
void EnablePlanMenu(CGUIMenuPopupAbstract* menu,const Preview& p)
{
    for(int command=PlanSummary;command<=PlanDelete;++command) menu->EnableItem(command+1,FALSE,PlanEnabled(p,command));
}
void SynchronizeMenu(Preview& p)
{
    if(!gui || p.running) return;
    MENU_TEMPLATE_ITEM items[]={{MNTT_PB,0,0,0,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_COPY_NEW,MNTS_B|MNTS_I|MNTS_A,PlanCopy+1,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_OVERWRITE,MNTS_B|MNTS_I|MNTS_A,PlanOverwrite+1,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_DELETE_FILES,MNTS_B|MNTS_I|MNTS_A,PlanDelete+1,-1,0,nullptr},
        {MNTT_PE,0,0,0,-1,0,nullptr}};
    auto* menu=gui->CreateMenuPopup();if(!menu) return;DWORD command=0;
    if(menu->LoadFromTemplate(language,items,nullptr,nullptr,nullptr)) {
        EnablePlanMenu(menu,p);RECT r{};GetWindowRect(GetDlgItem(p.window,IDC_SYNCHRONIZE),&r);
        command=menu->Track(MENU_TRACK_RETURNCMD|MENU_TRACK_NONOTIFY,r.left,r.bottom,p.window,nullptr);
    }
    gui->DestroyMenuPopup(menu);
    if(command && PlanEnabled(p,int(command)-1)) ValidatePreview(p,PlanIntent(int(command)-1));
}
void PreviewCommand(Preview& p,const std::vector<size_t>& rows,int command)
{
    if(p.running) return;
    if(PlanIntent(command)!=Preview::Intent::None) {
        if(PlanEnabled(p,command)) ValidatePreview(p,PlanIntent(command));return;
    }
    if(rows.empty()) return;
    if(command>=10 && command<=12) {
        auto text=RowPaths(p,rows,command-10);
        if(!text.empty() && (text.size()>INT_MAX || !general->CopyTextToClipboardW(text.c_str(),int(text.size()),FALSE,p.window)))
            Message(p.window,IDS_UI_CLIPBOARD_ERROR);
    } else ValidatePreview(p,Preview::Intent::Apply,&rows,command);
}
int PreviewHit(HWND list,POINT point)
{
    RECT client{};GetClientRect(list,&client);if(!PtInRect(&client,point)) return -1;
    int count=ListView_GetItemCount(list),first=ListView_GetTopIndex(list);
    int end=std::min(count,first+ListView_GetCountPerPage(list)+2);
    for(int i=first;i<end;++i) {
        RECT row{};
        if(ListView_GetItemRect(list,i,&row,LVIR_BOUNDS) && point.y>=row.top && point.y<row.bottom) return i;
    }
    return -1;
}
void PreviewMenu(Preview& p,LPARAM position)
{
    if(!gui || p.running) return;HWND list=GetDlgItem(p.window,IDC_LIST);
    POINT point{static_cast<short>(LOWORD(position)),static_cast<short>(HIWORD(position))};
    if(position==LPARAM(-1)) {
        int index=ListView_GetNextItem(list,-1,LVNI_FOCUSED);RECT r{};
        if(index>=0 && ListView_GetItemRect(list,index,&r,LVIR_BOUNDS)) point={r.left,r.bottom};else point={0,0};
        ClientToScreen(list,&point);
    } else {
        POINT client=point;ScreenToClient(list,&client);
        int index=PreviewHit(list,client);
        if(index>=0 && !(ListView_GetItemState(list,index,LVIS_SELECTED)&LVIS_SELECTED)) {
            ListView_SetItemState(list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
            ListView_SetItemState(list,index,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        }
    }
    const auto rows=SelectedRows(p);
    MENU_TEMPLATE_ITEM items[]={ {MNTT_PB,0,0,0,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_RESTORE,MNTS_B|MNTS_I|MNTS_A,RestoreProposal+1,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_SKIP,MNTS_B|MNTS_I|MNTS_A,1,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_RIGHT,MNTS_B|MNTS_I|MNTS_A,2,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_LEFT,MNTS_B|MNTS_I|MNTS_A,3,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_REVERSE,MNTS_B|MNTS_I|MNTS_A,ReverseProposal+1,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_DELETE,MNTS_B|MNTS_I|MNTS_A,4,-1,0,nullptr},
        {MNTT_SP,-1,MNTS_B|MNTS_I|MNTS_A,0,-1,0,nullptr},
        {MNTT_PB,IDS_MENU_PATHS,MNTS_B|MNTS_I|MNTS_A,20,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_RELATIVE,MNTS_B|MNTS_I|MNTS_A,11,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_FULL_LEFT,MNTS_B|MNTS_I|MNTS_A,12,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_FULL_RIGHT,MNTS_B|MNTS_I|MNTS_A,13,-1,0,nullptr},
        {MNTT_PE,0,0,0,-1,0,nullptr},
        {MNTT_SP,-1,MNTS_B|MNTS_I|MNTS_A,0,-1,0,nullptr},
        {MNTT_PB,IDS_MENU_PLAN,MNTS_B|MNTS_I|MNTS_A,21,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_SUMMARY,MNTS_B|MNTS_I|MNTS_A,PlanSummary+1,-1,0,nullptr},
        {MNTT_SP,-1,MNTS_B|MNTS_I|MNTS_A,0,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_COPY_NEW,MNTS_B|MNTS_I|MNTS_A,PlanCopy+1,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_OVERWRITE,MNTS_B|MNTS_I|MNTS_A,PlanOverwrite+1,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_DELETE_FILES,MNTS_B|MNTS_I|MNTS_A,PlanDelete+1,-1,0,nullptr},
        {MNTT_SP,-1,MNTS_B|MNTS_I|MNTS_A,0,-1,0,nullptr},
        {MNTT_IT,IDS_MENU_SAVE,MNTS_B|MNTS_I|MNTS_A,PlanSave+1,-1,0,nullptr},
        {MNTT_PE,0,0,0,-1,0,nullptr},{MNTT_PE,0,0,0,-1,0,nullptr}};
    auto* menu=gui->CreateMenuPopup();if(!menu) return;DWORD command=0;
    if(menu->LoadFromTemplate(language,items,nullptr,nullptr,nullptr)) {
        if(auto* plan=menu->GetSubMenu(21,FALSE)) EnablePlanMenu(plan,p);
        for(int proposal:{0,1,2,3,RestoreProposal,ReverseProposal}) {
            bool enabled=false;dirsync::Action action{};
            if(p.result.snapshot && !p.result.stale && !p.result.cancelled && !p.failed)
                for(size_t index:rows) if(Proposal(p,index,proposal,action)) {enabled=true;break;}
            menu->EnableItem(proposal+1,FALSE,enabled);
        }
        if(auto* paths=menu->GetSubMenu(20,FALSE))
            for(int side=0;side<3;++side) paths->EnableItem(11+side,FALSE,std::any_of(rows.begin(),rows.end(),[&](size_t index) {
                return index<p.result.rows.size() && (!side || (side==1?p.result.rows[index].hasLeft:p.result.rows[index].hasRight));
            }));
        command=menu->Track(MENU_TRACK_RETURNCMD|MENU_TRACK_RIGHTBUTTON|MENU_TRACK_NONOTIFY,point.x,point.y,p.window,nullptr);
    }
    gui->DestroyMenuPopup(menu);
    if(command) PreviewCommand(p,rows,int(command)-1);
}
LRESULT CALLBACK PreviewList(HWND list,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data)
{
    if(message==WM_RBUTTONDOWN) {
        SetFocus(list);POINT point{static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};
        int index=PreviewHit(list,point);
        if(index>=0 && !(ListView_GetItemState(list,index,LVIS_SELECTED)&LVIS_SELECTED)) {
            ListView_SetItemState(list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
            ListView_SetItemState(list,index,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        }
        return 0;
    }
    if(message==WM_RBUTTONUP) {
        POINT point{static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};ClientToScreen(list,&point);
        SendMessageW(GetParent(list),WM_CONTEXTMENU,WPARAM(list),MAKELPARAM(point.x,point.y));return 0;
    }
    if(message==WM_NCDESTROY) RemoveWindowSubclass(list,PreviewList,id);
    (void)data;return DefSubclassProc(list,message,w,l);
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
    Busy(p,true); SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,StopControl(p))),TRUE);
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
    p.recoveryRoot=settings.recoveryRoot;
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
    Busy(p,true); SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,StopControl(p))),TRUE);
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
    const bool keep=settings.keepOriginals; const auto root=settings.recoveryRoot;
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
    Busy(p,true); SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,StopControl(p))),TRUE);
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
    const bool keep=settings.keepOriginals; const auto root=settings.recoveryRoot;
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
    Busy(p,true); SendMessageW(p.window,WM_NEXTDLGCTL,WPARAM(GetDlgItem(p.window,StopControl(p))),TRUE);
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
        SetDlgItemTextW(window, IDC_MASK, L"*.*");
        CheckDlgButton(window, IDC_RECURSIVE, BST_CHECKED);
        CheckDlgButton(window, IDC_SELECTED, p->options.selection.empty() ? BST_UNCHECKED : BST_CHECKED);
        const int filters[]={IDC_EQUAL,IDC_DIFFERENT,IDC_SINGLE,IDC_TORIGHT,IDC_TOLEFT};
        for(size_t i=0;i<p->filterChecks.size();++i) {
            p->filterChecks[i]=true;
            CheckDlgButton(window,filters[i],BST_CHECKED);
        }
        HWND list = GetDlgItem(window, IDC_LIST);
        SetWindowSubclass(list,PreviewList,1,0);
        // Existing darkmodelib opt-in forwards only this list's custom drawing
        // to the plugin, preserving the shared theming of all other controls.
        SetPropW(list,L"Salamander.DarkModeLib.CustomListView",HANDLE(1));
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP | LVS_EX_INFOTIP);
        int widths[] = {200, 60, 120, 145, 60, 120, 120, 250, 60};
        HDC dc = GetDC(window);
        UINT dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(window, dc);
        ListView_SetUnicodeFormat(list, TRUE);
        // Column captions and actions always come from the language module.
        for (int i = 0; i < 9; ++i)
        { std::wstring title = Text(IDS_COLUMNS + i); LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = &title[0]; c.cx = MulDiv(widths[i], dpi, 96); SendMessageW(list, LVM_INSERTCOLUMNW, i, LPARAM(&c)); }
        RECT client{}, outer{}; GetClientRect(window, &client); GetWindowRect(window, &outer);
        p->initialClient = {client.right, client.bottom};
        p->minimumWindow = {outer.right - outer.left, outer.bottom - outer.top};
        for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        {
            if(GetDlgCtrlID(child)==IDC_STATUS) continue;
            RECT bounds{}; GetWindowRect(child, &bounds); MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&bounds), 2);
            p->controls.push_back({child, bounds});
        }
        ConfigureDark(window);ButtonIcons(*p); Tooltips(window);ActionColumn(window);SizeStatus(window); Busy(*p, false); Status(*p); return TRUE;
    }
    if (!p) return FALSE;
    if(message==WM_CTLCOLORBTN && (GetWindowLongPtrW(reinterpret_cast<HWND>(l),GWL_STYLE)&BS_PUSHLIKE)) {
        // Keep the native push-like painter; the general color helper would
        // reinstall the ordinary checkbox subclass during every paint.
        HDC dc=reinterpret_cast<HDC>(w);SetBkMode(dc,TRANSPARENT);
        return reinterpret_cast<INT_PTR>(backgroundBrush?backgroundBrush:GetSysColorBrush(COLOR_BTNFACE));
    }
    LRESULT brush = 0;
    if (dark && DarkModeHandleCtlColor(message, w, l, brush)) return brush;
    switch (message)
    {
    case WM_HELP: {
        auto* help=reinterpret_cast<HELPINFO*>(l);
        int id=help?GetDlgCtrlID(reinterpret_cast<HWND>(help->hItemHandle)):0;
        Message(window,id==IDC_MASK?IDS_TIP_MASK:id==IDC_KEEP_ORIGINALS?IDS_TIP_KEEP:
            id==IDC_STOP?IDS_TIP_STOP_COMPARE:id==IDC_STOP_SYNC?IDS_TIP_STOP_SYNC:id==IDC_SYNCHRONIZE?IDS_TIP_SCOPE:IDS_HELP);
        return TRUE;
    }
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
                else if (id == IDC_LEFT) r.right += dx/2;
                else if (id == IDC_BROWSE_LEFT) {r.left+=dx/2;r.right+=dx/2;}
                else if (id == IDC_BROWSE_RIGHT) {r.left+=dx;r.right+=dx;}
                else if (id == IDC_RIGHT) { r.left += dx/2; r.right += dx; }
                else if (id == IDC_RIGHT_LABEL) { r.left += dx/2; r.right += dx/2; }
                else if (id == IDC_COMPARE || id == IDC_STOP || id == IDC_SYNCHRONIZE || id == IDC_STOP_SYNC || id == IDCANCEL) {r.left+=dx;r.right+=dx;}
                else if (id==IDC_EQUAL || id==IDC_DIFFERENT || id==IDC_SINGLE || id==IDC_TORIGHT || id==IDC_TOLEFT)
                {r.left+=dx/2;r.right+=dx/2;}
                MoveWindow(control.first, r.left, r.top, r.right - r.left, r.bottom - r.top, FALSE);
            }
        }
        SizeStatus(window);
        // Erase exposed old child bounds as well as repainting the moved controls.
        RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
        return TRUE;
    case WM_TIMER: Status(*p); return TRUE;
    case Done:
        p->Join(); p->running = false; KillTimer(window, 1);
        if(!p->validating && !p->copying && !p->saving && !p->overwriting && !p->deleting) {
            p->defaults.clear();for(const auto& row:p->result.rows) p->defaults.push_back(row.action);
        }
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
            if(!p->overwriteReport.recovery.runDirectory.empty()) NotifyPath(settings.recoveryRoot);
            if(!p->closing) { if(p->failed) Message(window,IDS_OVERWRITE_FAILED); else OverwriteSummary(*p); }
        }
        if(p->deleting) {
            p->deleting=false; p->hasDeleteReport=true; p->result.stale=true; p->result.snapshot.reset();
            Status(*p);
            NotifyPath(p->right);
            if(!p->deleteReport.recovery.runDirectory.empty()) NotifyPath(settings.recoveryRoot);
            if(!p->closing) { if(p->failed) Message(window,IDS_DELETE_FAILED); else DeleteSummary(*p); }
        }
        if (p->closing) { PersistSettings(window); EndDialog(window, 0); }
        else
        {
            HWND focused = GetFocus();
            bool restoreFocus = !focused || focused == GetDlgItem(window, IDC_STOP) || focused == GetDlgItem(window, IDC_STOP_SYNC);
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
    case WM_SETTINGCHANGE: case WM_THEMECHANGED: ConfigureDark(window);ButtonIcons(*p); return TRUE;
    case WM_CONTEXTMENU:
        if(reinterpret_cast<HWND>(w)==GetDlgItem(window,IDC_LIST)) {PreviewMenu(*p,l);return TRUE;}
        break;
    case WM_COMMAND:
        switch (LOWORD(w))
        {

        case IDC_COMPARE: Start(*p); return TRUE;
        case IDC_BROWSE_LEFT: BrowseFolder(window,IDC_LEFT,IDS_BROWSE_LEFT);return TRUE;
        case IDC_BROWSE_RIGHT: BrowseFolder(window,IDC_RIGHT,IDS_BROWSE_RIGHT);return TRUE;
        case IDC_STOP: case IDC_STOP_SYNC:
            if(p->running && LOWORD(w)==StopControl(*p)) {p->Stop();Status(*p);} return TRUE;
        case IDC_SYNCHRONIZE: SynchronizeMenu(*p);return TRUE;
        case IDCANCEL: SendMessageW(window, WM_CLOSE, 0, 0); return TRUE;
        case IDC_SUMMARY: ValidatePreview(*p, Preview::Intent::Summary); return TRUE;
        case IDC_COPY_NEW: ValidatePreview(*p, Preview::Intent::Copy); return TRUE;
        case IDC_SAVE_ORIGINALS: ValidatePreview(*p, Preview::Intent::Save); return TRUE;
        case IDC_OVERWRITE: ValidatePreview(*p,Preview::Intent::Overwrite); return TRUE;
        case IDC_DELETE_FILES: ValidatePreview(*p,Preview::Intent::Delete); return TRUE;
        case IDC_EQUAL: case IDC_DIFFERENT: case IDC_SINGLE: case IDC_TORIGHT: case IDC_TOLEFT:
            if (!p->running && HIWORD(w)==BN_CLICKED) {
                const int filters[]={IDC_EQUAL,IDC_DIFFERENT,IDC_SINGLE,IDC_TORIGHT,IDC_TOLEFT};
                for(size_t i=0;i<p->filterChecks.size();++i) if(filters[i]==LOWORD(w)) {
                    p->filterChecks[i]=!p->filterChecks[i];
                    CheckDlgButton(window,filters[i],p->filterChecks[i]?BST_CHECKED:BST_UNCHECKED);
                    break;
                }
                Filter(*p);
            }
            return TRUE;
        case IDC_SELECTED: case IDC_RECURSIVE: case IDC_CONTENT: case IDC_IGNORE: case IDC_ASYMMETRIC: case IDC_EMPTY:
            if(HIWORD(w)==BN_CLICKED && !p->running) {Busy(*p,false);Status(*p);} return TRUE;
        case IDC_LEFT: case IDC_RIGHT: case IDC_MASK:
            if (HIWORD(w) == EN_CHANGE) InvalidatePlan(*p); return TRUE;
        }
        break;
    case WM_NOTIFY:
        if(reinterpret_cast<NMHDR*>(l)->idFrom==IDC_LIST && reinterpret_cast<NMHDR*>(l)->code==LVN_KEYDOWN) {
            auto* key=reinterpret_cast<NMLVKEYDOWN*>(l);
            if((GetKeyState(VK_CONTROL)&0x8000) && !(GetKeyState(VK_MENU)&0x8000) && !(GetKeyState(VK_SHIFT)&0x8000)) {
                int command=-1;
                switch(key->wVKey) {
                case 'D':command=RestoreProposal;break;case 'M':command=0;break;case 'L':command=1;break;
                case 'R':command=2;break;case 'W':command=ReverseProposal;break;case 'C':command=10;break;
                case 'A':if(!p->running) ListView_SetItemState(GetDlgItem(window,IDC_LIST),-1,LVIS_SELECTED,LVIS_SELECTED);return TRUE;
                }
                if(command>=0) {PreviewCommand(*p,SelectedRows(*p),command);return TRUE;}
            }
        }
        if(reinterpret_cast<NMHDR*>(l)->code==TTN_GETDISPINFOW) {
            auto* tip=reinterpret_cast<NMTTDISPINFOW*>(l);
            p->tipText.clear();POINT point{};GetCursorPos(&point);ScreenToClient(GetDlgItem(window,IDC_LIST),&point);
            LVHITTESTINFO hit{};hit.pt=point;int index=ListView_SubItemHitTest(GetDlgItem(window,IDC_LIST),&hit);
            if(index>=0 && size_t(index)<p->visible.size() && hit.iSubItem==3)
                p->tipText=Text(IDS_ACTION+int(p->result.rows[p->visible[size_t(index)]].action));
            tip->lpszText=p->tipText.data();return TRUE;
        }
        if(reinterpret_cast<NMHDR*>(l)->idFrom==IDC_LIST && reinterpret_cast<NMHDR*>(l)->code==LVN_GETINFOTIPW) {
            auto* tip=reinterpret_cast<NMLVGETINFOTIPW*>(l);
            if(tip->iItem>=0 && size_t(tip->iItem)<p->visible.size() && tip->cchTextMax>0) {
                auto text=Text(IDS_ACTION+int(p->result.rows[p->visible[size_t(tip->iItem)]].action));
                size_t count=std::min(text.size(),size_t(tip->cchTextMax-1));
                if(count && count<text.size() && IS_HIGH_SURROGATE(text[count-1])) --count;
                memcpy(tip->pszText,text.data(),count*sizeof(wchar_t));tip->pszText[count]=0;
            }
            return TRUE;
        }
        if(reinterpret_cast<NMHDR*>(l)->idFrom==IDC_LIST && reinterpret_cast<NMHDR*>(l)->code==LVN_ITEMCHANGED) {
            int index=ListView_GetNextItem(GetDlgItem(window,IDC_LIST),-1,LVNI_FOCUSED);
            if(!p->running && index>=0 && size_t(index)<p->visible.size()) {
                if(PendingOptions(*p)) Status(*p);
                else StatusPane(window,0,Text(IDS_ACTION+int(p->result.rows[p->visible[size_t(index)]].action)));
            }
        }
        if(reinterpret_cast<NMHDR*>(l)->idFrom==IDC_LIST && reinterpret_cast<NMHDR*>(l)->code==NM_CUSTOMDRAW) {
            auto* draw=reinterpret_cast<NMLVCUSTOMDRAW*>(l);
            LRESULT result=CDRF_DODEFAULT;
            if(draw->nmcd.dwDrawStage==(CDDS_ITEMPREPAINT|CDDS_SUBITEM) &&
               draw->nmcd.dwItemSpec<p->visible.size() &&
               !(ListView_GetItemState(draw->nmcd.hdr.hwndFrom,int(draw->nmcd.dwItemSpec),LVIS_SELECTED)&LVIS_SELECTED))
                draw->clrText=PreviewColor(p->result.rows[p->visible[size_t(draw->nmcd.dwItemSpec)]],draw->iSubItem,
                    ListView_GetBkColor(draw->nmcd.hdr.hwndFrom),ListView_GetTextColor(draw->nmcd.hdr.hwndFrom));
            if(draw->nmcd.dwDrawStage==CDDS_PREPAINT) result=CDRF_NOTIFYITEMDRAW;
            else if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT) result=CDRF_NOTIFYSUBITEMDRAW;
            else if((draw->iSubItem==3 || draw->iSubItem==6) && draw->nmcd.dwItemSpec<p->visible.size()) {
                const auto& row=p->result.rows[p->visible[size_t(draw->nmcd.dwItemSpec)]];
                Mark mark=RowMark(row,draw->iSubItem==3);
                bool selected=(ListView_GetItemState(draw->nmcd.hdr.hwndFrom,int(draw->nmcd.dwItemSpec),LVIS_SELECTED)&LVIS_SELECTED)!=0;
                if(draw->nmcd.dwDrawStage==(CDDS_ITEMPREPAINT|CDDS_SUBITEM)) {
                    result=CDRF_NOTIFYPOSTPAINT;
                } else if(draw->nmcd.dwDrawStage==(CDDS_ITEMPOSTPAINT|CDDS_SUBITEM)) {
                    RECT cell{};
                    if(ListView_GetSubItemRect(draw->nmcd.hdr.hwndFrom,int(draw->nmcd.dwItemSpec),draw->iSubItem,LVIR_BOUNDS,&cell)) {
                        bool iconOnly=draw->iSubItem==3 && !settings.actionLabels;
                        if(iconOnly) {
                            // Keep the native/theme-painted selection background, including
                            // inactive selection. The leading text padding leaves this pixel empty.
                            COLORREF bg=GetPixel(draw->nmcd.hdc,cell.left+2,(cell.top+cell.bottom)/2);
                            if(bg==CLR_INVALID) bg=ListView_GetBkColor(draw->nmcd.hdr.hwndFrom);
                            HBRUSH fill=CreateSolidBrush(bg);FillRect(draw->nmcd.hdc,&cell,fill);DeleteObject(fill);
                        }
                        COLORREF color=PreviewColor(row,draw->iSubItem,ListView_GetBkColor(draw->nmcd.hdr.hwndFrom),
                            ListView_GetTextColor(draw->nmcd.hdr.hwndFrom));
                        if(selected) color=GetFocus()==draw->nmcd.hdr.hwndFrom?GetSysColor(COLOR_HIGHLIGHTTEXT):ListView_GetTextColor(draw->nmcd.hdr.hwndFrom);
                        PaintMark(draw->nmcd.hdc,cell,mark,color,iconOnly);
                    }
                }
            }
            SetWindowLongPtrW(window,DWLP_MSGRESULT,result);return TRUE;
        }
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
    void WINAPI Configuration(HWND parent) override { ConfigureDark(nullptr);ConfigureAppearance(parent); }
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
    gui = entry->GetSalamanderGUI();
    language = entry->LoadLanguageModule(entry->GetParentWindow(), "Directory Synchronization");
    if (!language) return nullptr;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES}; InitCommonControlsEx(&controls);
    ConfigureDark(nullptr);
    entry->SetBasicPluginData(general->LoadStr(language, IDS_NAME), FUNCTION_LOADSAVECONFIGURATION|FUNCTION_CONFIGURATION, "0.1", "Copyright (c) 2026 Open Salamander Authors",
                              general->LoadStr(language, IDS_DESCRIPTION), "DirectorySync");
    return &plugin;
}
