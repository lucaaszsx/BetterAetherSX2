// SPDX-License-Identifier: GPL-3.0-or-later
// Native integer instructions for the EE recompiler, with the interpreter's semantics (R5900OpcodeImpl.cpp): 32-bit
// results sign-extended into the low 64 bits of the GPR, 64-bit operations on those bits, and r0 never written.

#include "ee/ee_alu.h"

#include "R5900.h"

namespace bax::jit::ee {
namespace {

namespace a64 = vixl::aarch64;

enum : u32 { SPECIAL = 0x00, ADDIU = 0x09, SLTI = 0x0a, SLTIU = 0x0b, ANDI = 0x0c, ORI = 0x0d, XORI = 0x0e };
enum : u32 { LUI = 0x0f, DADDIU = 0x19 };
enum : u32 { SLL = 0x00, SRL = 0x02, SRA = 0x03, SLLV = 0x04, SRLV = 0x06, SRAV = 0x07 };
enum : u32 { DSLLV = 0x14, DSRLV = 0x16, DSRAV = 0x17, ADDU = 0x21, SUBU = 0x23, AND = 0x24, OR = 0x25 };
enum : u32 { XOR = 0x26, NOR = 0x27, SLT = 0x2a, SLTU = 0x2b, DADDU = 0x2d, DSUBU = 0x2f, DSLL = 0x38 };
enum : u32 { DSRL = 0x3a, DSRA = 0x3b, DSLL32 = 0x3c, DSRL32 = 0x3e, DSRA32 = 0x3f };
constexpr u32 UPPER_SHIFT = 32;

class Emitter {
public:
    Emitter(a64::MacroAssembler& masm, const a64::Register& state) : masm_(masm), state_(state) {}

    a64::MemOperand gpr(u32 index) const {
        return a64::MemOperand(state_, reinterpret_cast<const u8*>(&cpuRegs.GPR.r[index]) -
                                           reinterpret_cast<const u8*>(&cpuRegs));
    }
    void load64(const a64::Register& to, u32 index) { masm_.Ldr(to.X(), gpr(index)); }
    void load32(const a64::Register& to, u32 index) { masm_.Ldr(to.W(), gpr(index)); }
    void store64(u32 index) { masm_.Str(a64::x0, gpr(index)); }
    // Stores w0 sign-extended, as the interpreter's u64(s64(s32(...))).
    void store32(u32 index) {
        masm_.Sxtw(a64::x0, a64::w0);
        store64(index);
    }
    void storeCondition(a64::Condition condition, u32 index) {
        masm_.Cset(a64::x0, condition);
        store64(index);
    }

