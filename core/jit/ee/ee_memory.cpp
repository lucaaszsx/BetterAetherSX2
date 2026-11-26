// SPDX-License-Identifier: GPL-3.0-or-later
// Native loads and stores for the EE recompiler: the common case, an aligned access to a RAM page, without a call.

#include "ee/ee_memory.h"

#include "R5900.h"
#include "ee/ee_registers.h"
#include "vtlb.h"

#include <array>
#include <optional>

namespace bax::jit::ee {
namespace {

namespace a64 = vixl::aarch64;

struct Access {
    u32 opcode;
    u32 bytes;
    bool store;
    bool sign_extend;      // Loads into a GPR: sign-extend to 64 bits rather than zero-extend.
    bool fpu;              // LWC1 and SWC1 move an FPU register.
    bool forced_alignment; // LQ and SQ drop the address's low bits instead of raising an address error.
    bool writes_zero;      // The interpreter's LD writes even r0; the other loads leave it alone.
};

// The interpreter's semantics for each, from R5900OpcodeImpl.cpp and FPU.cpp.
constexpr std::array<Access, 15> ACCESSES = {{
    {0x20, 1, false, true, false, false, false},  // LB
    {0x24, 1, false, false, false, false, false}, // LBU
    {0x21, 2, false, true, false, false, false},  // LH
    {0x25, 2, false, false, false, false, false}, // LHU
    {0x23, 4, false, true, false, false, false},  // LW
    {0x27, 4, false, false, false, false, false}, // LWU
    {0x37, 8, false, false, false, false, true},  // LD
    {0x1e, 16, false, false, false, true, false}, // LQ
    {0x31, 4, false, false, true, false, false},  // LWC1
    {0x28, 1, true, false, false, false, false},  // SB
    {0x29, 2, true, false, false, false, false},  // SH
    {0x2b, 4, true, false, false, false, false},  // SW
    {0x3f, 8, true, false, false, false, false},  // SD
    {0x1f, 16, true, false, false, true, false},  // SQ
    {0x39, 4, true, false, true, false, false},   // SWC1
}};

constexpr unsigned PAGE_BITS = 12;

std::optional<Access> accessFor(u32 code) {
    for (const Access& access : ACCESSES)
        if (access.opcode == code >> 26) return access;
    return std::nullopt;
}

// Moves a value of `bytes` between the host address in x1 and the register at `field` (relative to `base`).
void emitLoad(a64::MacroAssembler& masm, const Access& access, const a64::MemOperand& field) {
    const a64::MemOperand host(a64::x1);
    switch (access.bytes) {
    case 1:
        access.sign_extend ? masm.Ldrsb(a64::x3, host) : masm.Ldrb(a64::w3, host);
        break;
    case 2:
        access.sign_extend ? masm.Ldrsh(a64::x3, host) : masm.Ldrh(a64::w3, host);
        break;
    case 4:
        access.sign_extend ? masm.Ldrsw(a64::x3, host) : masm.Ldr(a64::w3, host);
        break;
    case 8:
        masm.Ldr(a64::x3, host);
        break;
    default:
        masm.Ldr(a64::q0, host);
        return masm.Str(a64::q0, field);
    }
    access.fpu ? masm.Str(a64::w3, field) : masm.Str(a64::x3, field);
}

void emitStore(a64::MacroAssembler& masm, const Access& access, const a64::MemOperand& field) {
    const a64::MemOperand host(a64::x1);
    switch (access.bytes) {
    case 1:
        masm.Ldr(a64::w3, field);
        return masm.Strb(a64::w3, host);
    case 2:
        masm.Ldr(a64::w3, field);
        return masm.Strh(a64::w3, host);
    case 4:
        masm.Ldr(a64::w3, field);
        return masm.Str(a64::w3, host);
    case 8:
        masm.Ldr(a64::x3, field);
        return masm.Str(a64::x3, host);
    default:
        masm.Ldr(a64::q0, field);
        return masm.Str(a64::q0, host);
    }
}

} // namespace

bool isFastMemoryAccess(u32 code) {
    return accessFor(code).has_value();
}

void emitMemoryAccess(a64::MacroAssembler& masm, const a64::Register& state, u32 code,
                      const std::function<void()>& emit_slow_path) {
    const Access access = *accessFor(code);
    const u32 rs = (code >> 21) & 0x1f;
    const u32 rt = (code >> 16) & 0x1f;
    const s32 offset = static_cast<s16>(code & 0xffff);
    a64::Label slow, done;

    // The address, as the interpreter computes it: the low 32 bits of rs plus the sign-extended offset.
    masm.Ldr(a64::w0, a64::MemOperand(state, gprOffset(rs)));
    if (offset != 0) masm.Add(a64::w0, a64::w0, offset);
    if (access.forced_alignment) {
        masm.And(a64::w0, a64::w0, ~(access.bytes - 1));
    } else if (access.bytes > 1) {
        masm.Tst(a64::w0, access.bytes - 1);
        masm.B(a64::ne, &slow);
    }
    // vtlb_memRead's page lookup: the entry plus the address is a host pointer, or negative for a handler.
    masm.Mov(a64::x1, reinterpret_cast<uintptr_t>(&vtlb_private::vtlbdata.vmap));
    masm.Ldr(a64::x1, a64::MemOperand(a64::x1));
    masm.Lsr(a64::w2, a64::w0, PAGE_BITS);
    masm.Ldr(a64::x1, a64::MemOperand(a64::x1, a64::x2, a64::LSL, 3));
    masm.Adds(a64::x1, a64::x1, a64::x0);
    masm.B(a64::mi, &slow);

    if (access.fpu) {
        masm.Mov(a64::x4, reinterpret_cast<uintptr_t>(&fpuRegs.fpr[rt]));
        const a64::MemOperand field(a64::x4);
        access.store ? emitStore(masm, access, field) : emitLoad(masm, access, field);
    } else if (access.store) {
        emitStore(masm, access, a64::MemOperand(state, gprOffset(rt)));
    } else if (rt != 0 || access.writes_zero) {
        emitLoad(masm, access, a64::MemOperand(state, gprOffset(rt)));
    }
    masm.B(&done);

    masm.Bind(&slow);
    emit_slow_path();
    masm.Bind(&done);
}

} // namespace bax::jit::ee
