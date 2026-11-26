// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"

#include <cstddef>

namespace bax::jit::ee {

// Where a compiled block landed in the code buffer.
struct CompiledBlock {
    const u8* entry; // Executable address: call it as void().
    std::size_t bytes;
    u32 instructions; // EE instructions the block covers, a branch's delay slot included.
};

// The longest block, in instructions before its branch; with the epilogue it fits well inside MAX_BLOCK_BYTES.
inline constexpr u32 MAX_BLOCK_INSTRUCTIONS = 128;
inline constexpr std::size_t MAX_BLOCK_BYTES = 16 * 1024;

// Compiles the EE block that starts at start_pc. Code is written at `writable` and runs at `executable`, the other
// view of the same memory. The block ends at a branch (after its delay slot), at a 4 KB page boundary, after
// MAX_BLOCK_INSTRUCTIONS, or right before stop_pc, the differential tester's end address.
//
// This first tier calls the interpreter for every instruction, so it covers every opcode; native code for the common
// classes replaces those calls class by class.
CompiledBlock compileBlock(u32 start_pc, u32 stop_pc, u8* writable, const u8* executable);

// Adds a block's raw cycles to cpuRegs.cycle with the interpreter's scaling (intUpdateCPUCycles), keeping the same
// fractional remainder between blocks.
void commitCycles(u32 raw_cycles);

} // namespace bax::jit::ee
