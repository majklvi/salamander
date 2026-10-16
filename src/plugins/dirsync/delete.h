// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "recovery.h"
namespace dirsync {
enum class DeleteStatus { NotStarted, Deleted, Cancelled, Failed };
struct DeleteOutcome {
    size_t row=0;
    std::wstring target;
    DeleteStatus status=DeleteStatus::NotStarted;
    DWORD error=0;
    bool removed=false;
};
struct DeleteReport {
    std::vector<DeleteOutcome> items;
    RecoveryReport recovery;
    std::wstring journal;
    DWORD error=0,journalError=0;
    size_t omitted=0;
    bool keepOriginals=true,stale=false,cancelled=false,complete=false;
};
bool IsFileDeletion(const Row& row);
bool IsDeletion(const Row& row);
// Shared by confirmation and execution. A directory requires all descendants
// to be visible, eligible deletion proposals; never implicitly selects children.
std::vector<size_t> DeletionRows(const Result& snapshot,const std::vector<size_t>& visible);
// Asymmetric right-only files followed by empty directories, deepest first.
// The UI separately confirms this set and the frozen recovery choice.
DeleteReport DeleteFiles(Result snapshot,const std::vector<size_t>& visible,bool keepOriginals,
    const std::wstring& recoveryRoot,std::atomic<bool>& cancel,Progress& checking,CopyProgress& progress);
}
