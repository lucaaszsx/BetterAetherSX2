// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"

namespace bax::jit::ee {

// For the differential tester: runs recompiled blocks from cpuRegs.pc until the PC reaches stop_pc, running at most
// max_blocks blocks. Blocks never run past stop_pc. No event tests run, as when the interpreter is stepped. Returns
// false when the PC did not get there, or when recCpu.Reserve could not get code memory.
bool runUntil(u32 stop_pc, u32 max_blocks);

} // namespace bax::jit::ee
