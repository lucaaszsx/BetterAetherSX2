// SPDX-License-Identifier: GPL-3.0-or-later
// The IOP side of the differential tester. The IOP interpreter has no single-step entry, so each engine runs the
// case's program for a fixed budget of EE cycles; the program ends in a branch to itself, where the budget runs out.

#include "iop_harness.h"

#include "DebugTools/Debug.h"
#include "IopHw.h"
#include "IopMem.h"
#include "R3000A.h"
#include "fmt/format.h"

#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace bax::difftest {
namespace {

// Far more than any program needs to reach its final loop: 4096 EE cycles are 512 IOP cycles.
constexpr s32 EE_CYCLE_BUDGET = 4096;
constexpr std::size_t GPR_COUNT = 34; // 32 GPRs, then HI and LO.

struct IopState {
    std::array<u32, GPR_COUNT> gprs;
    std::vector<u8> data;
};

u8* iopRam(u32 address) {
    return &iopMem->Main[address & (Ps2MemSize::ExposedIopRam - 1)];
}

void writeProgram(const IopCase& test_case) {
    std::memcpy(iopRam(IOP_PROGRAM_ADDRESS), test_case.program.data(), test_case.program.size() * sizeof(u32));
    psxCpu->Clear(IOP_PROGRAM_ADDRESS, static_cast<u32>(test_case.program.size()));
}

// Runs one engine from the case's start. Counter events, interrupts and PS1 mode stay off, so the event test the
// interpreter runs at each branch changes nothing the comparison sees.
IopState run(R3000Acpu& engine, const IopCase& test_case) {
    std::memcpy(psxRegs.GPR.r, test_case.gprs.data(), sizeof(u32) * GPR_COUNT);
    std::memcpy(iopRam(IOP_DATA_ADDRESS), test_case.data.data(), IOP_DATA_BYTES);
    psxRegs.pc = IOP_PROGRAM_ADDRESS;
    psxRegs.interrupt = 0;
    psxHu32(HW_ICTRL) = 0;
    psxHu32(HW_ICFG) = 0;
    psxNextStartCounter = psxRegs.cycle;
    psxNextDeltaCounter = std::numeric_limits<s32>::max();
    engine.ExecuteBlock(EE_CYCLE_BUDGET);
    IopState state;
    std::memcpy(state.gprs.data(), psxRegs.GPR.r, sizeof(u32) * GPR_COUNT);
    const u8* data = iopRam(IOP_DATA_ADDRESS);
    state.data.assign(data, data + IOP_DATA_BYTES);
    return state;
}

std::vector<std::string> diff(const IopState& expected, const IopState& actual) {
    std::vector<std::string> diffs;
    for (std::size_t index = 0; index < GPR_COUNT; ++index)
        if (expected.gprs[index] != actual.gprs[index])
            diffs.push_back(fmt::format("GPR[{}]: expected {:#010x}, got {:#010x}", index, expected.gprs[index],
                                        actual.gprs[index]));
    for (u32 offset = 0; offset < IOP_DATA_BYTES; ++offset)
        if (expected.data[offset] != actual.data[offset])
            diffs.push_back(fmt::format("data[{:#x}]: expected {:#04x}, got {:#04x}", IOP_DATA_ADDRESS + offset,
                                        expected.data[offset], actual.data[offset]));
    return diffs;
}

std::string disassemble(const IopCase& test_case) {
    std::string listing;
    for (std::size_t index = 0; index < test_case.program.size(); ++index) {
        const u32 pc = IOP_PROGRAM_ADDRESS + static_cast<u32>(index * 4);
        listing += fmt::format("  {:08x}: {:08x}  {}\n", pc, test_case.program[index],
                               R3000A::disR3000AF(test_case.program[index], pc));
    }
    return listing;
}

} // namespace

int runIopCases(const char* class_name, IopClass iop_class, u64 cases, u64 seed, bool mutate) {
    R3000Acpu& oracle = psxInt;
    // Until the IOP recompiler joins, the candidate is the interpreter too, which checks the harness itself.
    R3000Acpu& candidate = psxInt;
    for (u64 index = 0; index < cases; ++index) {
        std::mt19937_64 random(seed + index);
        const IopCase test_case = makeIopCase(iop_class, random);
        writeProgram(test_case);
        const IopState expected = run(oracle, test_case);
        IopState actual = run(candidate, test_case);
        if (mutate) actual.gprs[1] ^= 1;
        const std::vector<std::string> diffs = diff(expected, actual);
        if (diffs.empty()) continue;
        fmt::print("IOP candidate diverged from the interpreter in case {} (seed {}):\n{}", index, seed,
                   disassemble(test_case));
        for (const std::string& line : diffs) fmt::print("  {}\n", line);
        return 1;
    }
    fmt::print("{}: {} cases, 0 divergences (seed {})\n", class_name, cases, seed);
    return 0;
}

} // namespace bax::difftest
