// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

#include <functional>

namespace bax::jit::iop {

// True for the loads and stores emitMemoryAccess handles: LB, LBU, LH, LHU, LW, SB, SH and SW.
bool isFastMemoryAccess(u32 code);

// Emits `code` with a fast path for IOP RAM, as iopMemRead and iopMemWrite reach it through psxMemRLUT and psxMemWLUT:
// the address without its segment, folded onto the first RAM mirror, then a direct access. Any other address takes
// `emit_slow_path`, which must run the interpreter's function for the instruction. Stores also take it with cache
// isolation on (Status bit 16), and when `code_pages` (one byte per 4 KB page of IOP RAM) says their page holds
// compiled code, so the write clears those blocks. `state` holds &psxRegs; x0-x2 are scratch.
void emitMemoryAccess(vixl::aarch64::MacroAssembler& masm, const vixl::aarch64::Register& state, u32 code,
                      const u8* code_pages, const std::function<void()>& emit_slow_path);

} // namespace bax::jit::iop
