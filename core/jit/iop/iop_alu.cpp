// SPDX-License-Identifier: GPL-3.0-or-later
// Native integer instructions for the IOP recompiler, with the interpreter's semantics (R3000AOpcodeTables.cpp): 32-bit
// registers, r0 never written, and ADD, ADDI and SUB without the overflow exception, as the interpreter runs them.

#include "iop/iop_alu.h"

#include "R3000A.h"

namespace bax::jit::iop {
namespace {

namespace a64 = vixl::aarch64;

enum : u32 { SPECIAL = 0x00, ADDI = 0x08, ADDIU = 0x09, SLTI = 0x0a, SLTIU = 0x0b, ANDI = 0x0c, ORI = 0x0d };
enum : u32 { XORI = 0x0e, LUI = 0x0f };
enum : u32 { SLL = 0x00, SRL = 0x02, SRA = 0x03, SLLV = 0x04, SRLV = 0x06, SRAV = 0x07, MFHI = 0x10, MTHI = 0x11 };
enum : u32 { MFLO = 0x12, MTLO = 0x13, MULT = 0x18, MULTU = 0x19, ADD = 0x20, ADDU = 0x21, SUB = 0x22, SUBU = 0x23 };
enum : u32 { AND = 0x24, OR = 0x25, XOR = 0x26, NOR = 0x27, SLT = 0x2a, SLTU = 0x2b };
constexpr u32 HI = 32, LO = 33; // GPR.r indices of HI and LO.
constexpr u32 UPPER_SHIFT = 32;

class Emitter {
public:
    Emitter(a64::MacroAssembler& masm, const a64::Register& state) : masm_(masm), state_(state) {}

    a64::MemOperand gpr(u32 index) const {
        return a64::MemOperand(state_, reinterpret_cast<const u8*>(&psxRegs.GPR.r[index]) -
                                           reinterpret_cast<const u8*>(&psxRegs));
    }
    void load(const a64::Register& to, u32 index) { masm_.Ldr(to.W(), gpr(index)); }
    void store(u32 index) { masm_.Str(a64::w0, gpr(index)); }
    void storeCondition(a64::Condition condition, u32 index) {
        masm_.Cset(a64::w0, condition);
        store(index);
    }

