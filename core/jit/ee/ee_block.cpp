// SPDX-License-Identifier: GPL-3.0-or-later
// Block compiler for the first tier of the AArch64 EE recompiler: decode once, then call the interpreter for each
// instruction. Calls are absolute (address in a register, then BLR), so they do not depend on which view of the code
// buffer the bytes were written through.

#include "ee/ee_block.h"

#include "Config.h"
#include "Memory.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "bax/jit/a64_branch.h"
#include "ee/ee_branch.h"
#include "ee/ee_memory.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

#include <optional>
#include <utility>
#include <vector>

namespace bax::jit::ee {
namespace {

namespace a64 = vixl::aarch64;

// Callee-saved, so the interpreter calls leave them alone. The runtime's entry code sets STATE and saves both.
const a64::Register& STATE = a64::x19;  // &cpuRegs
const a64::Register& CYCLES = a64::w20; // Raw cycles of the instructions run so far.
// Holds call and jump targets. Not one of VIXL's scratch registers (x16, x17), which its macros may use in between.
const a64::Register& CALL_TARGET = a64::x9;

constexpr u32 PAGE_MASK = 0xfff;

// cpuRegisters is a struct of unions; its field offsets are taken from the live object.
std::ptrdiff_t offsetIn(const void* field) {
    return static_cast<const u8*>(field) - reinterpret_cast<const u8*>(&cpuRegs);
}

// The interpreter's fractional cycle count, carried from block to block.
u32 s_block_cycles = 0;

// Bit 18 of CP0 Config: when clear, each instruction costs twice its listed cycles, as execI applies it.
constexpr unsigned CONFIG_HALF_CYCLES_BIT = 18;
// intUpdateCPUCycles keeps cycles as 3-bit fixed point, carrying the fraction to the next update.
constexpr unsigned CYCLE_FRACTION_BITS = 3;

void callAbsolute(a64::MacroAssembler& masm, const void* function) {
    masm.Mov(CALL_TARGET, reinterpret_cast<uintptr_t>(function));
    masm.Blr(CALL_TARGET);
}

void jumpAbsolute(a64::MacroAssembler& masm, const void* target) {
    masm.Mov(CALL_TARGET, reinterpret_cast<uintptr_t>(target));
    masm.Br(CALL_TARGET);
}

void storeWord(a64::MacroAssembler& masm, u32 value, std::ptrdiff_t offset) {
    masm.Mov(a64::w0, value);
    masm.Str(a64::w0, a64::MemOperand(STATE, offset));
}

// Commits CYCLES to cpuRegs.cycle. At the default EE cycle rate this is intUpdateCPUCycles' arithmetic inline: the
// fixed-point total shifted down, at least one cycle, the fraction kept. Other rates call commitCycles; the runtime
// recompiles every block when the rate changes.
void emitCommitCycles(a64::MacroAssembler& masm) {
    masm.Mov(a64::w0, CYCLES);
    if (EmuConfig.Speedhacks.EECycleRate != 0) return callAbsolute(masm, reinterpret_cast<const void*>(&commitCycles));
    masm.Ldr(a64::w1, a64::MemOperand(STATE, offsetIn(&cpuRegs.CP0.n.Config)));
    masm.Ubfx(a64::w1, a64::w1, CONFIG_HALF_CYCLES_BIT, 1);
    masm.Eor(a64::w1, a64::w1, 1);
    masm.Lsl(a64::w0, a64::w0, a64::w1);
    masm.Mov(a64::x3, reinterpret_cast<uintptr_t>(&s_block_cycles));
    masm.Ldr(a64::w4, a64::MemOperand(a64::x3));
    masm.Add(a64::w4, a64::w4, a64::w0);
    masm.Lsr(a64::w5, a64::w4, CYCLE_FRACTION_BITS);
    masm.Cmp(a64::w5, 0);
    masm.Csinc(a64::w5, a64::w5, a64::wzr, a64::ne);
    masm.Ldr(a64::x6, a64::MemOperand(STATE, offsetIn(&cpuRegs.cycle)));
    masm.Add(a64::x6, a64::x6, a64::Operand(a64::w5, a64::UXTW));
    masm.Str(a64::x6, a64::MemOperand(STATE, offsetIn(&cpuRegs.cycle)));
    masm.And(a64::w4, a64::w4, (1u << CYCLE_FRACTION_BITS) - 1);
    masm.Str(a64::w4, a64::MemOperand(a64::x3));
}

} // namespace

void commitCycles(u32 raw_cycles) {
    // Bit 18 of CP0 Config halves the cost of each instruction, as execI applies it.
    s_block_cycles += raw_cycles * (2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1));
    const bool low_cycles = s_block_cycles <= 40;
    const s8 rate = EmuConfig.Speedhacks.EECycleRate;
    u32 scaled = 0;
    if (rate == 0 || low_cycles || rate < -99 || rate > 3)
        scaled = s_block_cycles >> 3;
    else if (rate > 1)
        scaled = s_block_cycles >> (2 + rate);
    else if (rate == 1)
        scaled = static_cast<u32>((s_block_cycles >> 3) / 1.3f);
    else if (rate == -1)
        scaled = (s_block_cycles <= 80 || s_block_cycles > 168 ? 5 : 7) * s_block_cycles / 32;
    else
        scaled = ((5 + (-2 * (rate + 1))) * s_block_cycles) >> 5;
    cpuRegs.cycle += scaled < 1 ? 1 : scaled;
    s_block_cycles &= rate > 1 ? (0x1u << (rate + 2)) - 1 : 0x7u;
}

