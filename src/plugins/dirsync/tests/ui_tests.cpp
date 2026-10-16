// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// Inspect the compiled shipping dialog, without creating or automating a window.
// Explicit font DPI tests do not simulate a monitor or WM_DPICHANGED.
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <stdexcept>
#include <cwctype>
#include <cstring>
#include "../resource.h"
namespace
{
int checks = 0;
void Check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
struct Reader
{
    const BYTE* at; const BYTE* end;
    template<class T> T Read()
    {
        Check(size_t(end - at) >= sizeof(T), "resource bounds");
        T value; memcpy(&value, at, sizeof(T)); at += sizeof(T); return value;
    }
    void Align() { while (reinterpret_cast<UINT_PTR>(at) % 4) Read<BYTE>(); }
    std::wstring Text()
    {
        WORD c = Read<WORD>(); if (c == 0xffff) { Read<WORD>(); return {}; }
        std::wstring text; while (c) { text += wchar_t(c); c = Read<WORD>(); } return text;
    }
};
struct Control { DWORD style, id; short x, y, width, height; WORD type; std::wstring text, className; };
std::wstring Caption(const std::wstring& text)
{
    std::wstring value;
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != L'&') value += text[i];
        else if (i + 1 < text.size() && text[i + 1] == L'&') { value += L'&'; ++i; }
    }
    return value;
}
void Test(int dialog)
{
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(dialog), MAKEINTRESOURCEW(5));
    Check(resource != nullptr, "shipping dialog resource");
    const BYTE* bytes = static_cast<const BYTE*>(LockResource(LoadResource(nullptr, resource)));
    Check(bytes != nullptr, "dialog data");
    Reader r{bytes, bytes + SizeofResource(nullptr, resource)};
    Check(r.Read<WORD>() == 1 && r.Read<WORD>() == 0xffff, "extended dialog");
    r.Read<DWORD>(); r.Read<DWORD>(); DWORD style = r.Read<DWORD>(); WORD count = r.Read<WORD>();
    r.Read<short>(); r.Read<short>(); short width = r.Read<short>(), height = r.Read<short>();
    r.Text(); r.Text(); r.Text(); Check((style & DS_SETFONT) != 0, "dialog font");
    WORD points = r.Read<WORD>(), weight = r.Read<WORD>(); BYTE italic = r.Read<BYTE>(), charset = r.Read<BYTE>();
    std::wstring face = r.Text(); Check(face == L"MS Shell Dlg", "repository dialog font");
    std::vector<Control> controls; std::set<wchar_t> keys;
    for (WORD i = 0; i < count; ++i)
    {
        r.Align(); r.Read<DWORD>(); r.Read<DWORD>(); Control c{}; c.style = r.Read<DWORD>();
        c.x = r.Read<short>(); c.y = r.Read<short>(); c.width = r.Read<short>(); c.height = r.Read<short>(); c.id = r.Read<DWORD>();
        WORD type = r.Read<WORD>();
        if (type == 0xffff) c.type = r.Read<WORD>();
        else while (type) {c.className+=wchar_t(type);type = r.Read<WORD>();}
        c.text = r.Text(); WORD extra = r.Read<WORD>(); for (WORD j = 0; j < extra; ++j) r.Read<BYTE>();
        for (size_t j = 0; j + 1 < c.text.size(); ++j) if (c.text[j] == L'&')
        {
            if (c.text[j + 1] == L'&') { ++j; continue; }
            Check(keys.insert(wchar_t(towlower(c.text[j + 1]))).second, "unique dialog mnemonic");
        }
        if (c.id == IDC_LIST || c.id == IDC_ACTION || c.id == IDC_COMPARE || c.id == IDC_COPY_NEW ||
            c.id == IDC_RECOVERY_ROOT || c.id == IDC_SAVE_ORIGINALS || c.id == IDC_KEEP_ORIGINALS || c.id == IDC_OVERWRITE || c.id == IDC_DELETE_FILES || c.id == IDCANCEL ||
            c.id == IDC_INFO_TABLE || c.id == IDC_INFO_NOTES || c.id == IDC_CONFIRM_DISCARD || c.id == IDC_DONT_ASK || c.id == IDOK)
            Check((c.style & WS_TABSTOP) != 0, "primary controls reachable by Tab");
        if(dialog==IDD_DISCARD_RESULTS && c.id==IDCANCEL)
            Check((c.style&BS_TYPEMASK)==BS_DEFPUSHBUTTON,"discard warning defaults to Cancel");
        if(c.id==IDC_STATUS) Check(c.className==L"msctls_statusbar32", "real native status bar");
        if(c.id==IDC_INFO_NOTES)
            Check((c.style&ES_READONLY)!=0 && (c.style&ES_MULTILINE)!=0 && (c.style&WS_VSCROLL)!=0,"selectable scrolling read-only details");
        controls.push_back(c);
    }
    if(dialog==IDD_PREVIEW) {
        auto control=[&](DWORD id)->const Control& { for(const auto& c:controls) if(c.id==id) return c; throw std::runtime_error("missing preview control"); };
        const auto& left=control(IDC_LEFT);const auto& right=control(IDC_RIGHT);
        Check(left.y==right.y && left.x+left.width<right.x,"roots side by side without overlap");
        Check(left.width==right.width,"equally sized root inputs");
        for(DWORD id:{IDC_CONTENT,IDC_IGNORE,IDC_ASYMMETRIC,IDC_SELECTED,IDC_EMPTY})
            Check(control(id).y==control(IDC_RECURSIVE).y,"scan choices in one row");
        Check(control(IDC_MASK).y+2==control(IDC_RECURSIVE).y && control(IDC_MASK).width<=60,"compact mask shares scan row");
        Check(control(IDC_LIST).height>height/2,"file list gets majority of initial height");
        for(const auto& c:controls) Check(c.id!=IDC_KEEP_ORIGINALS && c.id!=IDC_RECOVERY_ROOT,"recovery settings only in preferences");
    }
    if(dialog==IDD_APPEARANCE) {
        auto control=[&](DWORD id)->const Control& {for(const auto& c:controls) if(c.id==id) return c;throw std::runtime_error("missing preference control");};
        Check(control(IDC_COLOR_BUTTON_FIRST).y==control(IDC_COLOR_BUTTON_FIRST+4).y &&
            control(IDC_COLOR_BUTTON_FIRST+4).x>control(IDC_COLOR_BUTTON_FIRST).x,"two columns of color choices");
        Check(control(IDC_KEEP_ORIGINALS).y>control(IDC_COLOR_BUTTON_FIRST+3).y,"recovery settings below colors");
        Check(control(IDC_RECOVERY_ROOT).width>=200,"usable Unicode recovery path editor");
    }
    for (int dpi : {96, 120, 144, 192, 240})
    {
        HDC dc = CreateCompatibleDC(nullptr); Check(dc != nullptr, "measurement DC");
        HFONT font = CreateFontW(-MulDiv(points, dpi, 72), 0, 0, 0, weight, italic, FALSE, FALSE,
            charset, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, face.c_str());
        Check(font != nullptr, "DPI font"); HGDIOBJ old = SelectObject(dc, font);
        TEXTMETRICW metrics{}; SIZE alphabet{};
        Check(GetTextMetricsW(dc, &metrics) != FALSE, "font metrics");
        Check(GetTextExtentPoint32W(dc, L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, &alphabet) != FALSE, "dialog base units");
        int baseX = (alphabet.cx / 26 + 1) / 2, baseY = metrics.tmHeight;
        bool fits = true;
        std::vector<RECT> bounds;
        for (const auto& c : controls)
        {
            // Combo height includes the open dropdown, not only its closed face.
            int h = c.type == 0x85 ? 14 : c.height;
            fits &= c.x >= 0 && c.y >= 0 && c.x + c.width <= width && c.y + h <= height;
            RECT box{MulDiv(c.x, baseX, 4), MulDiv(c.y, baseY, 8),
                MulDiv(c.x + c.width, baseX, 4), MulDiv(c.y + h, baseY, 8)};
            for (const RECT& other : bounds) { RECT intersection{}; fits &= !IntersectRect(&intersection, &box, &other); }
            bounds.push_back(box);
            if (c.text.empty()) continue;
            auto text = Caption(c.text); SIZE size{};
            Check(GetTextExtentPoint32W(dc, text.c_str(), int(text.size()), &size) != FALSE, "caption metrics");
            int padding = c.type == 0x80 ? MulDiv((c.style & 0xf) == BS_AUTOCHECKBOX ? 20 : 12, dpi, 96) : 0;
            if (size.cx + padding > MulDiv(c.width, baseX, 4) || size.cy > MulDiv(h, baseY, 8))
            { std::wcerr << L"DPI " << dpi << L": clipped caption " << c.text << L"\n"; fits = false; }
            if(dialog==IDD_INFORMATION && c.id==IDOK) {
                for(int id:{IDS_UI_SUMMARY,IDS_UI_START_COPY,IDS_UI_START_SAVE,IDS_UI_START_OVERWRITE,IDS_UI_START_DELETE}) {
                    wchar_t label[128]{};int n=LoadStringW(GetModuleHandleW(nullptr),id,label,128);
                    Check(n>0,"localized action caption present"); SIZE action{};
                    Check(GetTextExtentPoint32W(dc,label,n,&action)!=FALSE,"action caption metrics");
                    Check(action.cx+MulDiv(12,dpi,96)<=MulDiv(c.width,baseX,4),"dynamic action captions fit");
                }
            }
        }
        SelectObject(dc, old); DeleteObject(font); DeleteDC(dc);
        Check(fits, "dialog bounds and captions fit at requested font DPI");
        std::cout << dpi << " DPI: compiled dialog geometry and caption metrics passed\n";
    }
}
}
int main()
{
    try {
        // The 0.16 host reloads registered metadata into 260-byte buffers.
        // A longer description installs successfully but drops the plugin on restart.
        for(int id:{IDS_NAME,IDS_DESCRIPTION,IDS_MENU}) {
            const wchar_t* text=nullptr;
            int n=LoadStringW(GetModuleHandleW(nullptr),id,reinterpret_cast<LPWSTR>(&text),0);
            Check(n>0,"registration metadata present");
            int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,n,nullptr,0,nullptr,nullptr);
            Check(bytes>0 && bytes<260,"registration metadata fits existing host UTF-8 limit including terminator");
        }
        Test(IDD_PREVIEW); Test(IDD_INFORMATION);Test(IDD_APPEARANCE);Test(IDD_DISCARD_RESULTS); std::cout << checks << " resource/layout checks passed\n"; return 0;
    }
    catch (const std::exception& e) { std::cerr << "FAILED: " << e.what() << "\n"; return 1; }
}
