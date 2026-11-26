// SPDX-License-Identifier: GPL-3.0-or-later
// Runtime of the AArch64 IOP recompiler: its code memory, the block table in upstream's recLUT layout, the loop that
// runs blocks with the interpreter's cycle accounting, and the R3000Acpu entry points VMManager selects.

#include "Config.h"
#include "Host.h"
#include "IopBios.h"
#include "IopHw.h"
#include "IopMem.h"
#include "R3000A.h"
#include "VMManager.h"
#include "bax/jit/code_buffer.h"
#include "common/Console.h"
#include "iop/iop_block.h"
#include "x86/BaseblockEx.h"

#include <optional>
#include <string>
#include <vector>

namespace {

using bax::jit::CodeBuffer;

constexpr std::size_t CODE_BUFFER_BYTES = 32 * _1mb;
constexpr std::size_t CODE_ALIGNMENT = 16;
constexpr u32 LUT_PAGES = 0x10000;
constexpr u32 ENTRIES_PER_PAGE = 0x10000 / 4;
// IOP RAM mirrors repeat through the first 8 MB of each segment, as upstream maps them.
constexpr u32 RAM_MIRROR_END = 0x00800000;
constexpr u32 SEGMENT_MASK = 0x1fffffff;
constexpr u32 CODE_PAGE_BITS = 12;
// The farthest back a block can start and still cover an address.
constexpr u32 MAX_BLOCK_SPAN = bax::jit::iop::MAX_BLOCK_INSTRUCTIONS * 4;
// PS1 mode's BIOS calls, which the interpreter runs as HLE when the PC reaches them (HW_ICFG bit 3).
constexpr u32 PS1_MODE_BIT = 1u << 3;
constexpr u32 PS1_BIOS_CALLS[] = {0xa0, 0xb0, 0xc0};
// IOP clock ratios from the interpreter's intExecuteBlock: 8 EE cycles per IOP cycle in PS2 mode, and
// PS2CLK / PSXCLK = 1280 / 147 in PS1 mode.
constexpr u32 EE_CYCLES_PER_IOP_CYCLE = 8;
constexpr u32 PS1_CLOCK_NUMERATOR = 1280;
constexpr u32 PS1_CLOCK_DENOMINATOR = 147;
// The interpreter's IRX-injection hack (execI): module count address and value, and the entry it patches in.
constexpr u32 IRX_MODULE_COUNT_ADDRESS = 0x20018;
constexpr u32 IRX_EXPECTED_MODULE_COUNT = 0x1f;
constexpr u32 IRX_ENTRY_ADDRESS = 0x20094;
constexpr u32 IRX_INJECTED_ENTRY = 0xbffc0000;

std::optional<CodeBuffer> s_code;
std::size_t s_code_used = 0;
alignas(16) uptr s_lut[LUT_PAGES];
std::vector<BASEBLOCK> s_lut_memory; // RAM, then the three ROMs; a null entry means not compiled yet.
std::vector<BASEBLOCK> s_lut_unmapped;
BaseBlocks s_blocks;
// Per 4 KB page of IOP RAM, whether a block was compiled from it, so Clear (called on every IOP store) is cheap.
std::vector<bool> s_code_pages;

// The address blocks are kept under: RAM folded onto its first mirror, the rest without its segment.
u32 physical(u32 address) {
    const u32 unsegmented = address & SEGMENT_MASK;
    return unsegmented < RAM_MIRROR_END ? unsegmented & (Ps2MemSize::ExposedIopRam - 1) : unsegmented;
}

BASEBLOCK* entryFor(u32 pc) {
    return PC_GETBLOCK_(pc, s_lut);
}

void mapLut() {
    s_lut_memory.assign((Ps2MemSize::ExposedIopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4, {});
    s_lut_unmapped.assign(ENTRIES_PER_PAGE, {});
    s_code_pages.assign(Ps2MemSize::ExposedIopRam >> CODE_PAGE_BITS, false);
    BASEBLOCK* ram = s_lut_memory.data();
    BASEBLOCK* rom = ram + Ps2MemSize::ExposedIopRam / 4;
    BASEBLOCK* rom1 = rom + Ps2MemSize::Rom / 4;
    BASEBLOCK* rom2 = rom1 + Ps2MemSize::Rom1 / 4;
    for (uint page = 0; page < LUT_PAGES; ++page) recLUT_SetPage(s_lut, nullptr, s_lut_unmapped.data(), page, 0, 0);
    const uint ram_page_mask = Ps2MemSize::ExposedIopRam / 0x10000 - 1;
    for (uint page = 0; page < RAM_MIRROR_END / 0x10000; ++page)
        for (uint base : {0x0000u, 0x8000u, 0xa000u})
            recLUT_SetPage(s_lut, nullptr, ram, base, page, page & ram_page_mask);
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
    if (s_code_used + bax::jit::iop::MAX_BLOCK_BYTES > s_code->size()) resetBlocks();
    const bax::jit::iop::CompiledBlock block =
        bax::jit::iop::compileBlock(pc, s_code->writable() + s_code_used, s_code->executable() + s_code_used);
    s_code->flush(s_code_used, block.bytes);
    s_code_used = (s_code_used + block.bytes + CODE_ALIGNMENT - 1) / CODE_ALIGNMENT * CODE_ALIGNMENT;
    const u32 start = physical(pc);
    BASEBLOCKEX* info = s_blocks.New(start, reinterpret_cast<uptr>(block.entry));
    info->size = block.instructions;
    info->x86size = static_cast<u32>(block.bytes);
    if (start < Ps2MemSize::ExposedIopRam)
        for (u32 page = start >> CODE_PAGE_BITS; page <= (start + block.instructions * 4 - 1) >> CODE_PAGE_BITS; ++page)
            s_code_pages[page] = true;
    entryFor(start)->SetFnptr(reinterpret_cast<uptr>(block.entry));
    return block.entry;
}

// Runs the block at psxRegs.pc, compiling it first if needed. False when the PC cannot hold code.
bool runNextBlock() {
    const u32 pc = psxRegs.pc;
    BASEBLOCK* entry = entryFor(pc);
    const bool unmapped = entry >= s_lut_unmapped.data() && entry < s_lut_unmapped.data() + s_lut_unmapped.size();
    if (unmapped || (pc & 3) != 0) {
        Host::ReportErrorAsync("R3000A Exception", fmt::format("Jump to an {} address (PC: 0x{:08x})",
                                                               unmapped ? "unmapped" : "unaligned", pc));
        VMManager::SetPaused(true);
        return false;
    }
    const uptr function = entry->GetFnptr();
    reinterpret_cast<void (*)()>(function ? function : reinterpret_cast<uptr>(compile(pc)))();
    return true;
}

// What the interpreter's intExecuteBlock and execI do between instructions, at the start of a block.
void runInterpreterHooks() {
    const u32 pc = psxRegs.pc;
    if ((psxHu32(HW_ICFG) & PS1_MODE_BIT) != 0)
        for (const u32 call : PS1_BIOS_CALLS)
            if ((pc & SEGMENT_MASK) == call) psxBiosCall();
    if (pc == bax::jit::iop::IRX_INJECT_PC && EmuConfig.CurrentIRX.length() > 3 &&
        iopMemRead32(IRX_MODULE_COUNT_ADDRESS) == IRX_EXPECTED_MODULE_COUNT)
        iopMemWrite32(IRX_ENTRY_ADDRESS, IRX_INJECTED_ENTRY);
}

void recReserve() {
    std::string error;
    s_code = CodeBuffer::create(CODE_BUFFER_BYTES, error);
    if (!s_code) {
        ERROR_LOG("IOP recompiler: no code memory ({}). The interpreter runs instead.", error);
        return;
    }
    resetBlocks();
}

void recReset() {
    if (s_code) resetBlocks();
}

// Runs IOP blocks for eeCycles worth of EE time, with the interpreter's accounting (intExecuteBlock).
s32 recExecuteBlock(s32 eeCycles) {
    psxRegs.iopBreak = 0;
    psxRegs.iopCycleEE = eeCycles;
    while (psxRegs.iopCycleEE > 0) {
        const u64 start_cycle = psxRegs.cycle;
        runInterpreterHooks();
        if (!runNextBlock()) break;
        const u64 iop_cycles = psxRegs.cycle - start_cycle;
        if ((psxHu32(HW_ICFG) & PS1_MODE_BIT) != 0) {
            const u64 scaled = PS1_CLOCK_NUMERATOR * iop_cycles + psxRegs.iopCycleEECarry;
            psxRegs.iopCycleEE -= static_cast<s32>(scaled / PS1_CLOCK_DENOMINATOR);
            psxRegs.iopCycleEECarry = static_cast<u32>(scaled % PS1_CLOCK_DENOMINATOR);
        } else {
            psxRegs.iopCycleEE -= static_cast<s32>(iop_cycles * EE_CYCLES_PER_IOP_CYCLE);
        }
    }
    return psxRegs.iopBreak + psxRegs.iopCycleEE;
}

// Forgets the blocks that cover any of the `size` words from `address`. Every IOP store and DMA into IOP RAM calls
// this, so pages without compiled code return at once.
void recClear(u32 address, u32 size) {
    if (!s_code || size == 0) return;
    const u32 start = physical(address);
    const u32 end = start + size * 4;
    if (start < Ps2MemSize::ExposedIopRam) {
        bool has_code = false;
        for (u32 page = start >> CODE_PAGE_BITS; page <= (end - 1) >> CODE_PAGE_BITS && !has_code; ++page)
            has_code = page < s_code_pages.size() && s_code_pages[page];
        if (!has_code) return;
    }
    for (int index = s_blocks.LastIndex(end - 4); index >= 0; --index) {
        const BASEBLOCKEX& block = *s_blocks[index];
        if (block.startpc >= end) continue;
        if (block.startpc + block.size * 4 <= start) {
            if (block.startpc + MAX_BLOCK_SPAN <= start) break;
            continue;
        }
        entryFor(block.startpc)->SetFnptr(0);
        s_blocks.Remove(index, index);
    }
}

void recShutdown() {
    s_blocks.Reset();
    s_lut_memory.clear();
    s_lut_unmapped.clear();
    s_code_pages.clear();
    s_code.reset();
}

} // namespace

R3000Acpu psxRec = {recReserve, recReset, recExecuteBlock, recClear, recShutdown};

// VMManager selects this recompiler only when its code memory exists and the user turned it on; it stays off by
// default until it is proven on devices.
bool BaxIOPRecIsReady() {
    return s_code.has_value() && Host::GetBaseBoolSettingValue("BetterAetherSX2", "IOPRecompiler", false);
}
