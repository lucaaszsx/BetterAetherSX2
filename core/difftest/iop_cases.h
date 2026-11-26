// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"

#include <array>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

namespace bax::difftest {

// IOP programs run from this IOP RAM address and end in a branch to itself, since the IOP interpreter runs a budget of
// cycles rather than single steps. The memory class loads and stores inside the data area, on its own pages.
inline constexpr u32 IOP_PROGRAM_ADDRESS = 0x00100000;
inline constexpr u32 IOP_DATA_ADDRESS = 0x00180000;
inline constexpr u32 IOP_DATA_BYTES = 4096;

// The IOP's instruction families, all MIPS I.
enum class IopClass { Alu, Shift, Branch, Memory };

std::optional<IopClass> parseIopClass(std::string_view name);

// A random IOP program and the registers it starts from: 32 GPRs, then HI and LO, as psxRegs.GPR.r orders them.
struct IopCase {
    std::vector<u32> program;
    std::array<u32, 34> gprs;
    std::vector<u8> data;
};

IopCase makeIopCase(IopClass iop_class, std::mt19937_64& random);

} // namespace bax::difftest
