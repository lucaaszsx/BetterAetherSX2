// SPDX-License-Identifier: GPL-3.0-or-later
// Runtime of the AArch64 IOP recompiler: its code memory, the block table in upstream's recLUT layout, the generated
// entry code and dispatcher that run blocks back to back, and the R3000Acpu entry points VMManager selects.

#include "iop/iop_rec.h"

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
#include "vixl/aarch64/macro-assembler-aarch64.h"
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
// The farthest back a block can start and still cover an address: its instructions plus a delay slot.
constexpr u32 MAX_BLOCK_SPAN = (bax::jit::iop::MAX_BLOCK_INSTRUCTIONS + 1) * 4;
// PS1 mode (HW_ICFG bit 3), in which the interpreter runs BIOS calls as HLE when a block starts at their addresses.
constexpr u32 PS1_MODE_BIT = 1u << 3;

std::optional<CodeBuffer> s_code;
std::size_t s_code_used = 0;
alignas(16) uptr s_lut[LUT_PAGES];
std::vector<BASEBLOCK> s_lut_memory; // RAM, then the three ROMs; a null entry means not compiled yet.
std::vector<BASEBLOCK> s_lut_unmapped;
BaseBlocks s_blocks;
// Per 4 KB page of IOP RAM, whether a block was compiled from it, so Clear (called on every IOP store) is cheap.
std::vector<bool> s_code_pages;

// The generated entry code at the start of the code buffer; blocks follow it.
using EnterFunction = void (*)();
EnterFunction s_enter = nullptr;
bax::jit::iop::BlockExits s_exits = {};
std::size_t s_entry_code_bytes = 0;

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
    s_code_used = s_entry_code_bytes;
}

const u8* compile(u32 pc) {
    if (s_code_used + bax::jit::iop::MAX_BLOCK_BYTES > s_code->size()) resetBlocks();
    const bax::jit::iop::CompiledBlock block =
        bax::jit::iop::compileBlock(pc, s_exits, s_code->writable() + s_code_used, s_code->executable() + s_code_used);
    s_code->flush(s_code_used, block.bytes);
    s_code_used = (s_code_used + block.bytes + CODE_ALIGNMENT - 1) / CODE_ALIGNMENT * CODE_ALIGNMENT;
    const u32 start = physical(pc);
    BASEBLOCKEX* info = s_blocks.New(start, reinterpret_cast<uptr>(block.entry));
    info->size = block.instructions;
    info->x86size = static_cast<u32>(block.bytes);
    if (start < Ps2MemSize::ExposedIopRam) {
        const u32 last_page = (start + block.instructions * 4 - 1) >> CODE_PAGE_BITS;
        for (u32 page = start >> CODE_PAGE_BITS; page <= last_page && page < s_code_pages.size(); ++page)
            s_code_pages[page] = true;
    }
    for (const bax::jit::iop::LinkSite& link : block.links)
        s_blocks.Link(physical(link.target_pc), reinterpret_cast<s32*>(reinterpret_cast<uptr>(link.site)));
    return block.entry;
}

// The dispatcher's slow path, for a PC whose table entry is empty: runs the interpreter's hooks for that address,
// then returns the block to run, compiling it if needed, or null to leave recompiled code when the PC cannot hold
// code. Blocks at hook addresses stay out of the table, so they always come through here.
const u8* dispatchMiss() {
    if ((psxHu32(HW_ICFG) & PS1_MODE_BIT) != 0 && bax::jit::iop::isHookAddress(psxRegs.pc) &&
        psxRegs.pc != bax::jit::iop::IRX_INJECT_PC)
        psxBiosCall();
    const u32 pc = psxRegs.pc;
    if (pc == bax::jit::iop::IRX_INJECT_PC) bax::jit::iop::injectIrx();
    BASEBLOCK* entry = entryFor(pc);
    const bool unmapped = entry >= s_lut_unmapped.data() && entry < s_lut_unmapped.data() + s_lut_unmapped.size();
    if (unmapped || (pc & 3) != 0) {
        Host::ReportErrorAsync("R3000A Exception", fmt::format("Jump to an {} address (PC: 0x{:08x})",
                                                               unmapped ? "unmapped" : "unaligned", pc));
        VMManager::SetPaused(true);
        return nullptr;
    }
    const BASEBLOCKEX* known = s_blocks.Get(physical(pc));
    const u8* block = known && known->startpc == physical(pc) ? reinterpret_cast<const u8*>(known->fnptr) : compile(pc);
    if (!bax::jit::iop::isHookAddress(pc)) entry->SetFnptr(reinterpret_cast<uptr>(block));
    return block;
}

