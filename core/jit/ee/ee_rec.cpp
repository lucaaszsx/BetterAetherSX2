// SPDX-License-Identifier: GPL-3.0-or-later
// Runtime of the AArch64 EE recompiler: its code memory, the block table in upstream's recLUT layout (so vtlb and
// Clear keep working), a dispatch loop that calls blocks as functions, and the R5900cpu entry points VMManager
// selects. Direct branches between blocks replace the loop later.

#include "Host.h"
#include "Memory.h"
#include "R3000A.h"
#include "R5900.h"
#include "VMManager.h"
#include "bax/jit/a64_branch.h"
#include "bax/jit/code_buffer.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "ee/ee_block.h"
#include "x86/BaseblockEx.h"

#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace {

using bax::jit::CodeBuffer;

constexpr std::size_t CODE_BUFFER_BYTES = 64 * _1mb;
constexpr std::size_t CODE_ALIGNMENT = 16;
// The farthest back a block can start and still cover an address: its instructions plus a delay slot.
constexpr u32 MAX_BLOCK_SPAN = (bax::jit::ee::MAX_BLOCK_INSTRUCTIONS + 1) * 4;
// No stop address: it is unaligned, so the EE PC never equals it.
constexpr u32 NO_STOP_PC = 0xffffffff;
// 64 KB pages in the 32-bit EE address space, and block entries per page (one per 4-byte instruction).
constexpr u32 LUT_PAGES = 0x10000;
constexpr u32 ENTRIES_PER_PAGE = 0x10000 / 4;

std::optional<CodeBuffer> s_code;
std::size_t s_code_used = 0;

alignas(16) uptr s_lut[LUT_PAGES];
std::vector<BASEBLOCK> s_lut_memory; // RAM, then the three ROMs; a null entry means not compiled yet.
std::vector<BASEBLOCK> s_lut_unmapped;
BaseBlocks s_blocks;

u32 s_stop_pc = NO_STOP_PC;
bool s_executing = false;
bool s_exit_requested = false;
bool s_needs_reset = false;

BASEBLOCK* entryFor(u32 pc) {
    return PC_GETBLOCK_(pc, s_lut);
}