    a64::MacroAssembler& masm() { return masm_; }

private:
    a64::MacroAssembler& masm_;
    const a64::Register& state_;
};

bool handlesImmediate(u32 opcode) {
    return opcode >= ADDI && opcode <= LUI;
}

bool handlesRegister(u32 function) {
    switch (function) {
    case SLL:
    case SRL:
    case SRA:
    case SLLV:
    case SRLV:
    case SRAV:
    case MFHI:
    case MTHI:
    case MFLO:
    case MTLO:
    case MULT:
    case MULTU:
    case ADD:
    case ADDU:
    case SUB:
    case SUBU:
    case AND:
    case OR:
    case XOR:
    case NOR:
    case SLT:
    case SLTU:
        return true;
    default:
        return false;
    }
}

void emitImmediate(Emitter& out, u32 opcode, u32 rs, u32 rt, u32 code) {
    a64::MacroAssembler& masm = out.masm();
    const s32 signed_immediate = static_cast<s16>(code & 0xffff);
    const u32 unsigned_immediate = code & 0xffff;
    if (opcode == LUI) {
        masm.Mov(a64::w0, unsigned_immediate << 16);
        return out.store(rt);
    }
    out.load(a64::w0, rs);
    switch (opcode) {
    case ADDI:
    case ADDIU:
        masm.Add(a64::w0, a64::w0, signed_immediate);
        break;
    case SLTI:
    case SLTIU:
        masm.Cmp(a64::w0, signed_immediate);
        return out.storeCondition(opcode == SLTI ? a64::lt : a64::lo, rt);
    case ANDI:
        masm.And(a64::w0, a64::w0, unsigned_immediate);
        break;
    case ORI:
        masm.Orr(a64::w0, a64::w0, unsigned_immediate);
        break;
    default:
        masm.Eor(a64::w0, a64::w0, unsigned_immediate);
        break;
    }
    out.store(rt);
}

// MULT and MULTU write the 64-bit product to LO (low half) and HI (high half).
void emitMultiply(Emitter& out, u32 function, u32 rs, u32 rt) {
    a64::MacroAssembler& masm = out.masm();
    out.load(a64::w1, rs);
    out.load(a64::w2, rt);
    function == MULT ? masm.Smull(a64::x0, a64::w1, a64::w2) : masm.Umull(a64::x0, a64::w1, a64::w2);
    out.store(LO);
    masm.Lsr(a64::x0, a64::x0, UPPER_SHIFT);
    out.store(HI);
}

void emitRegister(Emitter& out, u32 function, u32 rs, u32 rt, u32 rd, u32 sa) {
    a64::MacroAssembler& masm = out.masm();
    switch (function) {
    case MTHI:
    case MTLO:
        out.load(a64::w0, rs);
        return out.store(function == MTHI ? HI : LO);
    case MULT:
    case MULTU:
        return emitMultiply(out, function, rs, rt);
    default:
        break;
    }
    if (rd == 0) return;
    switch (function) {
    case MFHI:
    case MFLO:
        out.load(a64::w0, function == MFHI ? HI : LO);
        return out.store(rd);
    case SLL:
    case SRL:
    case SRA:
        out.load(a64::w0, rt);
        if (function == SLL) masm.Lsl(a64::w0, a64::w0, sa);
        if (function == SRL) masm.Lsr(a64::w0, a64::w0, sa);
        if (function == SRA) masm.Asr(a64::w0, a64::w0, sa);
        return out.store(rd);
    default:
        break;
    }
    out.load(a64::w1, rs);
    out.load(a64::w2, rt);
    switch (function) {
    // A shift by a register uses its low 5 bits, as the interpreter's shift does on AArch64.
    case SLLV:
        masm.Lsl(a64::w0, a64::w2, a64::w1);
        break;
    case SRLV:
        masm.Lsr(a64::w0, a64::w2, a64::w1);
        break;
    case SRAV:
        masm.Asr(a64::w0, a64::w2, a64::w1);
        break;
    case ADD:
    case ADDU:
        masm.Add(a64::w0, a64::w1, a64::w2);
        break;
    case SUB:
    case SUBU:
        masm.Sub(a64::w0, a64::w1, a64::w2);
        break;
    case AND:
        masm.And(a64::w0, a64::w1, a64::w2);
        break;
    case OR:
        masm.Orr(a64::w0, a64::w1, a64::w2);
        break;
    case XOR:
        masm.Eor(a64::w0, a64::w1, a64::w2);
        break;
    case NOR:
        masm.Orr(a64::w0, a64::w1, a64::w2);
        masm.Mvn(a64::w0, a64::w0);
        break;
    case SLT:
    case SLTU:
        masm.Cmp(a64::w1, a64::w2);
        return out.storeCondition(function == SLT ? a64::lt : a64::lo, rd);
    default:
        break;
    }
    out.store(rd);
}

} // namespace

bool emitNativeAlu(a64::MacroAssembler& masm, const a64::Register& state, u32 code) {
    const u32 opcode = code >> 26;
    const u32 function = code & 0x3f;
    if (opcode == SPECIAL ? !handlesRegister(function) : !handlesImmediate(opcode)) return false;
    Emitter out(masm, state);
    const u32 rs = (code >> 21) & 0x1f, rt = (code >> 16) & 0x1f, rd = (code >> 11) & 0x1f;
    if (opcode != SPECIAL) {
        if (rt != 0) emitImmediate(out, opcode, rs, rt, code);
        return true;
    }
    emitRegister(out, function, rs, rt, rd, (code >> 6) & 0x1f);
    return true;
}

} // namespace bax::jit::iop
