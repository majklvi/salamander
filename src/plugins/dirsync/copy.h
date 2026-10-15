// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "engine.h"

namespace dirsync {
enum class CopyPhase { Checking, Copying, Verifying, Publishing, Finished };
struct CopyProgress {
    std::atomic<CopyPhase> phase{CopyPhase::Checking};
    std::atomic<uint64_t> copied{0}, verified{0}, completed{0}, item{0};
};
enum class CopyStatus { NotStarted, Copied, Cancelled, Failed };
struct CopyOutcome {
    size_t row = 0;
    std::wstring target, remainingStage;
    CopyStatus status = CopyStatus::NotStarted;
    DWORD error = 0;
    bool published = false;
};
struct CopyReport {
    std::vector<CopyOutcome> items;
    bool stale = false, cancelled = false;
    size_t omitted = 0;
};
bool IsNewFileCopy(const Row& row);
CopyReport CopyNewFiles(Result snapshot, const std::vector<size_t>& visible,
                       std::atomic<bool>& cancel, Progress& checking, CopyProgress& progress);
#ifdef DIRSYNC_ENGINE_TEST
// Deterministic ordinary failure/cancellation boundaries; never in shipped SPL.
void SetCopyHook(std::function<void(CopyPhase, const std::wstring&)> hook);
#endif
}
