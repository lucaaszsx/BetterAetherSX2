// SPDX-License-Identifier: GPL-3.0-or-later
// Branch decisions as native code, with the interpreter's semantics (Interpreter.cpp): comparisons on the 64-bit
// registers, links written before the condition reads its register, and JR/JALR targets read before JALR links.

#include "ee/ee_branch_native.h"

#include "Config.h"
#include "ee/ee_branch.h"
#include "ee/ee_registers.h"

namespace bax::jit::ee {
namespace {

namespace a64 = vixl::aarch64;

enum : u32 { SPECIAL = 0, REGIMM = 1, J = 2, JAL = 3, BEQ = 4, BNE = 5, BLEZ = 6, BGTZ = 7 };
enum : u32 { BEQL = 20, BNEL = 21, BLEZL = 22, BGTZL = 23, JR = 8, JALR = 9 };
constexpr u32 LINK_REGISTER = 31;
constexpr u32 REGIMM_GREATER_EQUAL = 0x01;
constexpr u32 REGIMM_LIKELY = 0x02;
constexpr u32 REGIMM_LINK = 0x10;
// A branch's link is the address after its delay slot.
constexpr u32 LINK_DISTANCE = 8;

void writeLink(a64::MacroAssembler& masm, const a64::Register& state, u32 index, u32 branch_pc) {
    masm.Mov(a64::x0, branch_pc + LINK_DISTANCE);
    masm.Str(a64::x0, a64::MemOperand(state, gprOffset(index)));
}

// decision = condition ? taken : (likely ? skip the slot : run the slot without branching).
void selectDecision(a64::MacroAssembler& masm, const a64::Register& decision, a64::Condition condition, u32 target,
                    bool likely) {
    masm.Mov(a64::x1, BRANCH_TAKEN | target);
    masm.Mov(a64::x2, likely ? BRANCH_SKIPS_SLOT : 0);
    masm.Csel(decision, a64::x1, a64::x2, condition);
}

} // namespace

bool emitBranchDecision(a64::MacroAssembler& masm, const a64::Register& state, const a64::Register& decision, u32 code,
                        u32 branch_pc) {
    const u32 opcode = code >> 26;
    const u32 rs = (code >> 21) & 0x1f;
    const u32 rt = (code >> 16) & 0x1f;
    const bool goemon_fix = EmuConfig.Gamefixes.GoemonTlbHack;
    const std::optional<u32> target = staticTarget(code, branch_pc);
    switch (opcode) {
    case J:
    case JAL:
        if (opcode == JAL && goemon_fix) return false;
        if (opcode == JAL) writeLink(masm, state, LINK_REGISTER, branch_pc);
        masm.Mov(decision, BRANCH_TAKEN | *target);
        return true;
    case BEQ:
    case BNE:
    case BEQL:
    case BNEL:
        masm.Ldr(a64::x0, a64::MemOperand(state, gprOffset(rs)));
        masm.Ldr(a64::x1, a64::MemOperand(state, gprOffset(rt)));
        masm.Cmp(a64::x0, a64::x1);
        selectDecision(masm, decision, opcode == BEQ || opcode == BEQL ? a64::eq : a64::ne, *target, opcode >= BEQL);
        return true;
    case BLEZ:
    case BGTZ:
    case BLEZL:
    case BGTZL:
        masm.Ldr(a64::x0, a64::MemOperand(state, gprOffset(rs)));
        masm.Cmp(a64::x0, 0);
        selectDecision(masm, decision, opcode == BLEZ || opcode == BLEZL ? a64::le : a64::gt, *target, opcode >= BEQL);
        return true;
    case REGIMM:
        // The link comes first: BLTZAL and BGEZAL on $ra test the link they just wrote, as the interpreter does.
        if (rt & REGIMM_LINK) writeLink(masm, state, LINK_REGISTER, branch_pc);
        masm.Ldr(a64::x0, a64::MemOperand(state, gprOffset(rs)));
        masm.Cmp(a64::x0, 0);
        selectDecision(masm, decision, (rt & REGIMM_GREATER_EQUAL) ? a64::ge : a64::lt, *target,
                       (rt & REGIMM_LIKELY) != 0);
        return true;
    case SPECIAL: {
        const u32 function = code & 0x3f;
        const u32 rd = (code >> 11) & 0x1f;
        if (function == JR && goemon_fix) return false;
        // The target is read before JALR writes its link, which may be the same register.
        masm.Ldr(a64::w1, a64::MemOperand(state, gprOffset(rs)));
        if (function == JALR && rd != 0) writeLink(masm, state, rd, branch_pc);
        masm.Orr(decision, a64::x1, BRANCH_TAKEN);
        return true;
    }
    default:
        return false;
    }
}

} // namespace bax::jit::ee
