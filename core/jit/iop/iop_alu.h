// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

namespace bax::jit::iop {

// Emits `code` as native code if it is one of the IOP's integer instructions: ADDI, ADDIU, SLTI, SLTIU, ANDI, ORI,
// XORI, LUI, the shifts, ADD, ADDU, SUB, SUBU, AND, OR, XOR, NOR, SLT, SLTU, MULT, MULTU, MFHI, MFLO, MTHI and MTLO.
// Returns false, emitting nothing, for any other instruction. `state` holds &psxRegs; x0-x2 are scratch.
bool emitNativeAlu(vixl::aarch64::MacroAssembler& masm, const vixl::aarch64::Register& state, u32 code);

} // namespace bax::jit::iop
