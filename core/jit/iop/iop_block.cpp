// SPDX-License-Identifier: GPL-3.0-or-later
// Block compiler for the IOP recompiler's first tier: decode once, then call the interpreter's function for each
// instruction. Calls are absolute, so they do not depend on which view of the code buffer the bytes went through.

#include "iop/iop_block.h"

#include "IopMem.h"
#include "R3000A.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

namespace bax::jit::iop {
namespace {

namespace a64 = vixl::aarch64;

const a64::Register& STATE = a64::x19; // &psxRegs, callee-saved so the interpreter calls leave it alone.
const a64::Register& CALL_TARGET = a64::x9;
constexpr u32 PAGE_MASK = 0xfff;
constexpr s32 FRAME_BYTES = 32;

std::ptrdiff_t offsetIn(const void* field) {
    return static_cast<const u8*>(field) - reinterpret_cast<const u8*>(&psxRegs);
}

// MIPS I jumps and branches: J, JAL, BEQ, BNE, BLEZ, BGTZ, the REGIMM branches, and JR, JALR.
bool isBranch(u32 code) {
    constexpr u32 SPECIAL = 0, REGIMM = 1, J = 2, BGTZ = 7, JR = 8, JALR = 9;
    const u32 opcode = code >> 26;
    if (opcode == SPECIAL) return (code & 0x3f) == JR || (code & 0x3f) == JALR;
    return opcode == REGIMM || (opcode >= J && opcode <= BGTZ);
}

} // namespace

CompiledBlock compileBlock(u32 start_pc, u8* writable, const u8* executable) {
    a64::MacroAssembler masm(writable, MAX_BLOCK_BYTES, a64::PositionIndependentCode);
    const std::ptrdiff_t pc_offset = offsetIn(&psxRegs.pc);
    const std::ptrdiff_t cycle_offset = offsetIn(&psxRegs.cycle);

    masm.Stp(a64::x29, a64::x30, a64::MemOperand(a64::sp, -FRAME_BYTES, a64::PreIndex));
    masm.Str(STATE, a64::MemOperand(a64::sp, 16));
    masm.Mov(STATE, reinterpret_cast<uintptr_t>(&psxRegs));

    a64::Label leave;
    u32 pc = start_pc;
    u32 instructions = 0;
    for (;;) {
        const u32 code = iopMemRead32(pc);
        pc += 4;
        ++instructions;
        // As execI: the PC past the instruction, its word, one more cycle, then the instruction.
        masm.Mov(a64::w0, pc);
        masm.Str(a64::w0, a64::MemOperand(STATE, pc_offset));
        masm.Mov(a64::w0, code);
        masm.Str(a64::w0, a64::MemOperand(STATE, offsetIn(&psxRegs.code)));
        masm.Ldr(a64::x0, a64::MemOperand(STATE, cycle_offset));
        masm.Add(a64::x0, a64::x0, 1);
        masm.Str(a64::x0, a64::MemOperand(STATE, cycle_offset));
        masm.Mov(CALL_TARGET, reinterpret_cast<uintptr_t>(psxBSC[code >> 26]));
        masm.Blr(CALL_TARGET);
        if (isBranch(code)) break;
        // An exception moves the PC elsewhere, and the rest of the block no longer applies.
        masm.Ldr(a64::w0, a64::MemOperand(STATE, pc_offset));
        masm.Cmp(a64::w0, pc);
        masm.B(a64::ne, &leave);
        if (instructions >= MAX_BLOCK_INSTRUCTIONS || (pc & PAGE_MASK) == 0 || pc == IRX_INJECT_PC) break;
    }

    masm.Bind(&leave);
    masm.Ldr(STATE, a64::MemOperand(a64::sp, 16));
    masm.Ldp(a64::x29, a64::x30, a64::MemOperand(a64::sp, FRAME_BYTES, a64::PostIndex));
    masm.Ret();
    masm.FinalizeCode();
    return {executable, masm.GetSizeOfCodeGenerated(), instructions};
}

} // namespace bax::jit::iop
