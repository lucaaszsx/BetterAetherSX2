// SPDX-License-Identifier: GPL-3.0-or-later
// Runtime of the AArch64 EE recompiler: its code memory, the block table in upstream's recLUT layout (so vtlb and
// Clear keep working), the generated entry code and dispatcher that run blocks back to back, and the R5900cpu entry
// points VMManager selects.

#include "ee/ee_rec.h"

#include "Host.h"
#include "Memory.h"
#include "R3000A.h"
#include "R5900.h"
#include "VMManager.h"
#include "bax/jit/code_buffer.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "ee/ee_block.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"
#include "vtlb.h"
#include "x86/BaseblockEx.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
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
// Per 64 KB page, what to add to a virtual address to get its physical one (upstream's hwLUT).
alignas(16) u32 s_physical_lut[LUT_PAGES];
std::vector<BASEBLOCK> s_lut_memory; // RAM, then the three ROMs; a null entry means not compiled yet.
std::vector<BASEBLOCK> s_lut_unmapped;
BaseBlocks s_blocks;

// The generated entry code at the start of the code buffer; blocks follow it.
using EnterFunction = void (*)();
EnterFunction s_enter = nullptr;
bax::jit::ee::BlockExits s_exits = {};
std::size_t s_entry_code_bytes = 0;

u32 s_stop_pc = NO_STOP_PC;
s8 s_compiled_cycle_rate = 0;
// The differential tester's limit on cycles, so a program that never reaches its end cannot hang it.
constexpr u64 TEST_CYCLE_BUDGET = 1u << 20;
u64 s_test_deadline = 0;
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
    for (uint page = 0; page < LUT_PAGES; ++page)
        recLUT_SetPage(s_lut, s_physical_lut, s_lut_unmapped.data(), page, 0, 0);
    for (uint page = 0; page < Ps2MemSize::ExposedRam / 0x10000; ++page)
        for (uint base : {0x0000u, 0x2000u, 0x3000u, 0x8000u, 0xa000u, 0xb000u, 0xc000u, 0xd000u})
            recLUT_SetPage(s_lut, s_physical_lut, ram, base, page, page);
    const auto mapRom = [](BASEBLOCK* map, uint first_page, uint end_page) {
        for (uint page = first_page; page < end_page; ++page)
            for (uint base : {0x0000u, 0x8000u, 0xa000u})
                recLUT_SetPage(s_lut, s_physical_lut, map, base, page, page - first_page);
    };
    mapRom(rom, 0x1fc0, 0x2000);
    mapRom(rom1, 0x1e00, 0x1e40);
    mapRom(rom2, 0x1e40, 0x1e80);
}

void resetBlocks() {
    s_compiled_cycle_rate = EmuConfig.Speedhacks.EECycleRate;
    mapLut();
    s_blocks.Reset();
    s_code_used = s_entry_code_bytes;
}

// Keeps a block from running stale code, as upstream's x86 recompiler does. Its RAM page is write-protected, so any
// write to it (EE stores, DMA, disc reads) faults, and vtlb's handler clears the page's blocks and leaves the page
// unprotected. Code on such a page, or on the kernel's thread-context pages, instead checks its own words before each
// run. Returns where those words live when the block must check them, or null.
const u32* protectCode(u32 pc) {
    const u32 physical = s_physical_lut[pc >> 16] + pc;
    // The kernel keeps thread contexts at 0x800010c0-0x80001300 and the idle thread's at 0x81000.
    const bool thread_context = (pc >> 12) == 0x81 || (pc >> 12) == 0x80001;
    switch (thread_context ? ProtMode_Manual : mmap_GetRamPageInfo(physical)) {
    case ProtMode_None:
    case ProtMode_Write:
        mmap_MarkCountedRamPage(physical);
        return nullptr;
    case ProtMode_Manual:
        return reinterpret_cast<const u32*>(PSM(physical));
    case ProtMode_NotRequired:
        return nullptr;
    }
    return nullptr;
}

