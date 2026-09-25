// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>

// Desktop-relative PIDLs retain the complete identity of each selected item. The
// default shell menu/data object can therefore represent duplicate basenames.
inline HRESULT CreateShellObjectForPaths(HWND owner, const std::vector<std::wstring>& paths,
                                         REFIID iid, void** object, BOOL contextMenu)
{
    *object = NULL;
    if (paths.empty())
        return E_INVALIDARG;
    for (size_t item = 0; item < paths.size(); ++item)
    {
        const std::wstring& path = paths[item];
        if (path.empty()) return E_INVALIDARG;
        for (size_t i = 0; i < path.size(); ++i)
        {
            if ((i + 1 == path.size() || path[i + 1] == L'\\' || path[i + 1] == L'/') &&
                (path[i] == L'.' || path[i] <= L' '))
                return HRESULT_FROM_WIN32(ERROR_INVALID_NAME); // the shell could normalize to another item
        }
    }
    IShellFolder* desktop = NULL;
    HRESULT result = SHGetDesktopFolder(&desktop);
    if (FAILED(result))
        return result;
    std::vector<LPITEMIDLIST> items;
    for (size_t i = 0; i < paths.size(); ++i)
    {
        LPITEMIDLIST item = NULL;
        ULONG eaten = 0;
        result = desktop->ParseDisplayName(owner, NULL, const_cast<wchar_t*>(paths[i].c_str()),
                                           &eaten, &item, NULL);
        if (FAILED(result))
            break; // never publish a partial selection after a parse failure
        items.push_back(item);
    }
    if (SUCCEEDED(result))
    {
        if (contextMenu)
        {
            DEFCONTEXTMENU menu = {};
            menu.hwnd = owner;
            menu.psf = desktop;
            menu.cidl = (UINT)items.size();
            menu.apidl = (PCUITEMID_CHILD_ARRAY)items.data();
            result = SHCreateDefaultContextMenu(&menu, iid, object);
        }
        else
        {
            ITEMIDLIST desktopId = {};
            result = SHCreateDataObject(&desktopId, (UINT)items.size(),
                                         (PCUITEMID_CHILD_ARRAY)items.data(), NULL, iid, object);
            if (SUCCEEDED(result))
            {
                // The shell may shorten long names to DOS aliases when rendering
                // CF_HDROP. Supply exact UTF-16 paths for consumers of that format.
                IDataObject* data = NULL;
                result = ((IUnknown*)*object)->QueryInterface(IID_PPV_ARGS(&data));
                if (SUCCEEDED(result))
                {
                    size_t characters = 1;
                    for (size_t i = 0; i < paths.size(); ++i) characters += paths[i].size() + 1;
                    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + characters * sizeof(wchar_t));
                    DROPFILES* drop = memory != NULL ? (DROPFILES*)GlobalLock(memory) : NULL;
                    if (drop == NULL)
                    {
                        if (memory != NULL) GlobalFree(memory);
                        result = E_OUTOFMEMORY;
                    }
                    else
                    {
                        drop->pFiles = sizeof(DROPFILES);
                        drop->fWide = TRUE;
                        wchar_t* destination = (wchar_t*)((BYTE*)drop + sizeof(DROPFILES));
                        for (size_t i = 0; i < paths.size(); ++i)
                        {
                            memcpy(destination, paths[i].c_str(), (paths[i].size() + 1) * sizeof(wchar_t));
                            destination += paths[i].size() + 1;
                        }
                        GlobalUnlock(memory);
                        FORMATETC format = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
                        STGMEDIUM medium = {};
                        medium.tymed = TYMED_HGLOBAL;
                        medium.hGlobal = memory;
                        result = data->SetData(&format, &medium, TRUE);
                        if (FAILED(result)) GlobalFree(memory);
                    }
                    data->Release();
                }
                if (FAILED(result))
                {
                    ((IUnknown*)*object)->Release();
                    *object = NULL;
                }
            }
        }
    }
    for (size_t i = 0; i < items.size(); ++i)
        CoTaskMemFree(items[i]);
    desktop->Release();
    return result;
}
