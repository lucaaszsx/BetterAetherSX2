// SPDX-License-Identifier: GPL-3.0-or-later
// Differential tester for the EE: runs random instruction sequences on two engines from the same starting state and
// compares the full CPU state they leave. Until the recompiler exists both engines are the interpreter, which checks
// the harness itself: any divergence there is a harness bug.
//
//   bax_difftest <class> <cases> [seed]
//
// A failure prints the seed and case index, the program and every field that differs, so it can be replayed.

#include "CDVD/CDVDcommon.h"
#include "DebugTools/Debug.h"
#include "Memory.h"
#include "R5900.h"
#include "ee_cases.h"
#include "ee_state.h"
#include "fmt/format.h"

#include <charconv>
#include <cstdlib>
#include <string>

namespace {

using namespace bax::difftest;

// Programs run from the start of EE RAM's user area; the case's instructions are followed by nothing the engines
// reach, because they stop at the end address.
constexpr u32 PROGRAM_ADDRESS = 0x00100000;
// Far more steps than any program in a case, so a runaway engine fails instead of hanging.
constexpr u32 MAX_STEPS = 1024;

struct Engine {
    const char* name;
    // Runs from cpuRegs.pc until it reaches end_pc. False when it did not get there.
    bool (*run)(u32 end_pc);
};

bool runInterpreter(u32 end_pc) {
    for (u32 step = 0; step < MAX_STEPS; ++step) {
        if (cpuRegs.pc == end_pc) return true;
        intCpu.Step();
    }
    return false;
}

constexpr Engine ORACLE = {"interpreter", runInterpreter};
constexpr Engine CANDIDATE = {"interpreter", runInterpreter};

bool initializeCore() {
    if (!SysMemory::Allocate()) return false;
    memSetExtraMemMode(false);
    memBindConditionalHandlers();
    SysMemory::Reset();
    // cpuReset resets the CDVD drive too, which reads the disc through the current source; there must be one.
    CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
    cpuReset();
    return true;
}

EeState startState(const EeCase& test_case) {
    for (std::size_t index = 0; index < test_case.program.size(); ++index)
        memWrite32(PROGRAM_ADDRESS + static_cast<u32>(index * 4), test_case.program[index]);
    EeState state = captureEeState();
    for (int index = 0; index < 32; ++index) state.cpu.GPR.r[index].UQ = test_case.gprs[index];
    state.cpu.HI.UQ = test_case.hi;
    state.cpu.LO.UQ = test_case.lo;
    state.cpu.pc = PROGRAM_ADDRESS;
    return state;
}

std::string disassemble(const EeCase& test_case) {
    std::string listing;
    for (std::size_t index = 0; index < test_case.program.size(); ++index) {
        const u32 pc = PROGRAM_ADDRESS + static_cast<u32>(index * 4);
        std::string line;
        R5900::disR5900Fasm(line, test_case.program[index], pc, false);
        listing += fmt::format("  {:08x}: {:08x}  {}\n", pc, test_case.program[index], line);
    }
    return listing;
}

// Runs one engine from the start state and returns what it left, or nothing when it never reached the end.
std::optional<EeState> run(const Engine& engine, const EeState& start, u32 end_pc) {
    restoreEeState(start);
    if (!engine.run(end_pc)) return std::nullopt;
    return captureEeState();
}

bool parseNumber(const char* text, u64& value) {
    const char* end = text + std::char_traits<char>::length(text);
    return std::from_chars(text, end, value).ec == std::errc{};
}

} // namespace

int main(int argc, char** argv) {
    u64 cases = 0;
    u64 seed = 1;
    const std::optional<OpClass> op_class = argc >= 3 ? parseOpClass(argv[1]) : std::nullopt;
    if (!op_class || !parseNumber(argv[2], cases) || (argc >= 4 && !parseNumber(argv[3], seed))) {
        fmt::print(stderr, "Usage: {} <alu|shift> <cases> [seed]\n", argv[0]);
        return 2;
    }
    if (!initializeCore()) {
        fmt::print(stderr, "Could not allocate the emulated memory.\n");
        return 1;
    }

    for (u64 index = 0; index < cases; ++index) {
        std::mt19937_64 random(seed + index);
        const EeCase test_case = makeCase(*op_class, random);
        const EeState start = startState(test_case);
        const u32 end_pc = PROGRAM_ADDRESS + static_cast<u32>(test_case.program.size() * 4);
        const std::optional<EeState> expected = run(ORACLE, start, end_pc);
        const std::optional<EeState> actual = run(CANDIDATE, start, end_pc);
        const std::vector<std::string> diffs =
            expected && actual ? diffEeState(*expected, *actual) : std::vector<std::string>{"an engine ran away"};
        if (diffs.empty()) continue;

        fmt::print("{} diverged from {} in case {} (seed {}):\n{}", CANDIDATE.name, ORACLE.name, index, seed,
                   disassemble(test_case));
        for (const std::string& diff : diffs) fmt::print("  {}\n", diff);
        return 1;
    }
    fmt::print("{}: {} cases, 0 divergences (seed {})\n", argv[1], cases, seed);
    return 0;
}
