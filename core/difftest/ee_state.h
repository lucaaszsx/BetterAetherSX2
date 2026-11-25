// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "pcsx2/R5900.h"

#include <string>
#include <vector>

namespace bax::difftest {

// The EE state one engine leaves behind, compared field by field against another engine's.
struct EeState {
    cpuRegisters cpu;
    fpuRegisters fpu;
};

EeState captureEeState();
void restoreEeState(const EeState& state);

// One line per field that differs, naming the field and both values. Empty when the states match.
std::vector<std::string> diffEeState(const EeState& expected, const EeState& actual);

} // namespace bax::difftest
