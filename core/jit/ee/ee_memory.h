// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

#include <functional>

namespace bax::jit::ee {

// True for the loads and stores emitMemoryAccess handles: LB LBU LH LHU LW LWU LD LQ LWC1 and SB SH SW SD SQ SWC1.
bool isFastMemoryAccess(u32 code);

// Emits `code` with a fast path for RAM: the address from cpuRegs, the page through vtlbdata.vmap, then a direct
// access, as vtlb_memRead and vtlb_memWrite do for a page that holds a pointer. A misaligned address or a hardware
// page takes `emit_slow_path` instead, which must run the interpreter's function for the instruction: that function
// raises the address error, calls the hardware handler, and runs the event test forced on counter reads. `state` holds
// &cpuRegs; x0-x4 and q0 are used as scratch.
void emitMemoryAccess(vixl::aarch64::MacroAssembler& masm, const vixl::aarch64::Register& state, u32 code,
                      const std::function<void()>& emit_slow_path);

} // namespace bax::jit::ee
