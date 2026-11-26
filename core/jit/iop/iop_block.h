// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"

#include <cstddef>

namespace bax::jit::iop {

// The longest IOP block, and the room one takes at most in the code buffer.
inline constexpr u32 MAX_BLOCK_INSTRUCTIONS = 128;
inline constexpr std::size_t MAX_BLOCK_BYTES = 8 * 1024;
// The interpreter's IRX-injection hack runs before the instruction at this address, so no block runs across it.
inline constexpr u32 IRX_INJECT_PC = 0x1630;

struct CompiledBlock {
    const u8* entry; // Executable address: call it as void().
    std::size_t bytes;
    u32 instructions;
};

// Compiles the IOP block at start_pc into `writable`, which runs at `executable`. Each instruction runs through the
// interpreter's own function, as its execI would run it; a branch or jump ends the block, because the interpreter's
// branch functions already run the delay slot and the IOP event test. Blocks also end at a 4 KB page boundary, after
// MAX_BLOCK_INSTRUCTIONS, and before IRX_INJECT_PC.
CompiledBlock compileBlock(u32 start_pc, u8* writable, const u8* executable);

} // namespace bax::jit::iop
