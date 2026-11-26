// SPDX-License-Identifier: GPL-3.0-or-later
#include "ee_state.h"

#include "fmt/format.h"

#include <cstring>

namespace bax::difftest {

namespace {

std::string hex128(const GPR_reg& value) {
    return fmt::format("{:016x}_{:016x}", value.UD[1], value.UD[0]);
}

void compare128(std::vector<std::string>& diffs, const std::string& name, const GPR_reg& expected,
                const GPR_reg& actual) {
    if (std::memcmp(&expected, &actual, sizeof(GPR_reg)) != 0)
        diffs.push_back(fmt::format("{}: expected {}, got {}", name, hex128(expected), hex128(actual)));
}

template <typename T> void compare(std::vector<std::string>& diffs, const std::string& name, T expected, T actual) {
    if (expected != actual) diffs.push_back(fmt::format("{}: expected {:#x}, got {:#x}", name, expected, actual));
}

} // namespace

EeState captureEeState() {
    return {cpuRegs, fpuRegs};
}

void restoreEeState(const EeState& state) {
    cpuRegs = state.cpu;
    fpuRegs = state.fpu;
}

std::vector<std::string> diffEeState(const EeState& expected, const EeState& actual, u64 cycle_tolerance) {
    std::vector<std::string> diffs;
    for (int index = 0; index < 32; ++index)
        compare128(diffs, fmt::format("GPR[{}]", index), expected.cpu.GPR.r[index], actual.cpu.GPR.r[index]);
    compare128(diffs, "HI", expected.cpu.HI, actual.cpu.HI);
    compare128(diffs, "LO", expected.cpu.LO, actual.cpu.LO);
    for (int index = 0; index < 32; ++index)
        compare(diffs, fmt::format("CP0[{}]", index), expected.cpu.CP0.r[index], actual.cpu.CP0.r[index]);
    compare(diffs, "sa", expected.cpu.sa, actual.cpu.sa);
    compare(diffs, "pc", expected.cpu.pc, actual.cpu.pc);
    compare(diffs, "IsDelaySlot", expected.cpu.IsDelaySlot, actual.cpu.IsDelaySlot);
    const u64 cycle_gap = expected.cpu.cycle > actual.cpu.cycle ? expected.cpu.cycle - actual.cpu.cycle
                                                                : actual.cpu.cycle - expected.cpu.cycle;
    if (cycle_gap > cycle_tolerance) compare(diffs, "cycle", expected.cpu.cycle, actual.cpu.cycle);
    for (int index = 0; index < 32; ++index) {
        compare(diffs, fmt::format("FPR[{}]", index), expected.fpu.fpr[index].UL, actual.fpu.fpr[index].UL);
        compare(diffs, fmt::format("FCR[{}]", index), expected.fpu.fprc[index], actual.fpu.fprc[index]);
    }
    compare(diffs, "ACC", expected.fpu.ACC.UL, actual.fpu.ACC.UL);
    compare(diffs, "ACCflag", expected.fpu.ACCflag, actual.fpu.ACCflag);
    return diffs;
}

} // namespace bax::difftest
