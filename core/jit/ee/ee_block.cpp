// SPDX-License-Identifier: GPL-3.0-or-later
// Block compiler for the first tier of the AArch64 EE recompiler: decode once, then call the interpreter for each
// instruction. Calls are absolute (address in a register, then BLR), so they do not depend on which view of the code
// buffer the bytes were written through.

#include "ee/ee_block.h"

#include "Config.h"
#include "Memory.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "ee/ee_branch.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

#include <vector>

namespace bax::jit::ee {
namespace {

namespace a64 = vixl::aarch64;

// Kept in callee-saved registers for the whole block, so the interpreter calls leave them alone.
const a64::Register& STATE = a64::x19;  // &cpuRegs
const a64::Register& CYCLES = a64::w20; // Raw cycles of the instructions run so far.
// Holds call targets. Not one of VIXL's scratch registers (x16, x17), which its macros may use in between.
const a64::Register& CALL_TARGET = a64::x9;

constexpr u32 PAGE_MASK = 0xfff;
// Stack bytes the prologue saves: frame pointer and link register, then the two block registers.
constexpr s32 PROLOGUE_FRAME = 32;

// cpuRegisters is a struct of unions; its field offsets are taken from the live object.
std::ptrdiff_t offsetIn(const void* field) {
    return static_cast<const u8*>(field) - reinterpret_cast<const u8*>(&cpuRegs);
}

// The interpreter's fractional cycle count, carried from block to block.
u32 s_block_cycles = 0;

void callAbsolute(a64::MacroAssembler& masm, const void* function) {
    masm.Mov(CALL_TARGET, reinterpret_cast<uintptr_t>(function));
    masm.Blr(CALL_TARGET);
}

void storeWord(a64::MacroAssembler& masm, u32 value, std::ptrdiff_t offset) {
    masm.Mov(a64::w0, value);
    masm.Str(a64::w0, a64::MemOperand(STATE, offset));
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

CompiledBlock compileBlock(u32 start_pc, u32 stop_pc, const u32* code_to_check, u8* writable, const u8* executable) {
    // First the block's instructions, so a check of all of them can come before any runs. A branch is the last word
    // read; its delay slot is fetched when it runs.
    std::vector<u32> words;
    for (u32 pc = start_pc;;) {
        words.push_back(memRead32(pc));
        pc += 4;
        if (isBranch(words.back()) || words.size() >= MAX_BLOCK_INSTRUCTIONS || pc == stop_pc || (pc & PAGE_MASK) == 0)
            break;
    }

    a64::MacroAssembler masm(writable, MAX_BLOCK_BYTES, a64::PositionIndependentCode);
    const std::ptrdiff_t pc_offset = offsetIn(&cpuRegs.pc);
    const std::ptrdiff_t code_offset = offsetIn(&cpuRegs.code);

    masm.Stp(a64::x29, a64::x30, a64::MemOperand(a64::sp, -PROLOGUE_FRAME, a64::PreIndex));
    masm.Stp(STATE, CYCLES.X(), a64::MemOperand(a64::sp, 16));

    a64::Label stale;
    if (code_to_check) {
        masm.Mov(a64::x1, reinterpret_cast<uintptr_t>(code_to_check));
        for (std::size_t index = 0; index < words.size(); ++index) {
            masm.Ldr(a64::w0, a64::MemOperand(a64::x1, static_cast<s64>(index * 4)));
            masm.Cmp(a64::w0, words[index]);
            masm.B(a64::ne, &stale);
        }
    }

    masm.Mov(STATE, reinterpret_cast<uintptr_t>(&cpuRegs));
    masm.Mov(CYCLES, 0);
    a64::Label leave;
    u32 pc = start_pc;
    for (const u32 code : words) {
        const R5900::OPCODE& opcode = R5900::GetInstruction(code);
        // The interpreter sees the PC already past the instruction, and reads its operands from cpuRegs.code.
        storeWord(masm, pc + 4, pc_offset);
        storeWord(masm, code, code_offset);
        masm.Add(CYCLES, CYCLES, opcode.cycles);
        pc += 4;
        if (isBranch(code)) {
            callAbsolute(masm, reinterpret_cast<const void*>(&runBranch));
            masm.Add(CYCLES, CYCLES, a64::w0);
            break;
        }
        callAbsolute(masm, reinterpret_cast<const void*>(opcode.interpret));
        // Exceptions, SYSCALL and ERET move the PC elsewhere, and the rest of the block no longer applies.
        masm.Ldr(a64::w0, a64::MemOperand(STATE, pc_offset));
        masm.Cmp(a64::w0, pc);
        masm.B(a64::ne, &leave);
    }

    a64::Label restore;
    masm.Bind(&leave);
    masm.Mov(a64::w0, CYCLES);
    callAbsolute(masm, reinterpret_cast<const void*>(&commitCycles));
    masm.Bind(&restore);
    masm.Ldp(STATE, CYCLES.X(), a64::MemOperand(a64::sp, 16));
    masm.Ldp(a64::x29, a64::x30, a64::MemOperand(a64::sp, PROLOGUE_FRAME, a64::PostIndex));
    masm.Ret();

    if (code_to_check) {
        // Memory no longer holds what this block was compiled from: drop it, and return with the PC still at its
        // start, so it compiles again from the current code.
        masm.Bind(&stale);
        masm.Mov(a64::w0, start_pc);
        masm.Mov(a64::w1, static_cast<u32>(words.size()));
        callAbsolute(masm, reinterpret_cast<const void*>(&discardBlock));
        masm.B(&restore);
    }
    masm.FinalizeCode();

    const bool ends_in_branch = isBranch(words.back());
    return {executable, masm.GetSizeOfCodeGenerated(), static_cast<u32>(words.size()) + (ends_in_branch ? 1 : 0)};
}

} // namespace bax::jit::ee
