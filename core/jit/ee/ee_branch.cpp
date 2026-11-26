// SPDX-License-Identifier: GPL-3.0-or-later
// Branches and jumps for recompiled EE blocks. The interpreter's own branch functions cannot be called from a block:
// they run the delay slot, update the interpreter's cycle count and run its event test themselves. This file makes
// the same decisions, then runs the delay slot the way the interpreter's _doBranch_shared does.

#include "ee/ee_branch.h"

#include "Config.h"
#include "Memory.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"

namespace R5900::Interpreter::OpcodeImpl::COP0 {
int CPCOND0(); // COP0.cpp: the DMAC condition BC0F/BC0T test.
}

namespace bax::jit::ee {
namespace {

// The FPU condition bit in FCR31, as FPU.cpp defines FPUflagC.
constexpr u32 FPU_CONDITION_FLAG = 0x00800000;

enum class Decision { Taken, NotTaken, SkipDelaySlot };

struct Outcome {
    Decision decision;
    u32 target;
};

// Primary opcodes (bits 31-26), REGIMM rt values and SPECIAL functions that transfer control.
enum : u32 { SPECIAL = 0, REGIMM = 1, J = 2, JAL = 3, BEQ = 4, BNE = 5, BLEZ = 6, BGTZ = 7, COP0 = 16, COP1 = 17 };
enum : u32 { BEQL = 20, BNEL = 21, BLEZL = 22, BGTZL = 23 };
enum : u32 { JR = 8, JALR = 9 };
// rs value of BC0x and BC1x inside the COP0 and COP1 opcode spaces.
constexpr u32 COP_BRANCH = 8;
// REGIMM rt values: bit 1 means likely, bit 4 means link; 0 and 1 are BLTZ and BGEZ.
constexpr u32 REGIMM_LIKELY = 0x02;
constexpr u32 REGIMM_LINK = 0x10;
constexpr u32 REGIMM_GREATER_EQUAL = 0x01;
constexpr u32 REGIMM_KNOWN = 0x13;

Outcome conditional(bool taken, bool likely) {
    if (taken) return {Decision::Taken, _BranchTarget_};
    return {likely ? Decision::SkipDelaySlot : Decision::NotTaken, 0};
}

s64 rs() {
    return cpuRegs.GPR.r[_Rs_].SD[0];
}
s64 rt() {
    return cpuRegs.GPR.r[_Rt_].SD[0];
}

// Reads cpuRegs.code with cpuRegs.pc already past the branch, as the interpreter does, and writes any link register.
// The Goemon addresses are the ones upstream's interpreter checks for that game's TLB fix.
Outcome decide() {
    const u32 code = cpuRegs.code;
    switch (code >> 26) {
    case J:
        return {Decision::Taken, _JumpTarget_};
    case JAL:
        if (EmuConfig.Gamefixes.GoemonTlbHack && _JumpTarget_ == 0x3563b8) GoemonUnloadTlb(cpuRegs.GPR.n.a0.UL[0]);
        _SetLink(31);
        return {Decision::Taken, _JumpTarget_};
    case BEQ:
    case BEQL:
        return conditional(rs() == rt(), (code >> 26) == BEQL);
    case BNE:
    case BNEL:
        return conditional(rs() != rt(), (code >> 26) == BNEL);
    case BLEZ:
    case BLEZL:
        return conditional(rs() <= 0, (code >> 26) == BLEZL);
    case BGTZ:
    case BGTZL:
        return conditional(rs() > 0, (code >> 26) == BGTZL);
    case REGIMM: {
        const u32 kind = _Rt_;
        if (kind & REGIMM_LINK) _SetLink(31);
        const bool taken = (kind & REGIMM_GREATER_EQUAL) ? rs() >= 0 : rs() < 0;
        return conditional(taken, kind & REGIMM_LIKELY);
    }
    case COP0:
        return conditional(R5900::Interpreter::OpcodeImpl::COP0::CPCOND0() == static_cast<int>(_Rt_ & 1),
                           _Rt_ & REGIMM_LIKELY);
    case COP1:
        return conditional(((fpuRegs.fprc[31] & FPU_CONDITION_FLAG) != 0) == static_cast<bool>(_Rt_ & 1),
                           _Rt_ & REGIMM_LIKELY);
    default: {
        // JR and JALR read the target before JALR writes its link, which may be the same register.
        const u32 target = cpuRegs.GPR.r[_Rs_].UL[0];
        if (EmuConfig.Gamefixes.GoemonTlbHack && _Funct_ == JR && (target == 0x33ad48 || target == 0x35060c))
            GoemonPreloadTlb();
        if (_Funct_ == JALR && _Rd_) _SetLink(_Rd_);
        return {Decision::Taken, target};
    }
    }
}

} // namespace

bool isBranch(u32 code) {
    const u32 opcode = code >> 26;
    const u32 rs_field = (code >> 21) & 0x1f;
    const u32 rt_field = (code >> 16) & 0x1f;
    switch (opcode) {
    case SPECIAL:
        return (code & 0x3f) == JR || (code & 0x3f) == JALR;
    case REGIMM:
        return (rt_field & ~REGIMM_KNOWN) == 0;
    case J:
    case JAL:
    case BEQ:
    case BNE:
    case BLEZ:
    case BGTZ:
    case BEQL:
    case BNEL:
    case BLEZL:
    case BGTZL:
        return true;
    case COP0:
    case COP1:
        return rs_field == COP_BRANCH && rt_field < 4;
    default:
        return false;
    }
}

u64 decideBranch() {
    const Outcome outcome = decide();
    switch (outcome.decision) {
    case Decision::Taken:
        return BRANCH_TAKEN | outcome.target;
    case Decision::SkipDelaySlot:
        return BRANCH_SKIPS_SLOT;
    case Decision::NotTaken:
        return 0;
    }
    return 0;
}

std::optional<u32> staticTarget(u32 code, u32 pc) {
    switch (code >> 26) {
    case J:
    case JAL:
        return ((pc + 4) & 0xf0000000u) | ((code & 0x03ffffffu) << 2);
    case SPECIAL:
        return std::nullopt;
    default:
        return pc + 4 + (static_cast<u32>(static_cast<s32>(static_cast<s16>(code & 0xffff))) << 2);
    }
}

u32 runBranch() {
    const Outcome outcome = decide();
    if (outcome.decision == Decision::SkipDelaySlot) {
        cpuRegs.pc += 4;
        return 0;
    }
    const bool taken = outcome.decision == Decision::Taken;
    // A taken branch marks its delay slot so that an exception there records the branch delay; the exception
    // handler clears the mark, and the PC it set stands.
    if (taken) cpuRegs.branch = 1;
    // As in execI: the PC moves past the slot before the fetch, which an exception during the fetch relies on.
    const u32 slot_pc = cpuRegs.pc;
    cpuRegs.pc += 4;
    cpuRegs.code = memRead32(slot_pc);
    const R5900::OPCODE& slot = R5900::GetCurrentInstruction();
    slot.interpret();
    if (taken && cpuRegs.branch != 0) {
        cpuRegs.branch = 0;
        cpuRegs.pc = outcome.target;
    }
    return slot.cycles;
}

} // namespace bax::jit::ee
