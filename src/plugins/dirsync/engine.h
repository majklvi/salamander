// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace dirsync
{
enum class State { Equal, LeftOnly, RightOnly, Different, Conflict, Blocked };
enum class Action { None, CopyRight, CopyLeft, DeleteRight, Decide, Blocked };
enum class Reason { Metadata, Content, NewerLeft, NewerRight, Missing, TypeConflict,
                    ScanError, Unsupported, Changed, Ambiguous, Cancelled, InvalidRoot, InvalidMask, Excluded };
struct Entry
{
    std::wstring path;
    bool directory = false, empty = false;
    uint64_t size = 0, time = 0;
    DWORD attributes = 0, error = 0;
    Reason reason = Reason::Metadata;
};
struct Selection { std::wstring name; bool directory = false; };
struct Options
{
    bool recursive = true, content = false, ignoreDate = false, asymmetric = false;
    bool emptyDirectories = false, onlySelected = false;
    std::wstring mask = L"*.*";
    std::vector<Selection> selection;
    uint64_t tolerance = 30000000; // FILETIME ticks; approved inclusive 3 seconds.
};
struct Row
{
    std::wstring path;
    bool hasLeft = false, hasRight = false;
    Entry left, right;
    State state = State::Equal;
    Action action = Action::None;
    Reason reason = Reason::Metadata;
    DWORD error = 0;
};
struct Result { std::vector<Row> rows; bool cancelled = false; };
struct Progress { std::atomic<uint64_t> scanned{0}, compared{0}; };
bool DecodePath(const char* input, std::wstring& output);
bool NormalizeRoot(const std::wstring& input, std::wstring& output);
std::wstring ApiPath(const std::wstring& path);
bool Matches(const std::wstring& pattern, const std::wstring& relative);
bool ValidateMask(const std::wstring& mask);
bool Included(const std::wstring& mask, const std::wstring& relative);
bool ExcludedDirectory(const std::wstring& mask, const std::wstring& relative);
Row Classify(const Entry* left, const Entry* right, const Options& options,
             bool contentEqual = true);
Result Compare(const std::wstring& left, const std::wstring& right, const Options& options,
               std::atomic<bool>& cancel, Progress& progress);
}
