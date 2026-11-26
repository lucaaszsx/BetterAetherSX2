// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

namespace bax::jit::ee {

// Emits the decision for the branch or jump `code` at branch_pc into `decision`, with decideBranch's encoding (the
// target with BRANCH_TAKEN, BRANCH_SKIPS_SLOT, or 0), and writes any link register first, as the interpreter does.
// Returns false, emitting nothing, for what it leaves to decideBranch: BC0x and BC1x, and the jumps the Goemon TLB
// fix watches while that fix is on. `state` holds &cpuRegs; x0-x2 are scratch.
bool emitBranchDecision(vixl::aarch64::MacroAssembler& masm, const vixl::aarch64::Register& state,
                        const vixl::aarch64::Register& decision, u32 code, u32 branch_pc);

} // namespace bax::jit::ee