// Generates, at the start of the code buffer, the entry code that recExecuteBlock calls and the exits blocks jump to.
// Entry saves the registers blocks use (it runs inside the EE's event test, which keeps its own state in them) and
// points x19 at psxRegs; the dispatcher looks the PC up in the block table and jumps to the block, falling back to
// dispatchMiss; the event test runs iopEventTest, then dispatches; leaving restores the registers and returns.
void generateEntryCode() {
    namespace a64 = vixl::aarch64;
    a64::MacroAssembler masm(s_code->writable(), bax::jit::iop::MAX_BLOCK_BYTES, a64::PositionIndependentCode);
    a64::Label dispatch, miss, event_test, leave;
    masm.Stp(a64::x29, a64::x30, a64::MemOperand(a64::sp, -48, a64::PreIndex));
    masm.Mov(a64::x29, a64::sp);
    masm.Stp(a64::x19, a64::x20, a64::MemOperand(a64::sp, 16));
    masm.Stp(a64::x21, a64::x22, a64::MemOperand(a64::sp, 32));
    masm.Mov(a64::x19, reinterpret_cast<uintptr_t>(&psxRegs));

    // PC_GETBLOCK_: s_lut[pc >> 16] + pc * 2 bytes is the BASEBLOCK; its first field is the block's address.
    masm.Bind(&dispatch);
    masm.Ldr(a64::w0, a64::MemOperand(a64::x19, reinterpret_cast<const u8*>(&psxRegs.pc) -
                                                    reinterpret_cast<const u8*>(&psxRegs)));
    masm.Mov(a64::x1, reinterpret_cast<uintptr_t>(s_lut));
    masm.Lsr(a64::w2, a64::w0, 16);
    masm.Ldr(a64::x1, a64::MemOperand(a64::x1, a64::x2, a64::LSL, 3));
    masm.Add(a64::x1, a64::x1, a64::Operand(a64::x0, a64::LSL, 1));
    masm.Ldr(a64::x1, a64::MemOperand(a64::x1));
    masm.Cbz(a64::x1, &miss);
    masm.Br(a64::x1);

    masm.Bind(&miss);
    masm.Mov(a64::x9, reinterpret_cast<uintptr_t>(&dispatchMiss));
    masm.Blr(a64::x9);
    masm.Cbz(a64::x0, &leave);
    masm.Br(a64::x0);

    masm.Bind(&event_test);
    masm.Mov(a64::x9, reinterpret_cast<uintptr_t>(&iopEventTest));
    masm.Blr(a64::x9);
    masm.B(&dispatch);

    masm.Bind(&leave);
    masm.Ldp(a64::x21, a64::x22, a64::MemOperand(a64::sp, 32));
    masm.Ldp(a64::x19, a64::x20, a64::MemOperand(a64::sp, 16));
    masm.Ldp(a64::x29, a64::x30, a64::MemOperand(a64::sp, 48, a64::PostIndex));
    masm.Ret();
    masm.FinalizeCode();

    const u8* executable = s_code->executable();
    s_enter = reinterpret_cast<EnterFunction>(reinterpret_cast<uptr>(executable));
    s_exits = {executable + dispatch.GetLocation(), executable + event_test.GetLocation(),
               executable + leave.GetLocation()};
    // A link whose target block goes away (BaseBlocks::Remove) goes back to the dispatcher.
    s_blocks.SetJITCompile(s_exits.dispatch);
    s_entry_code_bytes = (masm.GetSizeOfCodeGenerated() + CODE_ALIGNMENT - 1) / CODE_ALIGNMENT * CODE_ALIGNMENT;
    s_code->flush(0, masm.GetSizeOfCodeGenerated());
}

void recReserve() {
    std::string error;
    s_code = CodeBuffer::create(CODE_BUFFER_BYTES, error);
    if (!s_code) {
        ERROR_LOG("IOP recompiler: no code memory ({}). The interpreter runs instead.", error);
        return;
    }
    generateEntryCode();
    resetBlocks();
}

void recReset() {
    if (s_code) resetBlocks();
}

// Runs IOP blocks for eeCycles worth of EE time; blocks subtract the time they take, as intExecuteBlock does. Without
// code memory the interpreter runs instead.
s32 recExecuteBlock(s32 eeCycles) {
    if (!s_code) return psxInt.ExecuteBlock(eeCycles);
    psxRegs.iopBreak = 0;
    psxRegs.iopCycleEE = eeCycles;
    if (eeCycles > 0) s_enter();
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

namespace bax::jit::iop {

const CodeBuffer* codeBuffer() {
    return s_code ? &*s_code : nullptr;
}

} // namespace bax::jit::iop
