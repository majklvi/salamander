// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "precomp.h"
#include "engine.h"
#include <cwchar>
#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace dirsync
{
namespace
{
bool Same(const std::wstring& a, const std::wstring& b)
{ return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL; }
struct Less
{
    bool operator()(const std::wstring& a, const std::wstring& b) const
    { return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN; }
};
bool Within(const std::wstring& path, const std::wstring& parent)
{
    return Same(path, parent) || (path.size() > parent.size() && path[parent.size()] == L'\\' &&
        Same(path.substr(0, parent.size()), parent));
}
uint64_t Ticks(FILETIME time)
{ return (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime; }
std::wstring Join(const std::wstring& root, const std::wstring& relative)
{ return root + (root.back() == L'\\' ? L"" : L"\\") + relative; }
struct Handle
{
    HANDLE value;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct FindHandle
{
    HANDLE value;
    ~FindHandle() { if (value != INVALID_HANDLE_VALUE) FindClose(value); }
};
bool Unsupported(DWORD attr)
{
    // Includes cloud reparse placeholders. Never hydrate/open their content.
    return (attr & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_ENCRYPTED |
                    FILE_ATTRIBUTE_OFFLINE | 0x00040000 | 0x00400000)) != 0;
}
struct Masks { std::vector<std::wstring> include, exclude; bool valid = true; };
Masks Parse(const std::wstring& mask)
{
    Masks result;
    bool exclude = false, quote = false;
    std::wstring token;
    auto add = [&]() { if (!token.empty()) { (exclude ? result.exclude : result.include).push_back(token); token.clear(); } };
    for (wchar_t c : mask)
    {
        if (c == L'"') { quote = !quote; continue; }
        if (!quote && c == L'|') { add(); if (exclude) result.valid = false; exclude = true; }
        else if (!quote && (c == L' ' || c == L'\t' || c == L';')) add();
        else token += c;
    }
    add();
    if (quote) result.valid = false;
    for (auto* list : {&result.include, &result.exclude})
        for (const auto& p : *list)
            if (p.find_first_of(L"<>:/") != std::wstring::npos || p.find(L"..") != std::wstring::npos)
                result.valid = false;
    if (result.include.empty()) result.include.push_back(L"*");
    return result;
}
bool Glob(const std::wstring& pattern, const std::wstring& path)
{
    // Bounded dynamic programming; '*' never crosses a separator, '**' does.
    std::vector<unsigned char> previous(path.size() + 1), current(path.size() + 1);
    previous[0] = 1;
    for (size_t i = 0; i < pattern.size(); ++i)
    {
        std::fill(current.begin(), current.end(), static_cast<unsigned char>(0));
        wchar_t c = pattern[i];
        bool deep = c == L'*' && i + 1 < pattern.size() && pattern[i + 1] == L'*';
        if (deep) ++i;
        if (c == L'*')
        {
            current[0] = previous[0];
            for (size_t j = 1; j <= path.size(); ++j)
                current[j] = previous[j] || (current[j - 1] && (deep || path[j - 1] != L'\\'));
        }
        else
            for (size_t j = 1; j <= path.size(); ++j)
                current[j] = previous[j - 1] &&
                    ((c == L'?' && path[j - 1] != L'\\') ||
                     CompareStringOrdinal(&c, 1, &path[j - 1], 1, TRUE) == CSTR_EQUAL);
        previous.swap(current);
        // '**\\' also matches zero directory levels.
        if (deep && i + 1 < pattern.size() && pattern[i + 1] == L'\\')
        {
            std::vector<unsigned char> separator(path.size() + 1);
            for (size_t j = 1; j <= path.size(); ++j)
                separator[j] = previous[j - 1] && path[j - 1] == L'\\';
            // Zero-level states are the states before processing '**'.
            for (size_t j = 0; j <= path.size(); ++j) separator[j] |= current[j];
            previous.swap(separator);
            ++i;
        }
    }
    return previous[path.size()] != 0;
}
bool Pattern(const std::wstring& input, const std::wstring& relative)
{
    std::wstring pattern = input == L"*.*" ? L"*" : input;
    bool anchored = !pattern.empty() && pattern.front() == L'\\';
    if (anchored) pattern.erase(0, 1);
    if (Glob(pattern, relative)) return true;
    if (!anchored)
        for (size_t pos = relative.find(L'\\'); pos != std::wstring::npos; pos = relative.find(L'\\', pos + 1))
            if (Glob(pattern, relative.substr(pos + 1))) return true;
    return false;
}
bool Excluded(const Masks& masks, const std::wstring& path, bool directory)
{
    for (const auto& p : masks.exclude)
        if (!p.empty() && p.back() != L'\\' && Pattern(p, path)) return true;
    std::wstring ancestor = directory ? path : path.substr(0, path.find_last_of(L'\\'));
    if (!directory && path.find(L'\\') == std::wstring::npos) ancestor.clear();
    while (!ancestor.empty())
    {
        for (const auto& p : masks.exclude)
            if (!p.empty() && p.back() == L'\\' && Pattern(p.substr(0, p.size() - 1), ancestor)) return true;
        size_t split = ancestor.find_last_of(L'\\');
        if (split == std::wstring::npos) break;
        ancestor.resize(split);
    }
    return false;
}
struct Tree { std::map<std::wstring, Entry, Less> entries; std::set<std::wstring, Less> blocked, excluded; };
void Add(Tree& tree, Entry entry)
{
    auto inserted = tree.entries.emplace(entry.path, entry);
    if (!inserted.second)
    {
        inserted.first->second.error = ERROR_DUP_NAME;
        inserted.first->second.reason = Reason::Ambiguous;
        tree.blocked.insert(entry.path);
    }
}
bool Scope(const std::wstring& path, const Options& options)
{
    if (!options.onlySelected) return true;
    for (const auto& s : options.selection)
        if (Same(path, s.name) || (s.directory && Within(path, s.name))) return true;
    return false;
}
bool Descend(const std::wstring& path, const Options& options)
{
    if (!options.onlySelected) return options.recursive;
    for (const auto& s : options.selection)
        if (s.directory && (Same(path, s.name) || (options.recursive && Within(path, s.name)))) return true;
    return false;
}
void Scan(const std::wstring& root, const Options& options, const Masks& masks,
          std::atomic<bool>& cancel, Progress& progress, Tree& tree)
{
    std::vector<std::wstring> pending{L""};
    while (!pending.empty() && !cancel)
    {
        std::wstring relative = std::move(pending.back()); pending.pop_back();
        std::wstring full = relative.empty() ? root : Join(root, relative);
        DWORD attr = GetFileAttributesW(ApiPath(full).c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY) || Unsupported(attr))
        {
            Entry e; e.path = relative; e.directory = true;
            e.error = attr == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_NOT_SUPPORTED;
            e.reason = attr == INVALID_FILE_ATTRIBUTES ? Reason::ScanError : Reason::Unsupported;
            Add(tree, e); tree.blocked.insert(relative); continue;
        }
        WIN32_FIND_DATAW data{};
        FindHandle find{FindFirstFileW(ApiPath(Join(full, L"*")).c_str(), &data)};
        if (find.value == INVALID_HANDLE_VALUE)
        {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND) { if (!relative.empty()) tree.entries[relative].empty = true; continue; }
            Entry e; e.path = relative; e.directory = true; e.error = error; e.reason = Reason::ScanError;
            tree.entries[relative] = e; tree.blocked.insert(relative); continue;
        }
        bool any = false;
        do
        {
            if (cancel) break;
            if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
            any = true; ++progress.scanned;
            Entry e;
            e.path = relative.empty() ? data.cFileName : relative + L"\\" + data.cFileName;
            e.directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            e.attributes = data.dwFileAttributes;
            e.size = (uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            e.time = Ticks(data.ftLastWriteTime);
            if (!Scope(e.path, options)) continue;
            if (Excluded(masks, e.path, e.directory)) { tree.excluded.insert(e.path); continue; }
            if (Unsupported(e.attributes))
            { e.error = ERROR_NOT_SUPPORTED; e.reason = Reason::Unsupported; tree.blocked.insert(e.path); }
            bool include = e.directory;
            if (!include) for (const auto& p : masks.include) if (Pattern(p, e.path)) { include = true; break; }
            if (!include) { tree.excluded.insert(e.path); continue; }
            if (e.directory && !e.error && options.emptyDirectories && !Descend(e.path, options))
            {
                WIN32_FIND_DATAW child{};
                FindHandle peek{FindFirstFileW(ApiPath(Join(Join(root, e.path), L"*")).c_str(), &child)};
                if (peek.value == INVALID_HANDLE_VALUE)
                {
                    DWORD error = GetLastError();
                    if (error == ERROR_FILE_NOT_FOUND) e.empty = true;
                    else { e.error = error; e.reason = Reason::ScanError; tree.blocked.insert(e.path); }
                }
                else
                {
                    bool found = false;
                    do { if (wcscmp(child.cFileName, L".") && wcscmp(child.cFileName, L"..")) { found = true; break; } }
                    while (FindNextFileW(peek.value, &child));
                    e.empty = !found;
                    if (!found && GetLastError() != ERROR_NO_MORE_FILES)
                    { e.error = GetLastError(); e.reason = Reason::ScanError; tree.blocked.insert(e.path); }
                }
            }
            Add(tree, e);
            if (e.directory && !e.error && Descend(e.path, options)) pending.push_back(e.path);
        } while (FindNextFileW(find.value, &data));
        DWORD error = GetLastError();
        if (!cancel && error != ERROR_NO_MORE_FILES)
        {
            Entry e; e.path = relative; e.directory = true; e.error = error; e.reason = Reason::ScanError;
            tree.entries[relative] = e; tree.blocked.insert(relative);
        }
        else if (!relative.empty() && !any) tree.entries[relative].empty = true;
    }
}
bool Under(const std::set<std::wstring, Less>& prefixes, std::wstring path)
{
    while (true)
    {
        if (prefixes.find(path) != prefixes.end()) return true;
        if (path.empty()) return false;
        size_t parent = path.find_last_of(L'\\');
        path.resize(parent == std::wstring::npos ? 0 : parent);
    }
}
bool UnderBlocked(const Tree& tree, const std::wstring& path) { return Under(tree.blocked, path); }
bool UnderExcluded(const Tree& tree, const std::wstring& path)
{ return Under(tree.excluded, path); }
bool Stable(const BY_HANDLE_FILE_INFORMATION& info, const Entry& expected)
{
    return !Unsupported(info.dwFileAttributes) && !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        Ticks(info.ftLastWriteTime) == expected.time &&
        ((uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow) == expected.size;
}
bool Content(const std::wstring& left, const std::wstring& right, const Row& row,
             std::atomic<bool>& cancel, bool& equal, DWORD& error)
{
    Handle a(CreateFileW(ApiPath(left).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (a.value == INVALID_HANDLE_VALUE) { error = GetLastError(); return false; }
    Handle b(CreateFileW(ApiPath(right).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (b.value == INVALID_HANDLE_VALUE) { error = GetLastError(); return false; }
    BY_HANDLE_FILE_INFORMATION beforeA{}, beforeB{}, afterA{}, afterB{};
    if (!GetFileInformationByHandle(a.value, &beforeA) || !GetFileInformationByHandle(b.value, &beforeB))
    { error = GetLastError(); return false; }
    if (!Stable(beforeA, row.left) || !Stable(beforeB, row.right)) { error = ERROR_FILE_INVALID; return false; }
    std::vector<unsigned char> ab(65536), bb(65536);
    equal = true;
    while (!cancel)
    {
        DWORD na = 0, nb = 0;
        if (!ReadFile(a.value, ab.data(), DWORD(ab.size()), &na, nullptr) ||
            !ReadFile(b.value, bb.data(), DWORD(bb.size()), &nb, nullptr)) { error = GetLastError(); return false; }
        if (na != nb || !std::equal(ab.begin(), ab.begin() + na, bb.begin())) { equal = false; break; }
        if (!na) break;
    }
    if (cancel) { error = ERROR_CANCELLED; return false; }
    if (!GetFileInformationByHandle(a.value, &afterA) || !GetFileInformationByHandle(b.value, &afterB))
    { error = GetLastError(); return false; }
    if (!Stable(afterA, row.left) || !Stable(afterB, row.right)) { error = ERROR_FILE_INVALID; return false; }
    return true;
}
bool RootParentsSafe(const std::wstring& root)
{
    std::wstring p = root;
    while (!p.empty())
    {
        DWORD attr = GetFileAttributesW(ApiPath(p).c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY) || Unsupported(attr)) return false;
        size_t pos = p.find_last_of(L'\\');
        if (pos == std::wstring::npos || pos <= 2) break;
        if (p.rfind(L"\\\\", 0) == 0 && p.find(L'\\', 2) == pos) break; // UNC share.
        p.resize(pos);
    }
    return true;
}
}
bool DecodePath(const char* input, std::wstring& output)
{
    output.clear();
    if (!input || !*input) return false;
    UINT cp = CP_UTF8; DWORD flags = MB_ERR_INVALID_CHARS;
    int size = MultiByteToWideChar(cp, flags, input, -1, nullptr, 0);
    if (!size) { cp = CP_ACP; flags = MB_ERR_INVALID_CHARS; size = MultiByteToWideChar(cp, flags, input, -1, nullptr, 0); }
    if (size <= 1) return false;
    std::vector<wchar_t> buffer(size);
    if (!MultiByteToWideChar(cp, flags, input, -1, buffer.data(), size)) return false;
    output.assign(buffer.data());
    return output.find(wchar_t(0xfffd)) == std::wstring::npos;
}
std::wstring ApiPath(const std::wstring& path)
{
    if (path.rfind(L"\\\\?\\", 0) == 0) return path;
    if (path.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + path.substr(2);
    return L"\\\\?\\" + path;
}
bool NormalizeRoot(const std::wstring& input, std::wstring& output)
{
    std::wstring p = input;
    if (p.rfind(L"\\\\.\\", 0) == 0) return false;
    if (p.rfind(L"\\\\?\\UNC\\", 0) == 0) p = L"\\\\" + p.substr(8);
    else if (p.rfind(L"\\\\?\\", 0) == 0) p.erase(0, 4);
    bool disk = p.size() >= 3 && ((p[0] >= L'A' && p[0] <= L'Z') || (p[0] >= L'a' && p[0] <= L'z')) && p[1] == L':' && p[2] == L'\\';
    bool unc = p.rfind(L"\\\\", 0) == 0 && p.find(L'\\', 2) != std::wstring::npos;
    if ((!disk && !unc) || p.size() >= 32760 || p.find_first_of(L"*?\"<>|") != std::wstring::npos) return false;
    DWORD size = GetFullPathNameW(p.c_str(), 0, nullptr, nullptr);
    if (!size || size >= 32767) return false;
    std::vector<wchar_t> buffer(size);
    DWORD used = GetFullPathNameW(p.c_str(), size, buffer.data(), nullptr);
    if (!used || used >= size) return false;
    output.assign(buffer.data());
    while (output.size() > 3 && output.back() == L'\\') output.pop_back();
    return true;
}
bool Matches(const std::wstring& pattern, const std::wstring& relative) { return Pattern(pattern, relative); }
bool ValidateMask(const std::wstring& mask) { return Parse(mask).valid; }
bool Included(const std::wstring& mask, const std::wstring& relative)
{
    Masks m = Parse(mask);
    if (!m.valid || Excluded(m, relative, false)) return false;
    for (const auto& p : m.include) if (Pattern(p, relative)) return true;
    return false;
}
bool ExcludedDirectory(const std::wstring& mask, const std::wstring& relative)
{ return Excluded(Parse(mask), relative, true); }
Row Classify(const Entry* left, const Entry* right, const Options& o, bool contentEqual)
{
    Row row;
    if (left) { row.left = *left; row.hasLeft = true; row.path = left->path; }
    if (right) { row.right = *right; row.hasRight = true; row.path = right->path; }
    const Entry* error = left && left->error ? left : right && right->error ? right : nullptr;
    if (error) { row.state = State::Blocked; row.action = Action::Blocked; row.reason = error->reason; row.error = error->error; return row; }
    if (!left || !right)
    {
        row.state = left ? State::LeftOnly : State::RightOnly; row.reason = Reason::Missing;
        row.action = left ? Action::CopyRight : o.asymmetric ? Action::DeleteRight : Action::CopyLeft;
        return row;
    }
    if (left->directory != right->directory)
    { row.state = State::Conflict; row.action = Action::Blocked; row.reason = Reason::TypeConflict; return row; }
    if (left->directory) return row;
    uint64_t delta = left->time > right->time ? left->time - right->time : right->time - left->time;
    if (!o.ignoreDate && delta > o.tolerance)
    {
        row.state = State::Different;
        bool newerLeft = left->time > right->time;
        row.reason = newerLeft ? Reason::NewerLeft : Reason::NewerRight;
        row.action = newerLeft ? Action::CopyRight : o.asymmetric ? Action::None : Action::CopyLeft;
    }
    else if (left->size != right->size || (o.content && !contentEqual))
    { row.state = State::Different; row.action = Action::Decide; row.reason = o.content ? Reason::Content : Reason::Metadata; }
    return row;
}
Result Compare(const std::wstring& inputLeft, const std::wstring& inputRight, const Options& o,
               std::atomic<bool>& cancel, Progress& progress)
{
    Result result;
    std::wstring left, right;
    Masks masks = Parse(o.mask);
    if (!masks.valid || !NormalizeRoot(inputLeft, left) || !NormalizeRoot(inputRight, right) ||
        !RootParentsSafe(left) || !RootParentsSafe(right) || Within(left, right) || Within(right, left))
    {
        Row row; row.state = State::Blocked; row.action = Action::Blocked;
        row.reason = masks.valid ? Reason::InvalidRoot : Reason::InvalidMask;
        row.error = ERROR_INVALID_PARAMETER; result.rows.push_back(row); return result;
    }
    Tree a, b;
    Scan(left, o, masks, cancel, progress, a);
    Scan(right, o, masks, cancel, progress, b);
    std::map<std::wstring, bool, Less> paths;
    for (const auto& e : a.entries) paths[e.first] = true;
    for (const auto& e : b.entries) paths[e.first] = true;
    std::vector<std::wstring> conflicts;
    for (const auto& e : a.entries)
    {
        auto other = b.entries.find(e.first);
        if (other != b.entries.end() && e.second.directory != other->second.directory)
            conflicts.push_back(e.first);
    }
    for (const auto& p : paths)
    {
        if (cancel) break;
        auto ia = a.entries.find(p.first), ib = b.entries.find(p.first);
        const Entry* ea = ia == a.entries.end() ? nullptr : &ia->second;
        const Entry* eb = ib == b.entries.end() ? nullptr : &ib->second;
        Row row = Classify(ea, eb, o);
        for (const auto& conflict : conflicts)
            if (!Same(p.first, conflict) && Within(p.first, conflict))
            { row.state = State::Blocked; row.action = Action::Blocked; row.reason = Reason::TypeConflict; }
        if (UnderBlocked(a, p.first) || UnderBlocked(b, p.first))
        { row.state = State::Blocked; row.action = Action::Blocked; if (!row.error) { row.reason = Reason::ScanError; row.error = ERROR_NOT_SUPPORTED; } }
        if (row.state != State::Blocked && (UnderExcluded(a, p.first) || UnderExcluded(b, p.first)))
        { row.state = State::Blocked; row.action = Action::Blocked; row.reason = Reason::Excluded; }
        bool directory = (ea && ea->directory) || (eb && eb->directory);
        if (directory && row.state != State::Blocked && row.state != State::Conflict &&
            (!o.emptyDirectories || (ea && !ea->empty) || (eb && !eb->empty))) continue;
        if (!directory && row.state != State::Blocked && ea && eb && o.content &&
            ea->size == eb->size && (o.ignoreDate || row.state == State::Equal))
        {
            bool equal = true; DWORD error = 0;
            if (!Content(Join(left, ea->path), Join(right, eb->path), row, cancel, equal, error))
            { row.state = State::Blocked; row.action = Action::Blocked; row.error = error; row.reason = error == ERROR_FILE_INVALID ? Reason::Changed : Reason::ScanError; }
            else { row = Classify(ea, eb, o, equal); row.reason = Reason::Content; }
        }
        ++progress.compared;
        result.rows.push_back(std::move(row));
    }
    result.cancelled = cancel.load();
    // Partial scans must never present speculative copy/delete proposals.
    if (result.cancelled) for (auto& row : result.rows)
    { row.action = Action::Blocked; row.reason = Reason::Cancelled; row.state = State::Blocked; }
    return result;
}
}
