// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "R5900.h"

#include <cstddef>

namespace bax::jit::ee {

// Where GPR `index` sits inside cpuRegs, for addressing it from the register that holds &cpuRegs. cpuRegisters is a
// struct of unions, so the offset is taken from the live object.
inline std::ptrdiff_t gprOffset(u32 index) {
    return reinterpret_cast<const u8*>(&cpuRegs.GPR.r[index]) - reinterpret_cast<const u8*>(&cpuRegs);
}

} // namespace bax::jit::ee
