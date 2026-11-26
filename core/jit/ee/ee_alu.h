// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

namespace bax::jit::ee {

// Emits `code` as native code if it is one of the integer instructions that cannot raise an exception: ADDIU,
// DADDIU, SLTI, SLTIU, ANDI, ORI, XORI, LUI, ADDU, SUBU, DADDU, DSUBU, AND, OR, XOR, NOR, SLT, SLTU and the 32- and
// 64-bit shifts. Returns false, emitting nothing, for any other instruction. `state` holds &cpuRegs; x0-x2 are scratch.
bool emitNativeAlu(vixl::aarch64::MacroAssembler& masm, const vixl::aarch64::Register& state, u32 code);

} // namespace bax::jit::ee
