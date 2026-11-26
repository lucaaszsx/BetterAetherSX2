// SPDX-License-Identifier: GPL-3.0-or-later
// Block compiler of the AArch64 IOP recompiler. Integer instructions, branches, their delay slots and the block's cycle
// accounting are native code; other instructions call the interpreter's functions. Calls are absolute (address in a register, then BLR), so they
// do not depend on which view of the code buffer the bytes were written through.

#include "iop/iop_block.h"

#include "Config.h"
#include "DebugTools/SymbolGuardian.h"
#include "IopBios.h"
#include "IopHw.h"
#include "IopMem.h"
#include "R3000A.h"
#include "bax/jit/a64_branch.h"
#include "iop/iop_alu.h"
#include "iop/iop_memory.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

#include <optional>
#include <utility>

namespace bax::jit::iop {
namespace {

namespace a64 = vixl::aarch64;

// Callee-saved, so the interpreter calls leave them alone. The runtime's entry code sets STATE and saves all three.
const a64::Register& STATE = a64::x19;  // &psxRegs
const a64::Register& TAKEN = a64::w21;  // Whether the block's conditional branch is taken.
const a64::Register& TARGET = a64::w22; // A jump register's target.
// Holds call and jump targets. Not one of VIXL's scratch registers (x16, x17), which its macros may use in between.
const a64::Register& CALL_TARGET = a64::x9;

constexpr u32 PAGE_MASK = 0xfff;
constexpr u32 SEGMENT_MASK = 0x1fffffff;
constexpr u32 LINK_REGISTER = 31;
// A link register gets the address past the delay slot.
constexpr u32 LINK_OFFSET = 8;

// MIPS I opcodes, SPECIAL functions and REGIMM kinds this compiler tells apart.
constexpr u32 SPECIAL = 0, REGIMM = 1, J = 2, JAL = 3, BEQ = 4, BNE = 5, BLEZ = 6, BGTZ = 7;
constexpr u32 JR = 8, JALR = 9, SYSCALL = 12, BREAK = 13;
constexpr u32 BLTZ = 0, BGEZ = 1, BLTZAL = 0x10, BGEZAL = 0x11;

// psxJ treats a J whose delay slot has this upper half as a call through an IRX import table.
constexpr u32 IRX_IMPORT_SLOT = 0x2400;
// doBranch's hooks: SYSMEM's entry, reached again on an IOP reboot, and IOPBOOT, which gets the IOP RAM size in a0.
constexpr u32 SYSMEM_ENTRY = 0x890;
constexpr u32 IOPBOOT_ENTRY = 0xbfc4a000;
// The interpreter's IRX-injection hack (execI): module count address and value, and the entry it patches in.
constexpr u32 IRX_MODULE_COUNT_ADDRESS = 0x20018;
constexpr u32 IRX_EXPECTED_MODULE_COUNT = 0x1f;
constexpr u32 IRX_ENTRY_ADDRESS = 0x20094;
constexpr u32 IRX_INJECTED_ENTRY = 0xbffc0000;
// PS1 mode (HW_ICFG bit 3) runs the IOP at PSXCLK: PS2CLK / PSXCLK = 1280 / 147 EE cycles per IOP cycle, carried as
// intExecuteBlock carries it. PS2 mode runs 8 EE cycles per IOP cycle.
constexpr unsigned PS1_MODE_BIT = 3;
constexpr u32 PS1_CLOCK_NUMERATOR = 1280;
constexpr u32 PS1_CLOCK_DENOMINATOR = 147;
constexpr u32 EE_CYCLES_PER_IOP_CYCLE = 8;

u32 opcodeOf(u32 code) {
    return code >> 26;
}
u32 rsOf(u32 code) {
    return (code >> 21) & 0x1f;
}
u32 rtOf(u32 code) {
    return (code >> 16) & 0x1f;
}
u32 rdOf(u32 code) {
    return (code >> 11) & 0x1f;
}
u32 functionOf(u32 code) {
    return code & 0x3f;
}

bool isJumpRegister(u32 code) {
    return opcodeOf(code) == SPECIAL && (functionOf(code) == JR || functionOf(code) == JALR);
}

bool isBranch(u32 code) {
    switch (opcodeOf(code)) {
    case SPECIAL:
        return isJumpRegister(code);
    case REGIMM: {
        const u32 kind = rtOf(code);
        return kind == BLTZ || kind == BGEZ || kind == BLTZAL || kind == BGEZAL;
    }
    default:
        return opcodeOf(code) >= J && opcodeOf(code) <= BGTZ;
    }
}

// SYSCALL and BREAK move the PC to the exception vector; no other instruction the blocks run does.
bool raisesException(u32 code) {
    return opcodeOf(code) == SPECIAL && (functionOf(code) == SYSCALL || functionOf(code) == BREAK);
}

// A branch's target when it is known at compile time; branch_pc is the branch's own address.
std::optional<u32> staticTarget(u32 code, u32 branch_pc) {
    const u32 slot_pc = branch_pc + 4;
    if (isJumpRegister(code)) return std::nullopt;
    if (opcodeOf(code) == J || opcodeOf(code) == JAL) return (slot_pc & 0xf0000000) | ((code & 0x03ffffff) << 2);
    return slot_pc + (static_cast<u32>(static_cast<s32>(static_cast<s16>(code & 0xffff))) << 2);
}

std::ptrdiff_t offsetIn(const void* field) {
    return static_cast<const u8*>(field) - reinterpret_cast<const u8*>(&psxRegs);
}

a64::MemOperand gpr(u32 index) {
    return a64::MemOperand(STATE, offsetIn(&psxRegs.GPR.r[index]));
}

void callAbsolute(a64::MacroAssembler& masm, const void* function) {
    masm.Mov(CALL_TARGET, reinterpret_cast<uintptr_t>(function));
    masm.Blr(CALL_TARGET);
}

void jumpAbsolute(a64::MacroAssembler& masm, const void* target) {
    masm.Mov(CALL_TARGET, reinterpret_cast<uintptr_t>(target));
    masm.Br(CALL_TARGET);
}

void storeWord(a64::MacroAssembler& masm, u32 value, const a64::MemOperand& destination) {
    masm.Mov(a64::w0, value);
    masm.Str(a64::w0, destination);
}

void runBranchHooks(u32 target) {
    if (target == SYSMEM_ENTRY) R3000SymbolGuardian.ClearIrxModules();
    if (target == IOPBOOT_ENTRY) psxRegs.GPR.n.a0 = Ps2MemSize::ExposedIopRam >> 20;
}

// psxJ's call through an IRX import table, marked by the delay slot word: true when the module's function ran as HLE
// and left the PC at its return.
bool runImportHle(u32 slot_pc) {
    const u32 slot = iopMemRead32(slot_pc);
    return slot >> 16 == IRX_IMPORT_SLOT &&
           R3000A::irxImportExec(R3000A::irxImportTableAddr(slot_pc), static_cast<u16>(slot & 0xffff)) != 0;
}

// A delay slot that is itself a branch, SYSCALL or BREAK, or lies on the next page, run as doBranch runs every slot:
// through execI, marked as a delay slot, from the word at psxRegs.pc.
void runDelaySlot() {
    iopIsDelaySlot = true;
    if (psxRegs.pc == IRX_INJECT_PC) injectIrx();
    psxRegs.code = iopMemRead32(psxRegs.pc);
    psxRegs.pc += 4;
    psxBSC[opcodeOf(psxRegs.code)]();
    iopIsDelaySlot = false;
}

void subtractPs1Cycles(u32 iop_cycles) {
    const u32 scaled = PS1_CLOCK_NUMERATOR * iop_cycles + psxRegs.iopCycleEECarry;
    psxRegs.iopCycleEE -= static_cast<s32>(scaled / PS1_CLOCK_DENOMINATOR);
    psxRegs.iopCycleEECarry = scaled % PS1_CLOCK_DENOMINATOR;
}

void emitInterpreterCall(a64::MacroAssembler& masm, u32 code) {
    storeWord(masm, code, a64::MemOperand(STATE, offsetIn(&psxRegs.code)));
    callAbsolute(masm, reinterpret_cast<const void*>(psxBSC[opcodeOf(code)]));
}

// Runs `code` as native code when it is an integer instruction or a load or store, else through the interpreter's
// function.
void emitInstruction(a64::MacroAssembler& masm, u32 code) {
    if (emitNativeAlu(masm, STATE, code)) return;
    if (!isFastMemoryAccess(code)) return emitInterpreterCall(masm, code);
    emitMemoryAccess(masm, STATE, code, codePages(), [&] { emitInterpreterCall(masm, code); });
}

// Writes the branch's link register, then decides it: TAKEN for a conditional branch, TARGET for a jump register.
// The interpreter writes the link first, so BLTZAL and BGEZAL on $ra, and JALR with rd equal to rs, read the new value.
void emitDecision(a64::MacroAssembler& masm, u32 code, u32 branch_pc) {
    const u32 link = branch_pc + LINK_OFFSET;
    const u32 opcode = opcodeOf(code);
    if (opcode == JAL || (opcode == REGIMM && (rtOf(code) & 0x10) != 0)) storeWord(masm, link, gpr(LINK_REGISTER));
    if (isJumpRegister(code)) {
        if (functionOf(code) == JALR && rdOf(code) != 0) storeWord(masm, link, gpr(rdOf(code)));
        masm.Ldr(TARGET, gpr(rsOf(code)));
        return;
    }
    if (opcode == J || opcode == JAL) return;
    masm.Ldr(a64::w0, gpr(rsOf(code)));
    if (opcode == BEQ || opcode == BNE) {
        masm.Ldr(a64::w1, gpr(rtOf(code)));
        masm.Cmp(a64::w0, a64::w1);
    } else {
        masm.Cmp(a64::w0, 0);
    }
    a64::Condition taken = a64::al;
    switch (opcode) {
    case BEQ:
        taken = a64::eq;
        break;
    case BNE:
        taken = a64::ne;
        break;
    case BLEZ:
        taken = a64::le;
        break;
    case BGTZ:
        taken = a64::gt;
        break;
    default:
        taken = (rtOf(code) & 1) != 0 ? a64::ge : a64::lt;
        break;
    }
    masm.Cset(TAKEN, taken);
}

// doBranch's hooks, for a taken branch, before its delay slot runs.
void emitBranchHooks(a64::MacroAssembler& masm, std::optional<u32> target, bool conditional) {
    a64::Label skip, hook;
    if (target) {
        if (*target != SYSMEM_ENTRY && *target != IOPBOOT_ENTRY) return;
        if (conditional) masm.Cbz(TAKEN, &skip);
        masm.Mov(a64::w0, *target);
    } else {
        masm.Cmp(TARGET, SYSMEM_ENTRY);
        masm.B(a64::eq, &hook);
        masm.Cmp(TARGET, IOPBOOT_ENTRY);
        masm.B(a64::ne, &skip);
        masm.Bind(&hook);
        masm.Mov(a64::w0, TARGET);
    }
    callAbsolute(masm, reinterpret_cast<const void*>(&runBranchHooks));
    masm.Bind(&skip);
}

} // namespace

bool isHookAddress(u32 pc) {
    const u32 unsegmented = pc & SEGMENT_MASK;
    return pc == IRX_INJECT_PC || unsegmented == 0xa0 || unsegmented == 0xb0 || unsegmented == 0xc0;
}

void injectIrx() {
    if (EmuConfig.CurrentIRX.length() > 3 && iopMemRead32(IRX_MODULE_COUNT_ADDRESS) == IRX_EXPECTED_MODULE_COUNT)
        iopMemWrite32(IRX_ENTRY_ADDRESS, IRX_INJECTED_ENTRY);
}

CompiledBlock compileBlock(u32 start_pc, const BlockExits& exits, u8* writable, const u8* executable) {
    std::vector<u32> words;
    u32 pc = start_pc;
    for (;;) {
        words.push_back(iopMemRead32(pc));
        pc += 4;
        const u32 code = words.back();
        if (isBranch(code) || raisesException(code) || words.size() >= MAX_BLOCK_INSTRUCTIONS ||
            (pc & PAGE_MASK) == 0 || pc == IRX_INJECT_PC)
            break;
    }
    const u32 end_pc = pc;
    const u32 last = words.back();
    const u32 body = static_cast<u32>(words.size());
    const bool ends_in_branch = isBranch(last);
    // A slot on the same page is part of the block, so a write to it clears the block.
    const u32 slot = ends_in_branch ? iopMemRead32(end_pc) : 0;

    a64::MacroAssembler masm(writable, MAX_BLOCK_BYTES, a64::PositionIndependentCode);
    const a64::MemOperand pc_field(STATE, offsetIn(&psxRegs.pc));
    const a64::MemOperand cycle_field(STATE, offsetIn(&psxRegs.cycle));
    const a64::MemOperand ee_cycles_field(STATE, offsetIn(&psxRegs.iopCycleEE));

    // Accounts the block as intExecuteBlock does, in upstream's x86 order: IOP cycles, then the EE time they took.
    // Then it leaves once that time is spent, goes to the event test once the cycle count reaches the next event, or
    // goes to the next block: through the dispatcher when its address is only known at run time or has a hook, or
    // through a direct B that starts out aimed at the dispatcher and that the runtime relinks to the block.
    std::vector<LinkSite> links;
    a64::Label event, leave;
    const auto emitExit = [&](u32 cycles, std::optional<u32> next_pc) {
        a64::Label ps2_mode, accounted;
        masm.Ldr(a64::x0, cycle_field);
        masm.Add(a64::x0, a64::x0, cycles);
        masm.Str(a64::x0, cycle_field);
        masm.Mov(a64::x1, reinterpret_cast<uintptr_t>(&psxHu32(HW_ICFG)));
        masm.Ldr(a64::w1, a64::MemOperand(a64::x1));
        masm.Tbz(a64::w1, PS1_MODE_BIT, &ps2_mode);
        masm.Mov(a64::w0, cycles);
        callAbsolute(masm, reinterpret_cast<const void*>(&subtractPs1Cycles));
        masm.Ldr(a64::x0, cycle_field);
        masm.Ldr(a64::w1, ee_cycles_field);
        masm.B(&accounted);
        masm.Bind(&ps2_mode);
        masm.Ldr(a64::w1, ee_cycles_field);
        masm.Sub(a64::w1, a64::w1, cycles * EE_CYCLES_PER_IOP_CYCLE);
        masm.Str(a64::w1, ee_cycles_field);
        masm.Bind(&accounted);
        masm.Cmp(a64::w1, 0);
        masm.B(a64::le, &leave);
        masm.Ldr(a64::x1, a64::MemOperand(STATE, offsetIn(&psxRegs.iopNextEventCycle)));
        masm.Cmp(a64::x0, a64::x1);
        masm.B(a64::hs, &event);
        if (!next_pc || isHookAddress(*next_pc)) return jumpAbsolute(masm, exits.dispatch);
        vixl::ExactAssemblyScope scope(&masm, a64::kInstructionSize);
        const u8* site = executable + masm.GetCursorOffset();
        masm.dc32(*encodeBranch(reinterpret_cast<uptr>(site), reinterpret_cast<uptr>(exits.dispatch)));
        links.push_back({*next_pc, site});
    };

    pc = start_pc;
    for (const u32 code : words) {
        pc += 4;
        if (isBranch(code)) break;
        // Only SYSCALL and BREAK read the PC (for EPC), and they end the block.
        if (raisesException(code)) storeWord(masm, pc, pc_field);
        emitInstruction(masm, code);
    }

    if (!ends_in_branch) {
        if (raisesException(last)) {
            emitExit(body, std::nullopt);
        } else {
            storeWord(masm, end_pc, pc_field);
            emitExit(body, end_pc);
        }
    } else {
        const u32 branch_pc = end_pc - 4;
        const std::optional<u32> target = staticTarget(last, branch_pc);
        const u32 opcode = opcodeOf(last);
        const bool conditional = opcode != J && opcode != JAL && !isJumpRegister(last);
        // A slot that can move the PC, is a branch itself, or lies on the next page (which may be another RAM mirror,
        // where a write would not clear this block) runs as the interpreter runs it, from the word in memory.
        const bool inline_slot = !isBranch(slot) && !raisesException(slot) && (end_pc & PAGE_MASK) != 0;
        const bool may_import = opcode == J && (slot >> 16 == IRX_IMPORT_SLOT || !inline_slot);
        a64::Label import_ran, not_taken;
        emitDecision(masm, last, branch_pc);
        if (may_import) {
            masm.Mov(a64::w0, end_pc);
            callAbsolute(masm, reinterpret_cast<const void*>(&runImportHle));
            masm.Cbnz(a64::w0, &import_ran);
        }
        emitBranchHooks(masm, target, conditional);
        const auto emitSlot = [&] {
            if (end_pc == IRX_INJECT_PC) callAbsolute(masm, reinterpret_cast<const void*>(&injectIrx));
            emitInstruction(masm, slot);
        };
        if (inline_slot) emitSlot();
        if (conditional) masm.Cbz(TAKEN, &not_taken);
        if (!inline_slot) {
            storeWord(masm, end_pc, pc_field);
            callAbsolute(masm, reinterpret_cast<const void*>(&runDelaySlot));
        }
        if (target)
            storeWord(masm, *target, pc_field);
        else
            masm.Str(TARGET, pc_field);
        emitExit(body + 1, target);
        if (conditional) {
            // Not taken, the slot is an ordinary instruction; one that could not run inline starts the next block.
            masm.Bind(&not_taken);
            const u32 next_pc = inline_slot ? end_pc + 4 : end_pc;
            storeWord(masm, next_pc, pc_field);
            emitExit(inline_slot ? body + 1 : body, next_pc);
        }
        if (may_import) {
            masm.Bind(&import_ran);
            emitExit(body, std::nullopt);
        }
    }

    masm.Bind(&leave);
    jumpAbsolute(masm, exits.leave);
    masm.Bind(&event);
    jumpAbsolute(masm, exits.event_test);
    masm.FinalizeCode();

    return {executable, masm.GetSizeOfCodeGenerated(), body + (ends_in_branch ? 1 : 0), std::move(links)};
}

} // namespace bax::jit::iop
