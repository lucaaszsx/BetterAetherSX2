// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"

#include <optional>

namespace bax::jit::ee {

// True for the EE instructions that transfer control with a delay slot: jumps, branches, and BC0x/BC1x.
bool isBranch(u32 code);

// Called by a block at its branch: cpuRegs.code holds the branch and cpuRegs.pc points past it. Decides the branch,
// runs the delay slot (unless a likely branch skips it) and leaves cpuRegs.pc at the next instruction to run.
// Returns the delay slot's cycles, or 0 when it was skipped.
u32 runBranch();

// decideBranch's flags, above the 32-bit target: the branch is taken, or a likely branch skips its delay slot.
inline constexpr u64 BRANCH_TAKEN = 1ull << 32;
inline constexpr u64 BRANCH_SKIPS_SLOT = 1ull << 33;
inline constexpr unsigned BRANCH_TAKEN_BIT = 32;
inline constexpr unsigned BRANCH_SKIPS_SLOT_BIT = 33;

// For blocks that run the delay slot themselves: decides the branch in cpuRegs.code, with cpuRegs.pc past it, and
// writes any link register, as runBranch does. Returns the target with BRANCH_TAKEN, BRANCH_SKIPS_SLOT, or 0 when a
// branch that is not taken still runs its delay slot.
u64 decideBranch();

// Where the branch or jump `code` at `pc` goes when taken, if that does not depend on a register (JR and JALR do).
std::optional<u32> staticTarget(u32 code, u32 pc);

} // namespace bax::jit::ee
