// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../copy.h"
namespace dirsync_test {
enum class SpaceFault { None, Reserve, MainData, NamedStream, RetainStage, Publish };
// Only the copy test executable links this WinAPI fault injector.
void SetSpaceFault(SpaceFault fault,DWORD error,unsigned ordinal);
bool SpaceFaultReached();
void ReleaseSpaceFault();
}
