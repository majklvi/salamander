// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "copy.h"
namespace dirsync {
struct RecoveryItem {
    size_t row=0;
    bool right=false, logged=false;
    std::wstring original, saved;
    CopyOutcome copy;
};
struct RecoveryReport {
    std::vector<RecoveryItem> items;
    std::wstring runDirectory, journal;
    DWORD error=0, journalError=0;
    size_t omitted=0;
    bool stale=false, cancelled=false, complete=false;
};
bool IsOriginalToSave(const Row& row);
#if defined(DIRSYNC_ENGINE_TEST) || defined(DIRSYNC_COPY_TEST_HOOKS)
// Test-only fault at a journal append. Zero permits the actual write.
void SetRecoveryLogHook(std::function<DWORD(const std::wstring&)> hook);
#endif
// Creates independent recovery copies only. Never replaces or deletes originals.
// Copied alone is not a completed D4 record: require logged and a complete report.
RecoveryReport SaveOriginals(Result snapshot,const std::vector<size_t>& visible,const std::wstring& recoveryRoot,
    std::atomic<bool>& cancel,Progress& checking,CopyProgress& progress);
}