// Every page points into s_lut_unmapped, then RAM and ROM pages into s_lut_memory, at the same mirrors as upstream's
// x86 recompiler maps them.
void mapLut() {
    s_lut_memory.assign((Ps2MemSize::ExposedRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4, {});
    s_lut_unmapped.assign(ENTRIES_PER_PAGE, {});
    BASEBLOCK* ram = s_lut_memory.data();
    BASEBLOCK* rom = ram + Ps2MemSize::ExposedRam / 4;
    BASEBLOCK* rom1 = rom + Ps2MemSize::Rom / 4;
    BASEBLOCK* rom2 = rom1 + Ps2MemSize::Rom1 / 4;
    for (uint page = 0; page < LUT_PAGES; ++page) recLUT_SetPage(s_lut, nullptr, s_lut_unmapped.data(), page, 0, 0);
    for (uint page = 0; page < Ps2MemSize::ExposedRam / 0x10000; ++page)
        for (uint base : {0x0000u, 0x2000u, 0x3000u, 0x8000u, 0xa000u, 0xb000u, 0xc000u, 0xd000u})
            recLUT_SetPage(s_lut, nullptr, ram, base, page, page);
    const auto mapRom = [](BASEBLOCK* map, uint first_page, uint end_page) {
        for (uint page = first_page; page < end_page; ++page)
            for (uint base : {0x0000u, 0x8000u, 0xa000u})
                recLUT_SetPage(s_lut, nullptr, map, base, page, page - first_page);
    };
    mapRom(rom, 0x1fc0, 0x2000);
    mapRom(rom1, 0x1e00, 0x1e40);
    mapRom(rom2, 0x1e40, 0x1e80);
}

void resetBlocks() {
    mapLut();
    s_blocks.Reset();
    s_code_used = 0;
}

const u8* compile(u32 pc) {
    if (s_code_used + bax::jit::ee::MAX_BLOCK_BYTES > s_code->size()) resetBlocks();
    const bax::jit::ee::CompiledBlock block =
        bax::jit::ee::compileBlock(pc, s_stop_pc, s_code->writable() + s_code_used, s_code->executable() + s_code_used);
    s_code->flush(s_code_used, block.bytes);
    s_code_used = (s_code_used + block.bytes + CODE_ALIGNMENT - 1) / CODE_ALIGNMENT * CODE_ALIGNMENT;
    BASEBLOCKEX* info = s_blocks.New(pc, reinterpret_cast<uptr>(block.entry));
    info->size = block.instructions;
    info->x86size = static_cast<u32>(block.bytes);
    entryFor(pc)->SetFnptr(reinterpret_cast<uptr>(block.entry));
    return block.entry;
}

void safeExitExecution() {
    // Mid-event-test the state is not consistent, so the exit waits until the test returns; otherwise the next
    // block end runs an event test at once, and the loop leaves after it.
    s_exit_requested = true;
    if (!eeEventTestIsActive) {
        cpuRegs.nextEventCycle = 0;
    } else if (psxRegs.iopCycleEE > 0) {
        psxRegs.iopBreak += psxRegs.iopCycleEE;
        psxRegs.iopCycleEE = 0;
    }
}

// Runs the block at cpuRegs.pc, compiling it first if needed. False when the PC cannot hold code.
bool runNextBlock() {
    const u32 pc = cpuRegs.pc;
    BASEBLOCK* entry = entryFor(pc);
    const bool unmapped = entry >= s_lut_unmapped.data() && entry < s_lut_unmapped.data() + s_lut_unmapped.size();
    if (unmapped || (pc & 3) != 0) {
        Host::ReportErrorAsync("R5900 Exception", fmt::format("Jump to an {} address (PC: 0x{:08x})",
                                                              unmapped ? "unmapped" : "unaligned", pc));
        VMManager::SetPaused(true);
        return false;
    }
    const uptr function = entry->GetFnptr();
    reinterpret_cast<void (*)()>(function ? function : reinterpret_cast<uptr>(compile(pc)))();
    return true;
}

void recReserve() {
    std::string error;
    s_code = CodeBuffer::create(CODE_BUFFER_BYTES, error);
    if (!s_code) {
        ERROR_LOG("EE recompiler: no code memory ({}). The interpreter runs instead.", error);
        return;
    }
    INFO_LOG("EE recompiler: {} MB of code memory, writable and executable through separate views.",
             s_code->size() / _1mb);
    resetBlocks();
}

void recShutdown() {
    s_blocks.Reset();
    s_lut_memory.clear();
    s_lut_unmapped.clear();
    s_code.reset();
}

void recResetEE() {
    if (s_executing) {
        s_needs_reset = true;
        safeExitExecution();
        return;
    }
    resetBlocks();
}

// Single-stepping is for debuggers, which use the interpreter.
void recStep() {}

void recExecute() {
    if (s_needs_reset) {
        s_needs_reset = false;
        resetBlocks();
    }
    s_executing = true;
    while (runNextBlock()) {
        if (cpuRegs.cycle < cpuRegs.nextEventCycle) continue;
        _cpuEventTest_Shared();
        if (s_exit_requested) break;
    }
    s_exit_requested = false;
    s_executing = false;
}

void recCancelInstruction() {
    pxFailRel("recCancelInstruction() called; only the interpreter cancels instructions.");
}

// Forgets the blocks that cover any of the `size` words from `addr`, so they compile again from current memory.
void recClear(u32 addr, u32 size) {
    if (!s_code || size == 0) return;
    const u32 end = addr + size * 4;
    for (int index = s_blocks.LastIndex(end - 4); index >= 0; --index) {
        const BASEBLOCKEX& block = *s_blocks[index];
        if (block.startpc >= end) continue;
        if (block.startpc + block.size * 4 <= addr) {
            if (block.startpc + MAX_BLOCK_SPAN <= addr) break;
            continue;
        }
        entryFor(block.startpc)->SetFnptr(0);
        s_blocks.Remove(index, index);
    }
}

} // namespace

R5900cpu recCpu = {
    recReserve, recShutdown, recResetEE, recStep, recExecute, safeExitExecution, recCancelInstruction, recClear,
};

// VMManager selects the recompiler only when this is true: its code memory exists and the user turned it on. It stays
// off by default until it is proven on devices. Upstream's EnableEE is left alone, because turning it off changes how
// the interpreter runs (VU0 cycle sync) and shows a performance warning.
bool BaxEERecIsReady() {
    return s_code.has_value() && Host::GetBaseBoolSettingValue("BetterAetherSX2", "EERecompiler", false);
}

void BaseBlocks_SetBranchTarget(uptr site, uptr target) {
    const std::optional<u32> branch = bax::jit::encodeBranch(site, target);
    pxAssertRel(branch.has_value(), "A block link is outside the range of a direct branch.");
    std::memcpy(s_code->writableAt(reinterpret_cast<const void*>(site)), &*branch, sizeof(u32));
    s_code->flush(site - reinterpret_cast<uptr>(s_code->executable()), sizeof(u32));
}
