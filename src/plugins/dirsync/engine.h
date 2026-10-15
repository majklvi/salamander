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
#include <memory>
#ifdef DIRSYNC_ENGINE_TEST
#include <functional>
#endif

namespace dirsync
{
enum class State { Equal, LeftOnly, RightOnly, Different, Conflict, Blocked };
enum class Action { None, CopyRight, CopyLeft, DeleteRight, Decide, Blocked };
enum class Reason { Metadata, Content, NewerLeft, NewerRight, Missing, TypeConflict,
                    ScanError, Unsupported, Changed, Ambiguous, Cancelled, InvalidRoot, InvalidMask, Excluded, Aliased };
struct Entry
{
    std::wstring path;
    bool directory = false, empty = false;
    uint64_t size = 0, time = 0;
    DWORD attributes = 0, error = 0;
    uint64_t volume = 0, id = 0, change = 0;
    DWORD links = 0;
    bool identified = false;
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
struct Snapshot;
struct Result
{
    std::vector<Row> rows;
    bool cancelled = false, stale = false;
    std::shared_ptr<const Snapshot> snapshot;
};
struct Progress { std::atomic<uint64_t> scanned{0}, compared{0}, readBytes{0}; };
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
// Read-only checkpoint, never permission to execute a later filesystem write.
// Any change/error/cancellation permanently blocks this result until Compare.
bool Revalidate(Result& result, std::atomic<bool>& cancel, Progress& progress);
// Internal plugin API; retrieves only the roots belonging to this snapshot.
bool SnapshotRoots(const Result& result, std::wstring& left, std::wstring& right);
bool SnapshotAsymmetric(const Result& result);
#ifdef DIRSYNC_ENGINE_TEST
void SetAfterScanHook(std::function<void()> hook);
void SetBeforeReadHook(std::function<void()> hook);
#endif
}
