// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"

#include <cstddef>
#include <vector>

namespace bax::jit::iop {

// The longest IOP block, in instructions before its branch, and the room one takes at most in the code buffer.
inline constexpr u32 MAX_BLOCK_INSTRUCTIONS = 128;
inline constexpr std::size_t MAX_BLOCK_BYTES = 16 * 1024;
// One byte per 4 KB page of IOP RAM, nonzero when the page holds compiled code; provided by the runtime. It stays at
// one address until the runtime discards every block.
const u8* codePages();

// The interpreter's IRX-injection hack runs before the instruction at this address, so no block runs across it.
inline constexpr u32 IRX_INJECT_PC = 0x1630;

// Where a block goes when it ends, as executable addresses in the runtime's entry code: the dispatcher, which jumps
// to the block at psxRegs.pc; the event test, once psxRegs.cycle reaches iopNextEventCycle; or back to the caller of
// ExecuteBlock, once the EE cycles it was given are spent.
struct BlockExits {
    const void* dispatch;
    const void* event_test;
    const void* leave;
};

// A direct B at the end of a block, which should jump to the block at target_pc once that block exists.
struct LinkSite {
    u32 target_pc;
    const u8* site; // Executable address of the B.
};

struct CompiledBlock {
    const u8* entry; // Executable address the dispatcher jumps to.
    std::size_t bytes;
    u32 instructions; // IOP instructions the block covers, a branch's delay slot included.
    std::vector<LinkSite> links;
};

// Compiles the IOP block at start_pc into `writable`, which runs at `executable`. Instructions run through the
// interpreter's own functions; branches are decided in native code, as the interpreter's branch functions and doBranch
// decide them. A block ends at a branch (with its delay slot), SYSCALL or BREAK, a 4 KB page boundary,
// MAX_BLOCK_INSTRUCTIONS, or before IRX_INJECT_PC. Blocks have no frame: they run with &psxRegs in x19, set by the
// runtime's entry code, and end by jumping to `exits`. Targets for which isHookAddress is true never get a link site.
CompiledBlock compileBlock(u32 start_pc, const BlockExits& exits, u8* writable, const u8* executable);

// Whether the interpreter runs a hook when it reaches `pc`: the IRX injection, or a PS1 BIOS call. Blocks at these
// addresses always go through the runtime's dispatcher, which runs the hook.
bool isHookAddress(u32 pc);

// One byte per 4 KB page of IOP RAM, nonzero when the page holds compiled code; provided by the runtime. It stays at
// one address until the runtime discards every block.
const u8* codePages();

// The interpreter's IRX-injection hack (execI), run before the instruction at IRX_INJECT_PC.
void injectIrx();

} // namespace bax::jit::iop