const u8* compile(u32 pc) {
    if (s_code_used + bax::jit::ee::MAX_BLOCK_BYTES > s_code->size()) resetBlocks();
    const bax::jit::ee::CompiledBlock block = bax::jit::ee::compileBlock(
        pc, s_stop_pc, protectCode(pc), s_exits, s_code->writable() + s_code_used, s_code->executable() + s_code_used);
    s_code->flush(s_code_used, block.bytes);
    s_code_used = (s_code_used + block.bytes + CODE_ALIGNMENT - 1) / CODE_ALIGNMENT * CODE_ALIGNMENT;
    BASEBLOCKEX* info = s_blocks.New(pc, reinterpret_cast<uptr>(block.entry));
    info->size = block.instructions;
    info->x86size = static_cast<u32>(block.bytes);
    // Direct links skip the dispatcher, and with it the boot hooks, so they wait until the game's ELF has booted. The
    // differential tester, which never boots one, links too: its stop address never gets a block, so a link there
    // stays aimed at the dispatcher.
    if (VMManager::Internal::HasBootedELF() || s_stop_pc != NO_STOP_PC)
        for (const bax::jit::ee::LinkSite& link : block.links)
            s_blocks.Link(link.target_pc, reinterpret_cast<s32*>(reinterpret_cast<uptr>(link.site)));
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

// Where EELOAD keeps the JAL to its main function, and the JALs to the function that calls ExecPS2, by BIOS
// version (v1.00-v1.10; v1.20-v1.60 3000x; v1.60 3900x; v1.70-v2.30), with the two places that function lives.
// The offsets come from upstream's interpreter (Interpreter.cpp), which reads them the same way.
constexpr u32 EELOAD_MAIN_JUMP = EELOAD_START + 0x9c;
constexpr u32 EELOAD_EXEC_JUMP_A = EELOAD_START + 0x470;
constexpr u32 EELOAD_EXEC_JUMP_B = EELOAD_START + 0x5b0;
constexpr u32 EELOAD_EXEC_JUMP_C = EELOAD_START + 0x618;
constexpr u32 EELOAD_EXEC_JUMP_D = EELOAD_START + 0x600;
constexpr u32 EELOAD_EXEC_LATE = EELOAD_START + 0x2b8;
constexpr u32 EELOAD_EXEC_EARLY = EELOAD_START + 0x170;
constexpr u32 JAL_OPCODE = 3;

bool isJal(u32 code) {
    return code >> 26 == JAL_OPCODE;
}

// The EELOAD function that calls ExecPS2, which eeloadHook2 patches so fast boot can pass launch arguments.
u32 findEeloadExec() {
    if (isJal(memRead32(EELOAD_EXEC_JUMP_B)) || isJal(memRead32(EELOAD_EXEC_JUMP_C)) ||
        isJal(memRead32(EELOAD_EXEC_JUMP_D)))
        return EELOAD_EXEC_LATE;
    if (isJal(memRead32(EELOAD_EXEC_JUMP_A))) return EELOAD_EXEC_EARLY;
    WARNING_LOG("EE recompiler: unknown EELOAD version; fast boot cannot pass launch arguments.");
    return 0;
}

// The hooks upstream's interpreter runs at fixed addresses while the BIOS boots a disc: eeloadHook hands EELOAD the
// game's ELF on fast boot and adds launch arguments, eeloadHook2 lets EELOAD accept them, and reaching the ELF's
// entry point applies the game's patches and database settings. Without them the game does not boot as it should.
void runBootHooks(u32 pc) {
    if (pc == EELOAD_START) {
        const u32 main_jump = memRead32(EELOAD_MAIN_JUMP);
        if (isJal(main_jump)) g_eeloadMain = ((EELOAD_MAIN_JUMP + 4) & 0xf0000000u) | ((main_jump << 2) & 0x0fffffffu);
    } else if (g_eeloadMain != 0 && pc == g_eeloadMain) {
        eeloadHook();
        if (VMManager::Internal::IsFastBootInProgress()) g_eeloadExec = findEeloadExec();
    } else if (g_eeloadExec != 0 && pc == g_eeloadExec) {
        eeloadHook2();
    } else if (pc == VMManager::Internal::GetCurrentELFEntryPoint()) {
        VMManager::Internal::EntryPointCompilingOnCPUThread();
    }
}

// The dispatcher's slow path, for a PC whose table entry is empty: returns the block to run there, compiling it if
// needed, or null to leave recompiled code (the tester's stop address, or a PC that cannot hold code). While the BIOS
// is still booting the disc, blocks stay out of the table so every block comes through here for the boot hooks.
const u8* dispatchMiss() {
    const u32 pc = cpuRegs.pc;
    if (pc == s_stop_pc) return nullptr;
    const bool booted = VMManager::Internal::HasBootedELF();
    if (!booted) runBootHooks(pc);
    BASEBLOCK* entry = entryFor(pc);
    const bool unmapped = entry >= s_lut_unmapped.data() && entry < s_lut_unmapped.data() + s_lut_unmapped.size();
    if (unmapped || (pc & 3) != 0) {
        Host::ReportErrorAsync("R5900 Exception", fmt::format("Jump to an {} address (PC: 0x{:08x})",
                                                              unmapped ? "unmapped" : "unaligned", pc));
        VMManager::SetPaused(true);
        return nullptr;
    }
    const BASEBLOCKEX* known = s_blocks.Get(pc);
    const u8* block = known && known->startpc == pc ? reinterpret_cast<const u8*>(known->fnptr) : compile(pc);
    if (booted) entry->SetFnptr(reinterpret_cast<uptr>(block));
    return block;
}

// Runs the event test once the cycle count reaches the next event. Returns false to leave recompiled code: an exit
// was requested, or the tester's cycle budget ran out.
bool eventTest() {
    _cpuEventTest_Shared();
    // Blocks commit cycles for the EE cycle rate they were compiled under; no block is running here.
    if (EmuConfig.Speedhacks.EECycleRate != s_compiled_cycle_rate) {
        s_compiled_cycle_rate = EmuConfig.Speedhacks.EECycleRate;
        resetBlocks();
    }
    if (s_stop_pc != NO_STOP_PC && cpuRegs.cycle >= s_test_deadline) return false;
    return !std::exchange(s_exit_requested, false);
}

// Generates, at the start of the code buffer, the entry code that recExecute calls and the dispatcher blocks jump
// to. Entry saves the registers blocks use and points STATE at cpuRegs; the dispatcher looks the PC up in the block
// table and jumps to the block, falling back to dispatchMiss; leaving restores the registers and returns.
void generateEntryCode() {
    namespace a64 = vixl::aarch64;
    a64::MacroAssembler masm(s_code->writable(), bax::jit::ee::MAX_BLOCK_BYTES, a64::PositionIndependentCode);
    a64::Label dispatch, miss, event_test, leave;
    masm.Stp(a64::x29, a64::x30, a64::MemOperand(a64::sp, -48, a64::PreIndex));
    masm.Mov(a64::x29, a64::sp);
    masm.Stp(a64::x19, a64::x20, a64::MemOperand(a64::sp, 16));
    masm.Stp(a64::x21, a64::x22, a64::MemOperand(a64::sp, 32));
    masm.Mov(a64::x19, reinterpret_cast<uintptr_t>(&cpuRegs));

    // PC_GETBLOCK_: s_lut[pc >> 16] + pc * 2 bytes is the BASEBLOCK; its first field is the block's address.
    masm.Bind(&dispatch);
    masm.Ldr(a64::w0, a64::MemOperand(a64::x19, reinterpret_cast<const u8*>(&cpuRegs.pc) -
                                                    reinterpret_cast<const u8*>(&cpuRegs)));
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
    masm.Mov(a64::x9, reinterpret_cast<uintptr_t>(&eventTest));
    masm.Blr(a64::x9);
    masm.Cbnz(a64::w0, &dispatch);

    masm.Bind(&leave);
    masm.Ldp(a64::x21, a64::x22, a64::MemOperand(a64::sp, 32));
    masm.Ldp(a64::x19, a64::x20, a64::MemOperand(a64::sp, 16));
    masm.Ldp(a64::x29, a64::x30, a64::MemOperand(a64::sp, 48, a64::PostIndex));
    masm.Ret();
    masm.FinalizeCode();

    const u8* executable = s_code->executable();
    s_enter = reinterpret_cast<EnterFunction>(reinterpret_cast<uptr>(executable));
    s_exits = {executable + dispatch.GetLocation(), executable + event_test.GetLocation()};
    // A link whose target block goes away (BaseBlocks::Remove) goes back to the dispatcher.
    s_blocks.SetJITCompile(s_exits.dispatch);
    s_entry_code_bytes = (masm.GetSizeOfCodeGenerated() + CODE_ALIGNMENT - 1) / CODE_ALIGNMENT * CODE_ALIGNMENT;
    s_code->flush(0, masm.GetSizeOfCodeGenerated());
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
    generateEntryCode();
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
    s_enter();
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

namespace bax::jit::ee {

void discardBlock(u32 start_pc, u32 words) {
    recClear(start_pc, words);
}

const CodeBuffer* codeBuffer() {
    return s_code ? &*s_code : nullptr;
}

bool runUntil(u32 stop_pc) {
    if (!s_code) return false;
    // Blocks compiled for another stop address may run past this one.
    if (stop_pc != s_stop_pc) {
        s_stop_pc = stop_pc;
        resetBlocks();
    }
    s_test_deadline = cpuRegs.cycle + TEST_CYCLE_BUDGET;
    cpuRegs.nextEventCycle = std::min(cpuRegs.nextEventCycle, s_test_deadline);
    s_enter();
    return cpuRegs.pc == stop_pc;
}

} // namespace bax::jit::ee

R5900cpu recCpu = {
    recReserve, recShutdown, recResetEE, recStep, recExecute, safeExitExecution, recCancelInstruction, recClear,
};

// VMManager selects the recompiler only when this is true: its code memory exists and the user turned it on. It stays
// off by default until it is proven on devices. Upstream's EnableEE is left alone, because turning it off changes how
// the interpreter runs (VU0 cycle sync) and shows a performance warning.
bool BaxEERecIsReady() {
    return s_code.has_value() && Host::GetBaseBoolSettingValue("BetterAetherSX2", "EERecompiler", false);
}
