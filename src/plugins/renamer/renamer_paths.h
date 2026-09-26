// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <new>

namespace RenamerPaths
{
// A Win32 path can contain SAL_MAX_PATH UTF-16 units. Its UTF-8 form may
// need three bytes per unit; keep the terminator and expansion slack on the heap.
const int Capacity = 4 * SAL_MAX_PATH;

inline std::wstring ToWide(const char* text)
{
    if (text == NULL || *text == 0)
        return std::wstring();
    UINT cp = CP_UTF8;
    int count = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (count == 0)
    {
        cp = CP_ACP;
        count = MultiByteToWideChar(cp, 0, text, -1, NULL, 0);
    }
    if (count <= 0)
        return std::wstring();
    std::wstring result(count, L'\0');
    if (MultiByteToWideChar(cp, cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
                            text, -1, &result[0], count) == 0)
        return std::wstring();
    result.resize(count - 1);
    return result;
}

inline std::string ToUtf8(const wchar_t* text)
{
    if (text == NULL || *text == 0)
        return std::string();
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                         text, -1, NULL, 0, NULL, NULL);
    if (count <= 0)
        return std::string();
    std::string result(count, '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1,
                            &result[0], count, NULL, NULL) == 0)
        return std::string();
    result.resize(count - 1);
    return result;
}

// Call only while item still belongs to the source panel, on the host UI thread.
// An advertised service is authoritative: a failure must never fabricate a path.
inline BOOL ResolvePanelItemPath(CSalamanderGeneralAbstract* general, int panel,
                                 const CFileData* item, const char* panelRoot,
                                 std::string& fullPath)
{
    fullPath.clear();
    if (general == NULL || item == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    try
    {
        std::wstring path;
        CSalamanderServiceQuery query = {SALAMANDER_SERVICE_PANEL_ITEM_PATHS,
                                        SALAMANDER_PANEL_ITEM_PATHS_VERSION_1_0, 0};
        CSalamanderServiceResult result = {};
        if (general->QueryService(&query, &result))
        {
            if (result.Interface == NULL || result.Version < query.MinimumVersion)
            {
                SetLastError(ERROR_NOT_SUPPORTED);
                return FALSE;
            }
            std::vector<wchar_t> widePath(SAL_MAX_PATH, 0);
            CSalamanderPanelItemPathsAbstract* paths =
                static_cast<CSalamanderPanelItemPathsAbstract*>(result.Interface);
            if (!paths->GetItemFullPath(panel, item, widePath.data(), (int)widePath.size()))
                return FALSE;
            path = widePath.data();
        }
        else
        {
            path = ToWide(panelRoot);
            if (path.empty())
            {
                SetLastError(ERROR_INVALID_NAME);
                return FALSE;
            }
            if (path.back() != L'\\')
                path += L'\\';
            const std::wstring name = item->UseWideName() ? item->NameW : ToWide(item->Name);
            if (name.empty())
            {
                SetLastError(ERROR_INVALID_NAME);
                return FALSE;
            }
            path += name;
        }
        fullPath = ToUtf8(path.c_str());
        if (fullPath.empty())
        {
            SetLastError(ERROR_INVALID_NAME);
            return FALSE;
        }
        return TRUE;
    }
    catch (const std::bad_alloc&)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
}

// Compare complete Unicode scalars without allocating inside CRT sort callbacks.
// Ordinal ignore-case comparison does not perform linguistic expansions.
inline int NextCharacter(const char*& text, wchar_t (&wide)[2])
{
    const unsigned char lead = (unsigned char)*text;
    int bytes = lead < 0x80 ? 1 : (lead >= 0xC2 && lead <= 0xDF ? 2 :
                                  (lead >= 0xE0 && lead <= 0xEF ? 3 :
                                   (lead >= 0xF0 && lead <= 0xF4 ? 4 : 1)));
    for (int i = 1; i < bytes; ++i)
    {
        if (text[i] == 0 || ((unsigned char)text[i] & 0xC0) != 0x80)
        {
            bytes = 1;
            break;
        }
    }
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, bytes, wide, 2);
    if (length == 0)
    {
        // Invalid byte sequences have a deterministic order as well. Valid panel
        // identities are normalized to UTF-8 before they enter the source model.
        wide[0] = (wchar_t)(0xDC00 + lead);
        bytes = length = 1;
    }
    text += bytes;
    return length;
}

inline int Compare(const char* first, const char* second)
{
    while (*first != 0 && *second != 0)
    {
        wchar_t a[2], b[2];
        const int aLength = NextCharacter(first, a);
        const int bLength = NextCharacter(second, b);
        const int result = CompareStringOrdinal(a, aLength, b, bLength, TRUE);
        if (result != CSTR_EQUAL)
            return result - CSTR_EQUAL;
    }
    return *first != 0 ? 1 : (*second != 0 ? -1 : 0);
}
}
