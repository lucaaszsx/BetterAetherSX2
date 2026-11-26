// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"

namespace bax::jit::ee {

// True for the EE instructions that transfer control with a delay slot: jumps, branches, and BC0x/BC1x.
bool isBranch(u32 code);

// Called by a block at its branch: cpuRegs.code holds the branch and cpuRegs.pc points past it. Decides the branch,
// runs the delay slot (unless a likely branch skips it) and leaves cpuRegs.pc at the next instruction to run.
// Returns the delay slot's cycles, or 0 when it was skipped.
u32 runBranch();

} // namespace bax::jit::ee
