// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "recovery.h"
namespace dirsync {
struct OverwriteReport {
    std::vector<CopyOutcome> items;
    RecoveryReport recovery;
    std::wstring journal;
    DWORD error=0,journalError=0;
    size_t omitted=0;
    bool keepOriginals=true,stale=false,cancelled=false,complete=false;
};
bool IsFileOverwrite(const Row& row);
// Replaces existing regular-file names only, never deletes user items as an action.
// keepOriginals=false requires an explicit UI confirmation without recovery.
OverwriteReport OverwriteFiles(Result snapshot,const std::vector<size_t>& visible,bool keepOriginals,
    const std::wstring& recoveryRoot,std::atomic<bool>& cancel,Progress& checking,CopyProgress& progress);
}
