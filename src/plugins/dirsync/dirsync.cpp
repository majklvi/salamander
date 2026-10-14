// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// Plugin lifecycle/resources and darkmode integration follow the repository's
// Split & Combine plugin. The comparison engine is independent of the SDK.
#include "precomp.h"
#include "engine.h"
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
std::wstring Text(int id)
{
    const wchar_t* value = nullptr;
    int count = LoadStringW(language, id, reinterpret_cast<LPWSTR>(&value), 0);
    return count > 0 ? std::wstring(value, count) : std::wstring();
}
void Message(HWND parent, int id)
{ DarkModeMessageBoxW(parent, Text(id).c_str(), Text(IDS_NAME).c_str(), MB_OK | MB_ICONINFORMATION); }
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
struct Preview
{
    HWND window = nullptr;
    std::wstring left, right;
    std::wstring panelLeft, panelRight;
    dirsync::Options options;
    dirsync::Result result;
    std::vector<size_t> visible;
    std::atomic<bool> cancel{false};
    dirsync::Progress progress;
    std::thread worker;
    SIZE initialClient{}, minimumWindow{};
    std::vector<std::pair<HWND, RECT>> controls;
    bool running = false, closing = false, failed = false;
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
void Status(Preview& p)
{
    if (p.running)
    {
        wchar_t text[256]{};
        swprintf_s(text, Text(IDS_SCAN).c_str(), p.progress.scanned.load(), p.progress.compared.load());
        SetDlgItemTextW(p.window, IDC_STATUS, text); return;
    }
    if (p.failed || p.result.cancelled)
    { SetDlgItemTextW(p.window, IDC_STATUS, Text(p.failed ? IDS_FAILED : IDS_CANCELLED).c_str()); return; }
    uint64_t blocked = 0;
    for (const auto& row : p.result.rows) if (row.state == dirsync::State::Blocked || row.state == dirsync::State::Conflict) ++blocked;
    wchar_t text[512]{};
    swprintf_s(text, Text(IDS_DONE).c_str(), uint64_t(p.result.rows.size()), uint64_t(p.visible.size()),
               uint64_t(p.result.rows.size() - p.visible.size()), blocked);
    SetDlgItemTextW(p.window, IDC_STATUS, text);
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
                   IDC_DIFFERENT, IDC_SINGLE, IDC_TORIGHT, IDC_TOLEFT})
        EnableWindow(GetDlgItem(p.window, id), !busy);
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
    p.cancel = false; p.progress.scanned = 0; p.progress.compared = 0;
    p.running = true; Busy(p, true);
    try
    {
        p.worker = std::thread([&p]() {
            try { p.result = dirsync::Compare(p.left, p.right, p.options, p.cancel, p.progress); }
            catch (...) { p.failed = true; p.result = {}; }
            PostMessageW(p.window, Done, 0, 0);
        });
        SetTimer(p.window, 1, 150, nullptr);
    }
    catch (...) { p.failed = true; p.running = false; Busy(p, false); Status(p); }
}
void Apply(Preview& p)
{
    int value = int(SendDlgItemMessageW(p.window, IDC_ACTION, CB_GETCURSEL, 0, 0));
    if (value < 0 || value > 4) return;
    auto action = static_cast<dirsync::Action>(value);
    HWND list = GetDlgItem(p.window, IDC_LIST);
    bool found = false;
    for (int i = ListView_GetNextItem(list, -1, LVNI_SELECTED); i >= 0; i = ListView_GetNextItem(list, i, LVNI_SELECTED))
    {
        found = true;
        auto& row = p.result.rows[p.visible[size_t(i)]];
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
    wchar_t text[2048]{};
    swprintf_s(text, Text(IDS_SUMMARY).c_str(), nr, br, nl, bl, deletes, unresolved, blocked,
               uint64_t(p.result.rows.size() - p.visible.size()));
    DarkModeMessageBoxW(p.window, text, Text(IDS_NAME).c_str(), MB_OK | MB_ICONINFORMATION);
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
            RECT bounds{}; GetWindowRect(child, &bounds); MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&bounds), 2);
            p->controls.push_back({child, bounds});
        }
        ConfigureDark(window); Busy(*p, false); return TRUE;
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
                else if (id == IDC_STATUS) { r.top += dy; r.bottom += dy; r.right += dx; }
                else if (id == IDC_ACTION || id == IDC_APPLY || id == IDC_SUMMARY || id == IDCANCEL)
                { r.top += dy; r.bottom += dy; if (id == IDCANCEL) { r.left += dx; r.right += dx; } }
                MoveWindow(control.first, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
            }
        }
        return TRUE;
    case WM_TIMER: Status(*p); return TRUE;
    case Done:
        p->Join(); p->running = false; KillTimer(window, 1);
        if (p->closing) EndDialog(window, 0);
        else { Busy(*p, false); Filter(*p); }
        return TRUE;
    case WM_CLOSE:
        if (p->running) { p->closing = true; p->Stop(); }
        else EndDialog(window, 0);
        return TRUE;
    case WM_SETTINGCHANGE: case WM_THEMECHANGED: ConfigureDark(window); return TRUE;
    case WM_COMMAND:
        switch (LOWORD(w))
        {
        case IDC_COMPARE: Start(*p); return TRUE;
        case IDC_STOP: p->Stop(); return TRUE;
        case IDCANCEL: SendMessageW(window, WM_CLOSE, 0, 0); return TRUE;
        case IDC_APPLY: Apply(*p); return TRUE;
        case IDC_SUMMARY: Summary(*p); return TRUE;
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
        return FALSE; // Read-only preview preserves panel selections.
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
    void WINAPI LoadConfiguration(HWND, HKEY, CSalamanderRegistryAbstract*) override {}
    void WINAPI SaveConfiguration(HWND, HKEY, CSalamanderRegistryAbstract*) override {}
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
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&controls);
    ConfigureDark(nullptr);
    entry->SetBasicPluginData(general->LoadStr(language, IDS_NAME), 0, "0.1", "Copyright (c) 2026 Open Salamander Authors",
                              general->LoadStr(language, IDS_DESCRIPTION), "DirectorySync");
    return &plugin;
}