    a64::MacroAssembler& masm() { return masm_; }

private:
    a64::MacroAssembler& masm_;
    const a64::Register& state_;
};

bool handles(u32 code) {
    const u32 opcode = code >> 26;
    if (opcode != SPECIAL) return opcode == ADDIU || opcode == DADDIU || (opcode >= SLTI && opcode <= LUI);
    switch (code & 0x3f) {
    case SLL:
    case SRL:
    case SRA:
    case SLLV:
    case SRLV:
    case SRAV:
    case DSLLV:
    case DSRLV:
    case DSRAV:
    case ADDU:
    case SUBU:
    case AND:
    case OR:
    case XOR:
    case NOR:
    case SLT:
    case SLTU:
    case DADDU:
    case DSUBU:
    case DSLL:
    case DSRL:
    case DSRA:
    case DSLL32:
    case DSRL32:
    case DSRA32:
        return true;
    default:
        return false;
    }
}

bool emitImmediate(Emitter& out, u32 opcode, u32 rs, u32 rt, u32 code) {
    a64::MacroAssembler& masm = out.masm();
    const s64 signed_immediate = static_cast<s16>(code & 0xffff);
    const u64 unsigned_immediate = code & 0xffff;
    switch (opcode) {
    case ADDIU:
        out.load32(a64::w0, rs);
        masm.Add(a64::w0, a64::w0, static_cast<s32>(signed_immediate));
        out.store32(rt);
        return true;
    case DADDIU:
        out.load64(a64::x0, rs);
        masm.Add(a64::x0, a64::x0, signed_immediate);
        out.store64(rt);
        return true;
    case SLTI:
    case SLTIU:
        out.load64(a64::x0, rs);
        masm.Cmp(a64::x0, signed_immediate);
        out.storeCondition(opcode == SLTI ? a64::lt : a64::lo, rt);
        return true;
    case ANDI:
    case ORI:
    case XORI:
        out.load64(a64::x0, rs);
        if (opcode == ANDI) masm.And(a64::x0, a64::x0, unsigned_immediate);
        if (opcode == ORI) masm.Orr(a64::x0, a64::x0, unsigned_immediate);
        if (opcode == XORI) masm.Eor(a64::x0, a64::x0, unsigned_immediate);
        out.store64(rt);
        return true;
    case LUI:
        masm.Mov(a64::x0, static_cast<s64>(static_cast<s32>(unsigned_immediate << 16)));
        out.store64(rt);
        return true;
    default:
        return false;
    }
}

bool emitShift(Emitter& out, u32 function, u32 rs, u32 rt, u32 rd, u32 sa) {
    a64::MacroAssembler& masm = out.masm();
    switch (function) {
    case SLL:
    case SRL:
    case SRA:
        out.load32(a64::w0, rt);
        if (function == SLL) masm.Lsl(a64::w0, a64::w0, sa);
        if (function == SRL) masm.Lsr(a64::w0, a64::w0, sa);
        if (function == SRA) masm.Asr(a64::w0, a64::w0, sa);
        out.store32(rd);
        return true;
    case SLLV:
    case SRLV:
    case SRAV:
        // A shift by a register uses its low 5 bits, as the interpreter's & 0x1f does.
        out.load32(a64::w0, rt);
        out.load32(a64::w1, rs);
        if (function == SLLV) masm.Lsl(a64::w0, a64::w0, a64::w1);
        if (function == SRLV) masm.Lsr(a64::w0, a64::w0, a64::w1);
        if (function == SRAV) masm.Asr(a64::w0, a64::w0, a64::w1);
        out.store32(rd);
        return true;
    case DSLLV:
    case DSRLV:
    case DSRAV:
        // The 64-bit forms use the low 6 bits, as & 0x3f does.
        out.load64(a64::x0, rt);
        out.load64(a64::x1, rs);
        if (function == DSLLV) masm.Lsl(a64::x0, a64::x0, a64::x1);
        if (function == DSRLV) masm.Lsr(a64::x0, a64::x0, a64::x1);
        if (function == DSRAV) masm.Asr(a64::x0, a64::x0, a64::x1);
        out.store64(rd);
        return true;
    case DSLL:
    case DSRL:
    case DSRA:
    case DSLL32:
    case DSRL32:
    case DSRA32: {
        const u32 amount = sa + (function >= DSLL32 ? UPPER_SHIFT : 0);
        out.load64(a64::x0, rt);
        if (function == DSLL || function == DSLL32) masm.Lsl(a64::x0, a64::x0, amount);
        if (function == DSRL || function == DSRL32) masm.Lsr(a64::x0, a64::x0, amount);
        if (function == DSRA || function == DSRA32) masm.Asr(a64::x0, a64::x0, amount);
        out.store64(rd);
        return true;
    }
    default:
        return false;
    }
}

bool emitRegister(Emitter& out, u32 function, u32 rs, u32 rt, u32 rd) {
    a64::MacroAssembler& masm = out.masm();
    switch (function) {
    case ADDU:
    case SUBU:
        out.load32(a64::w0, rs);
        out.load32(a64::w1, rt);
        function == ADDU ? masm.Add(a64::w0, a64::w0, a64::w1) : masm.Sub(a64::w0, a64::w0, a64::w1);
        out.store32(rd);
        return true;
    case DADDU:
    case DSUBU:
    case AND:
    case OR:
    case XOR:
    case NOR:
        out.load64(a64::x0, rs);
        out.load64(a64::x1, rt);
        if (function == DADDU) masm.Add(a64::x0, a64::x0, a64::x1);
        if (function == DSUBU) masm.Sub(a64::x0, a64::x0, a64::x1);
        if (function == AND) masm.And(a64::x0, a64::x0, a64::x1);
        if (function == OR) masm.Orr(a64::x0, a64::x0, a64::x1);
        if (function == XOR) masm.Eor(a64::x0, a64::x0, a64::x1);
        if (function == NOR) {
            masm.Orr(a64::x0, a64::x0, a64::x1);
            masm.Mvn(a64::x0, a64::x0);
        }
        out.store64(rd);
        return true;
    case SLT:
    case SLTU:
        out.load64(a64::x0, rs);
        out.load64(a64::x1, rt);
        masm.Cmp(a64::x0, a64::x1);
        out.storeCondition(function == SLT ? a64::lt : a64::lo, rd);
        return true;
    default:
        return false;
    }
}

} // namespace

bool emitNativeAlu(a64::MacroAssembler& masm, const a64::Register& state, u32 code) {
    const u32 opcode = code >> 26;
    const u32 rs = (code >> 21) & 0x1f;
    const u32 rt = (code >> 16) & 0x1f;
    const u32 rd = (code >> 11) & 0x1f;
    const u32 sa = (code >> 6) & 0x1f;
    // The interpreter returns at once when the destination is r0, so the block needs nothing at all.
    if (!handles(code)) return false;
    if ((opcode == SPECIAL ? rd : rt) == 0) return true;
    Emitter out(masm, state);
    if (opcode != SPECIAL) return emitImmediate(out, opcode, rs, rt, code);
    const u32 function = code & 0x3f;
    return emitShift(out, function, rs, rt, rd, sa) || emitRegister(out, function, rs, rt, rd);
}

} // namespace bax::jit::ee
