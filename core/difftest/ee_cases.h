// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"

#include <array>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

namespace bax::difftest {

// A family of EE instructions tested together. Each recompiler PR adds the classes it implements.
enum class OpClass { Alu, Shift };

std::optional<OpClass> parseOpClass(std::string_view name);

// A random program from one class, and the register values it starts from.
struct EeCase {
    std::vector<u32> program;
    std::array<u128, 32> gprs;
    u128 hi;
    u128 lo;
};

EeCase makeCase(OpClass op_class, std::mt19937_64& random);

} // namespace bax::difftest
