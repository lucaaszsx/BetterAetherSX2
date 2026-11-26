// SPDX-License-Identifier: GPL-3.0-or-later
// Native IOP loads and stores to RAM, with the interpreter's semantics (iopMemRead*, iopMemWrite*): no alignment
// check, as the interpreter has none, and RAM mirrored through the first 8 MB of each segment.

#include "iop/iop_memory.h"

#include "IopMem.h"
#include "R3000A.h"

namespace bax::jit::iop {
namespace {

namespace a64 = vixl::aarch64;

enum : u32 { LB = 0x20, LH = 0x21, LW = 0x23, LBU = 0x24, LHU = 0x25, SB = 0x28, SH = 0x29, SW = 0x2b };
// Addresses whose bits 23-28 are clear lie in the RAM mirrors at the start of a segment.
constexpr u32 OUTSIDE_RAM_MIRRORS = 0x1f800000;
// CP0 Status bit 16 isolates the cache: stores no longer reach RAM.
constexpr unsigned ISOLATE_CACHE_BIT = 16;
constexpr unsigned CODE_PAGE_BITS = 12;

std::ptrdiff_t offsetIn(const void* field) {
    return static_cast<const u8*>(field) - reinterpret_cast<const u8*>(&psxRegs);
}

void emitLoad(a64::MacroAssembler& masm, u32 opcode, const a64::MemOperand& host) {
    switch (opcode) {
    case LB:
        masm.Ldrsb(a64::w2, host);
        break;
    case LBU:
        masm.Ldrb(a64::w2, host);
        break;
    case LH:
        masm.Ldrsh(a64::w2, host);
        break;
    case LHU:
        masm.Ldrh(a64::w2, host);
        break;
    default:
        masm.Ldr(a64::w2, host);
        break;
    }
}

void emitStore(a64::MacroAssembler& masm, u32 opcode, const a64::MemOperand& host) {
    switch (opcode) {
    case SB:
        masm.Strb(a64::w2, host);
        break;
    case SH:
        masm.Strh(a64::w2, host);
        break;
    default:
        masm.Str(a64::w2, host);
        break;
    }
}

bool isStore(u32 opcode) {
    return opcode == SB || opcode == SH || opcode == SW;
}

} // namespace

bool isFastMemoryAccess(u32 code) {
    const u32 opcode = code >> 26;
    return opcode == LB || opcode == LBU || opcode == LH || opcode == LHU || opcode == LW || isStore(opcode);
}

void emitMemoryAccess(a64::MacroAssembler& masm, const a64::Register& state, u32 code, const u8* code_pages,
                      const std::function<void()>& emit_slow_path) {
    const u32 opcode = code >> 26;
    const u32 rs = (code >> 21) & 0x1f, rt = (code >> 16) & 0x1f;
    const auto gpr = [&](u32 index) { return a64::MemOperand(state, offsetIn(&psxRegs.GPR.r[index])); };
    a64::Label slow, done;
    masm.Ldr(a64::w0, gpr(rs));
    masm.Add(a64::w0, a64::w0, static_cast<s32>(static_cast<s16>(code & 0xffff)));
    masm.Tst(a64::w0, OUTSIDE_RAM_MIRRORS);
    masm.B(a64::ne, &slow);
    if (isStore(opcode)) {
        masm.Ldr(a64::w1, a64::MemOperand(state, offsetIn(&psxRegs.CP0.n.Status)));
        masm.Tbnz(a64::w1, ISOLATE_CACHE_BIT, &slow);
    }
    masm.And(a64::w0, a64::w0, Ps2MemSize::ExposedIopRam - 1);
    if (isStore(opcode)) {
        masm.Mov(a64::x1, reinterpret_cast<uintptr_t>(code_pages));
        masm.Lsr(a64::w2, a64::w0, CODE_PAGE_BITS);
        masm.Ldrb(a64::w2, a64::MemOperand(a64::x1, a64::x2));
        masm.Cbnz(a64::w2, &slow);
        masm.Ldr(a64::w2, gpr(rt));
    }
    masm.Mov(a64::x1, reinterpret_cast<uintptr_t>(iopMem->Main));
    const a64::MemOperand host(a64::x1, a64::x0);
    if (isStore(opcode)) {
        emitStore(masm, opcode, host);
    } else {
        emitLoad(masm, opcode, host);
        if (rt != 0) masm.Str(a64::w2, gpr(rt));
    }
    masm.B(&done);
    masm.Bind(&slow);
    emit_slow_path();
    masm.Bind(&done);
}

} // namespace bax::jit::iop
