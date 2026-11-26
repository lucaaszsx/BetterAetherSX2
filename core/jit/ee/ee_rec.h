// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "bax/jit/code_buffer.h"
#include "common/Pcsx2Types.h"

namespace bax::jit::ee {

// For the differential tester: runs recompiled blocks from cpuRegs.pc until the PC reaches stop_pc. Blocks never run
// past stop_pc, and a cycle budget stops a program that would not get there. Returns false when the PC did not get
// there, or when recCpu.Reserve could not get code memory.
bool runUntil(u32 stop_pc);

// The EE recompiler's code memory, or null when recCpu.Reserve could not get it.
const CodeBuffer* codeBuffer();

} // namespace bax::jit::ee
