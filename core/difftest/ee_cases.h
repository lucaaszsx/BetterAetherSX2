// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"

#include <array>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

namespace bax::difftest {

// Programs run from the start of EE RAM's user area. Jumps encode it, so the generator needs it too.
inline constexpr u32 PROGRAM_ADDRESS = 0x00100000;

// A family of EE instructions tested together. Each recompiler PR adds the classes it implements.
enum class OpClass { Alu, Shift, Branch };

std::optional<OpClass> parseOpClass(std::string_view name);

// A random program from one class, and the register values it starts from.
struct EeCase {
    std::vector<u32> program;
    std::array<u128, 32> gprs;
    u128 hi;
    u128 lo;
};

EeCase makeCase(OpClass op_class, std::mt19937_64& random);

// How far cpuRegs.cycle may differ between the engines for a class. The interpreter adds up cycles at taken branches
// and the recompiler at block ends; each addition rounds and counts at least one cycle, so the totals can differ by
// up to one per addition.
u64 cycleTolerance(OpClass op_class, const EeCase& test_case);

} // namespace bax::difftest
