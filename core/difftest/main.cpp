// SPDX-License-Identifier: GPL-3.0-or-later
// Differential tester for the EE: runs random instruction sequences on two engines from the same starting state and
// compares the full CPU state they leave: the interpreter is the reference, our AArch64 recompiler the candidate.
//
//   bax_difftest <class> <cases> [seed] [--mutate]
//
// A failure prints the seed and case index, the program and every field that differs, so it can be replayed.
// --mutate flips one bit of the candidate's result in every case, which must fail: it shows the comparison catches a
// divergence.

#include "CDVD/CDVDcommon.h"
#include "Counters.h"
#include "DebugTools/Debug.h"
#include "Memory.h"
#include "R3000A.h"
#include "R5900.h"
#include "VUmicro.h"
#include "common/Console.h"
#include "ee/ee_rec.h"
#include "ee_cases.h"
#include "ee_state.h"
#include "fmt/format.h"

#include <charconv>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>

namespace {

using namespace bax::difftest;

// Far more steps than any program in a case, so a runaway engine fails instead of hanging.
constexpr u32 MAX_STEPS = 1024;
// Half the 32-bit range, as far as Compare can be from Count.
constexpr u32 COMPARE_DISTANCE = 0x80000000;

struct Engine {
    const char* name;
    // Runs from cpuRegs.pc until it reaches end_pc. False when it did not get there.
    bool (*run)(u32 end_pc);
};

bool runInterpreter(u32 end_pc) {
    for (u32 step = 0; step < MAX_STEPS; ++step) {
        if (cpuRegs.pc != end_pc) {
            intCpu.Step();
            continue;
        }
        // The interpreter adds up cycles at branches and the recompiler at the end of each block; for a program
        // without branches both happen once, at its end.
        intUpdateCPUCycles();
        return true;
    }
    return false;
}

bool runRecompiler(u32 end_pc) {
    return bax::jit::ee::runUntil(end_pc, MAX_STEPS);
}

constexpr Engine ORACLE = {"interpreter", runInterpreter};
constexpr Engine CANDIDATE = {"recompiler", runRecompiler};

bool s_mutate = false;

bool initializeCore() {
    if (!SysMemory::Allocate()) return false;
    memSetExtraMemMode(false);
    memBindConditionalHandlers();
    SysMemory::Reset();
    // cpuReset resets the CDVD drive too, which reads the disc through the current source; there must be one.
    CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
    // As VMManager selects them for an interpreter run. vtlb's page fault handler calls Cpu->Clear when a write hits a
    // page the recompiler protected, and branches run event tests that reach the IOP and the VUs.
    Cpu = &intCpu;
    psxCpu = &psxInt;
    CpuVU0 = &CpuIntVU0;
    CpuVU1 = &CpuIntVU1;
    cpuReset();
    recCpu.Reserve();
    return true;
}

EeState startState(const EeCase& test_case) {
    for (std::size_t index = 0; index < test_case.program.size(); ++index)
        memWrite32(PROGRAM_ADDRESS + static_cast<u32>(index * 4), test_case.program[index]);
    // The recompiler has no write protection on code pages yet, so it learns about the new program here.
    recCpu.Clear(PROGRAM_ADDRESS, static_cast<u32>(test_case.program.size()));
    EeState state = captureEeState();
    for (int index = 0; index < 32; ++index) state.cpu.GPR.r[index].UQ = test_case.gprs[index];
    state.cpu.HI.UQ = test_case.hi;
    state.cpu.LO.UQ = test_case.lo;
    state.cpu.pc = PROGRAM_ADDRESS;
    // Far from Count, so the COP0 timer interrupt cannot fire during a case.
    state.cpu.CP0.n.Compare = state.cpu.CP0.n.Count + COMPARE_DISTANCE;
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
    // The interpreter runs the event test at its branches and the recompiler's test entry runs none. Keep the test
    // from changing anything the comparison sees: no counter event and no IOP time falls inside a case.
    nextStartCounter = cpuRegs.cycle;
    nextDeltaCounter = std::numeric_limits<s32>::max();
    EEoCycle = cpuRegs.cycle;
    EEsCycle = 0;
    if (!engine.run(end_pc)) return std::nullopt;
    // Bring the COP0 timer up to the cycle count, as the next event test does, so both engines are compared with
    // the time they spent.
    cpuRegs.CP0.n.Count += static_cast<u32>(cpuRegs.cycle - cpuRegs.lastCOP0Cycle);
    cpuRegs.lastCOP0Cycle = cpuRegs.cycle;
    return captureEeState();
}

bool parseNumber(const char* text, u64& value) {
    const char* end = text + std::char_traits<char>::length(text);
    return std::from_chars(text, end, value).ec == std::errc{};
}

// Runs the cases and returns the process exit code: 0 when the engines agree on every case.
int runCases(const char* class_name, OpClass op_class, u64 cases, u64 seed) {
    for (u64 index = 0; index < cases; ++index) {
        std::mt19937_64 random(seed + index);
        const EeCase test_case = makeCase(op_class, random);
        const EeState start = startState(test_case);
        const u32 end_pc = PROGRAM_ADDRESS + static_cast<u32>(test_case.program.size() * 4);
        const std::optional<EeState> expected = run(ORACLE, start, end_pc);
        std::optional<EeState> actual = run(CANDIDATE, start, end_pc);
        if (s_mutate && actual) actual->cpu.GPR.r[1].UD[0] ^= 1;
        const std::vector<std::string> diffs =
            expected && actual ? diffEeState(*expected, *actual, cycleTolerance(op_class, test_case))
                               : std::vector<std::string>{"an engine ran away"};
        if (diffs.empty()) continue;

        fmt::print("{} diverged from {} in case {} (seed {}):\n{}", CANDIDATE.name, ORACLE.name, index, seed,
                   disassemble(test_case));
        for (const std::string& diff : diffs) fmt::print("  {}\n", diff);
        return 1;
    }
    fmt::print("{}: {} cases, 0 divergences (seed {})\n", class_name, cases, seed);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    u64 cases = 0;
    u64 seed = 1;
    const std::optional<OpClass> op_class = argc >= 3 ? parseOpClass(argv[1]) : std::nullopt;
    s_mutate = argc >= 5 && std::string_view(argv[4]) == "--mutate";
    if (!op_class || !parseNumber(argv[2], cases) || (argc >= 4 && !parseNumber(argv[3], seed)) ||
        (argc >= 5 && !s_mutate)) {
        fmt::print(stderr, "Usage: {} <alu|shift|branch> <cases> [seed] [--mutate]\n", argv[0]);
        return 2;
    }
    // The core's warnings and errors go to the terminal, so a failure in CI says why.
    Log::SetConsoleOutputLevel(LOGLEVEL_WARNING);
    if (!initializeCore()) {
        fmt::print(stderr, "Could not allocate the emulated memory.\n");
        return 1;
    }
    const int exit_code = runCases(argv[1], *op_class, cases, seed);
    // The memory map is a static whose destructor asserts that nothing is still mapped, and by then the assertion
    // handler's own statics are gone; release it while the process is still whole.
    SysMemory::Release();
    return exit_code;
}
