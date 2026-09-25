// SPDX-License-Identifier: GPL-2.0-or-later
#include "../branchshell.h"
#include "../branchclipboard.h"
#include "../operationrecycle.h"
#include <cstdio>
#include <filesystem>
#include <set>

static int failures = 0;
static void Check(bool condition, const char* name)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) ++failures;
}
static bool MakeFile(const std::wstring& path)
{
    const std::wstring apiPath = path.size() >= MAX_PATH ? L"\\\\?\\" + path : path;
    HANDLE file = CreateFileW(apiPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    return true;
}
int wmain()
{
    HRESULT initialized = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return 2;
    std::vector<wchar_t> temp(32768);
    DWORD size = GetTempPathW((DWORD)temp.size(), temp.data());
    if (size == 0 || size >= temp.size()) return 2;
    const std::filesystem::path root = std::filesystem::path(temp.data()) /
        (L"samandarin-branch-operations-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code error;
    std::filesystem::create_directories(root / L"prvni-\u0161\u017e\u6f22", error);
    std::filesystem::create_directories(root / L"druhy-\u00e9\u03b1\U0001f600", error);
    std::filesystem::path longParent = root;
    for (int i = 0; i < 5; ++i) longParent /= std::wstring(60, L'\u6f22');
    std::filesystem::create_directories(std::filesystem::path(L"\\\\?\\" + longParent.wstring()), error);
    const std::vector<std::wstring> paths = {
        (root / L"prvni-\u0161\u017e\u6f22" / L"same-\u6f22.txt").wstring(),
        (root / L"druhy-\u00e9\u03b1\U0001f600" / L"same-\u6f22.txt").wstring(),
        (longParent / L"same-\u6f22.txt").wstring()};
    Check(MakeFile(paths[0]) && MakeFile(paths[1]) && MakeFile(paths[2]), "create duplicate Unicode basenames");
    IDataObject* object = NULL;
    HRESULT result = CreateShellObjectForPaths(NULL, paths, IID_IDataObject, (void**)&object, FALSE);
    Check(SUCCEEDED(result) && object != NULL, "multi-parent shell data object");
    if (object != NULL)
    {
        FORMATETC format = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium = {};
        result = object->GetData(&format, &medium);
        Check(SUCCEEDED(result), "CF_HDROP available");
        if (SUCCEEDED(result))
        {
            std::set<std::wstring> actual;
            HDROP drop = (HDROP)medium.hGlobal;
            UINT count = DragQueryFileW(drop, 0xffffffff, NULL, 0);
            for (UINT i = 0; i < count; ++i)
            {
                UINT length = DragQueryFileW(drop, i, NULL, 0);
                std::vector<wchar_t> name((size_t)length + 1);
                DragQueryFileW(drop, i, name.data(), (UINT)name.size());
                actual.insert(name.data());
            }
            Check(actual == std::set<std::wstring>(paths.begin(), paths.end()), "CF_HDROP preserves every exact Unicode and long path");
            ReleaseStgMedium(&medium);
        }
        object->Release();
    }
    IContextMenu2* menu = NULL;
    result = CreateShellObjectForPaths(NULL, paths, IID_IContextMenu2, (void**)&menu, TRUE);
    Check(SUCCEEDED(result) && menu != NULL, "multi-parent default context menu");
    if (menu != NULL)
    {
        HMENU popup = CreatePopupMenu();
        result = menu->QueryContextMenu(popup, 0, 1, 1000, CMF_NORMAL);
        Check(SUCCEEDED(result) && GetMenuItemCount(popup) > 0, "context menu populated without invoking actions");
        DestroyMenu(popup);
        menu->Release();
    }
    std::vector<std::wstring> broken = paths;
    broken.push_back((root / L"missing.txt").wstring());
    object = NULL;
    result = CreateShellObjectForPaths(NULL, broken, IID_IDataObject, (void**)&object, FALSE);
    Check(FAILED(result) && object == NULL, "invalid selection cannot partially succeed");
    std::vector<std::wstring> unsafe = paths;
    unsafe[0] += L" ";
    object = NULL;
    result = CreateShellObjectForPaths(NULL, unsafe, IID_IDataObject, (void**)&object, FALSE);
    Check(FAILED(result) && object == NULL, "shell normalization cannot alias a different file");
    std::wstring unc, relative;
    Check(BranchClipboardUNCPath(L"\\\\server\\share\\\u6f22\\same.txt", unc) &&
          unc == L"\\\\server\\share\\\u6f22\\same.txt", "clipboard preserves direct Unicode UNC path");
    Check(BranchClipboardPlainPath(L"\\\\?\\UNC\\server\\share\\file") == L"\\\\server\\share\\file", "clipboard removes only extended UNC prefix");
    Check(BranchShareRelativePath(L"C:\\data\\\u6f22\\same.txt", L"c:\\data", relative) &&
          relative == L"\u6f22\\same.txt", "local-share suffix preserves exact Unicode identity");
    Check(!BranchShareRelativePath(L"C:\\database\\same.txt", L"C:\\data", relative), "local share prefix respects path-component boundary");
    std::wstring shellPath;
    Check(SUCCEEDED(OperationRecyclePath(LR"(\\?\C:\short.txt)", shellPath)) && shellPath == LR"(C:\short.txt)",
          "recycle strips extended prefix even on short ASCII paths");
    Check(SUCCEEDED(OperationRecyclePath(LR"(\\?\UNC\server\share\file.txt)", shellPath)) && shellPath == LR"(\\server\share\file.txt)",
          "recycle preserves UNC root while stripping transport prefix");
    Check(FAILED(OperationRecyclePath(paths[0] + L".", shellPath)) &&
          FAILED(OperationRecyclePath(root.wstring() + LR"(\parent.\same.txt)", shellPath)),
          "recycle rejects both leaf and ancestor shell-normalization aliases");
    const std::wstring byteLongPath = (root / (std::wstring(100, L'\u6f22') + L".txt")).wstring();
    Check(byteLongPath.size() < MAX_PATH && MakeFile(byteLongPath), "create UTF8-over-260 fixture with short UTF16 path");
    const std::vector<std::wstring> recyclePaths = {
        (root / L"short.txt").wstring(), paths[0], paths[1], paths[2], byteLongPath};
    Check(MakeFile(recyclePaths[0]), "create short ordinary-panel recycle fixture");
    for (size_t i = 0; i < recyclePaths.size(); ++i)
    {
        const std::wstring exact = LR"(\\?\)" + recyclePaths[i];
        DWORD recycled = RecycleOperationFileW(NULL, exact);
        char label[100];
        sprintf_s(label, "actual selected-extension recycle preserves exact path %zu (%lu)", i, recycled);
        Check(recycled == ERROR_SUCCESS && GetFileAttributesW(exact.c_str()) == INVALID_FILE_ATTRIBUTES, label);
    }
    Check(RecycleOperationFileW(NULL, LR"(\\?\)" + root.wstring() + LR"(\missing.txt)") != ERROR_SUCCESS,
          "missing recycle item cannot silently succeed");
    const std::wstring lockedPath = (root / L"locked.txt").wstring();
    HANDLE locked = CreateFileW(lockedPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    Check(locked != INVALID_HANDLE_VALUE, "create locked recycle fixture");
    Check(RecycleOperationFileW(NULL, lockedPath) != ERROR_SUCCESS &&
          GetFileAttributesW(lockedPath.c_str()) != INVALID_FILE_ATTRIBUTES,
          "failed per-item recycle cannot silently report success");
    if (locked != INVALID_HANDLE_VALUE) CloseHandle(locked);
    // Own unique test root only; no user files are used or removed.
    std::filesystem::remove_all(std::filesystem::path(L"\\\\?\\" + root.wstring()), error);
    CoUninitialize();
    return failures == 0 ? 0 : 1;
}