CompiledBlock compileBlock(u32 start_pc, u32 stop_pc, const u32* code_to_check, const BlockExits& exits, u8* writable,
                           const u8* executable) {
    // First the block's instructions, so a check of all of them can come before any runs. A branch is the last word
    // read, then its delay slot when that lies on the same page and is not a branch itself; the block runs such a slot
    // inline, and leaves any other to runBranch, which fetches it when it runs.
    std::vector<u32> words;
    u32 pc = start_pc;
    for (;;) {
        words.push_back(memRead32(pc));
        pc += 4;
        if (isBranch(words.back()) || words.size() >= MAX_BLOCK_INSTRUCTIONS || pc == stop_pc || (pc & PAGE_MASK) == 0)
            break;
    }
    const u32 end_pc = pc;
    const bool ends_in_branch = isBranch(words.back());
    std::optional<u32> delay_slot;
    if (ends_in_branch && (end_pc & PAGE_MASK) != 0 && end_pc != stop_pc && !isBranch(memRead32(end_pc)))
        delay_slot = memRead32(end_pc);

    a64::MacroAssembler masm(writable, MAX_BLOCK_BYTES, a64::PositionIndependentCode);
    const std::ptrdiff_t pc_offset = offsetIn(&cpuRegs.pc);
    const std::ptrdiff_t code_offset = offsetIn(&cpuRegs.code);
    const std::ptrdiff_t branch_offset = offsetIn(&cpuRegs.branch);

    a64::Label stale;
    if (code_to_check) {
        std::vector<u32> checked = words;
        if (delay_slot) checked.push_back(*delay_slot);
        masm.Mov(a64::x1, reinterpret_cast<uintptr_t>(code_to_check));
        for (std::size_t index = 0; index < checked.size(); ++index) {
            masm.Ldr(a64::w0, a64::MemOperand(a64::x1, static_cast<s64>(index * 4)));
            masm.Cmp(a64::w0, checked[index]);
            masm.B(a64::ne, &stale);
        }
    }

    // Commits the block's cycles, then goes to the event test once the cycle count reaches the next event, or else to
    // the next block: through the dispatcher when its address is only known at run time, or through a direct B that
    // starts out aimed at the dispatcher and that the runtime relinks to the block at next_pc once that block exists.
    std::vector<LinkSite> links;
    a64::Label event;
    const auto emitExit = [&](std::optional<u32> next_pc) {
        emitCommitCycles(masm);
        masm.Ldr(a64::x0, a64::MemOperand(STATE, offsetIn(&cpuRegs.cycle)));
        masm.Ldr(a64::x1, a64::MemOperand(STATE, offsetIn(&cpuRegs.nextEventCycle)));
        masm.Cmp(a64::x0, a64::x1);
        masm.B(a64::hs, &event);
        if (!next_pc) return jumpAbsolute(masm, exits.dispatch);
        vixl::ExactAssemblyScope scope(&masm, a64::kInstructionSize);
        const u8* site = executable + masm.GetCursorOffset();
        masm.dc32(*encodeBranch(reinterpret_cast<uptr>(site), reinterpret_cast<uptr>(exits.dispatch)));
        links.push_back({*next_pc, site});
    };

    masm.Mov(CYCLES, 0);
    a64::Label leave;
    pc = start_pc;
    for (const u32 code : words) {
        const R5900::OPCODE& opcode = R5900::GetInstruction(code);
        masm.Add(CYCLES, CYCLES, opcode.cycles);
        const u32 next_pc = pc + 4;
        // The interpreter sees the PC already past the instruction, and reads its operands from cpuRegs.code.
        // Exceptions, SYSCALL and ERET move the PC elsewhere, and the rest of the block no longer applies.
        const auto emitInterpreterCall = [&] {
            storeWord(masm, next_pc, pc_offset);
            storeWord(masm, code, code_offset);
            callAbsolute(masm, reinterpret_cast<const void*>(opcode.interpret));
            masm.Ldr(a64::w0, a64::MemOperand(STATE, pc_offset));
            masm.Cmp(a64::w0, next_pc);
            masm.B(a64::ne, &leave);
        };
        pc = next_pc;
        if (isBranch(code)) {
            storeWord(masm, next_pc, pc_offset);
            storeWord(masm, code, code_offset);
            break;
        }
        if (isFastMemoryAccess(code)) {
            emitMemoryAccess(masm, STATE, code, emitInterpreterCall);
            continue;
        }
        emitInterpreterCall();
    }

    if (!ends_in_branch) {
        // A native load or store last in the block leaves the PC behind, and the dispatcher reads it.
        storeWord(masm, end_pc, pc_offset);
        emitExit(end_pc);
    } else if (!delay_slot) {
        callAbsolute(masm, reinterpret_cast<const void*>(&runBranch));
        masm.Add(CYCLES, CYCLES, a64::w0);
        emitExit(std::nullopt);
    } else {
        // The branch decides first, as the interpreter does, then the delay slot runs, marked for a taken branch so an
        // exception there records the branch delay (the exception handler clears the mark and its PC stands).
        const R5900::OPCODE& slot = R5900::GetInstruction(*delay_slot);
        const a64::Register& decision = a64::x22;
        a64::Label run_slot, not_taken, skip_slot, taken_exit, fallthrough_exit;
        callAbsolute(masm, reinterpret_cast<const void*>(&decideBranch));
        masm.Mov(decision, a64::x0);
        masm.Tbnz(decision, BRANCH_SKIPS_SLOT_BIT, &skip_slot);
        masm.Tbz(decision, BRANCH_TAKEN_BIT, &run_slot);
        masm.Mov(a64::w0, 1);
        masm.Str(a64::w0, a64::MemOperand(STATE, branch_offset));
        masm.Bind(&run_slot);
        storeWord(masm, end_pc + 4, pc_offset);
        storeWord(masm, *delay_slot, code_offset);
        masm.Add(CYCLES, CYCLES, slot.cycles);
        callAbsolute(masm, reinterpret_cast<const void*>(slot.interpret));
        masm.Tbz(decision, BRANCH_TAKEN_BIT, &not_taken);
        masm.Ldr(a64::w0, a64::MemOperand(STATE, branch_offset));
        masm.Cbz(a64::w0, &leave);
        masm.Str(a64::wzr, a64::MemOperand(STATE, branch_offset));
        masm.Str(decision.W(), a64::MemOperand(STATE, pc_offset));
        masm.B(&taken_exit);
        masm.Bind(&not_taken);
        masm.Ldr(a64::w0, a64::MemOperand(STATE, pc_offset));
        masm.Cmp(a64::w0, end_pc + 4);
        masm.B(a64::ne, &leave);
        masm.B(&fallthrough_exit);
        masm.Bind(&skip_slot);
        storeWord(masm, end_pc + 4, pc_offset);
        masm.Bind(&fallthrough_exit);
        emitExit(end_pc + 4);
        masm.Bind(&taken_exit);
        emitExit(staticTarget(words.back(), end_pc - 4));
    }

    masm.Bind(&leave);
    emitExit(std::nullopt);
    masm.Bind(&event);
    jumpAbsolute(masm, exits.event_test);

    if (code_to_check) {
        // Memory no longer holds what this block was compiled from: drop it, and dispatch with the PC still at its
        // start, so it compiles again from the current code.
        masm.Bind(&stale);
        masm.Mov(a64::w0, start_pc);
        masm.Mov(a64::w1, static_cast<u32>(words.size()));
        callAbsolute(masm, reinterpret_cast<const void*>(&discardBlock));
        jumpAbsolute(masm, exits.dispatch);
    }
    masm.FinalizeCode();

    return {executable, masm.GetSizeOfCodeGenerated(), static_cast<u32>(words.size()) + (ends_in_branch ? 1 : 0),
            std::move(links)};
}

} // namespace bax::jit::ee
