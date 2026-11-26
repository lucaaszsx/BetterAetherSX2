// SPDX-License-Identifier: GPL-3.0-or-later
#include "ee_cases.h"

#include <utility>

namespace bax::difftest {

namespace {

constexpr u32 MAX_PROGRAM_LENGTH = 8;
constexpr u32 MAX_BRANCH_PROGRAM_LENGTH = 16;

// SPECIAL-group function codes and primary opcodes, from the R5900 manual.
constexpr std::array<u32, 10> ALU_FUNCTIONS = {0x21, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2A, 0x2B, 0x2D, 0x2F};
constexpr std::array<u32, 8> ALU_IMMEDIATE_OPCODES = {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x19};
constexpr std::array<u32, 6> SHIFT_BY_REGISTER_FUNCTIONS = {0x04, 0x06, 0x07, 0x14, 0x16, 0x17};
constexpr std::array<u32, 9> SHIFT_BY_AMOUNT_FUNCTIONS = {0x00, 0x02, 0x03, 0x38, 0x3A, 0x3B, 0x3C, 0x3E, 0x3F};
// BEQ, BNE, BLEZ, BGTZ and their likely forms; REGIMM's eight branches (rt field); J and JAL; COP0 and COP1.
constexpr std::array<u32, 8> COMPARE_BRANCH_OPCODES = {0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17};
constexpr std::array<u32, 8> REGIMM_BRANCHES = {0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13};
constexpr u32 REGIMM = 0x01, J = 0x02, JAL = 0x03, COP0 = 0x10, COP1 = 0x11, COP_BRANCH = 0x08;

// Values that tend to expose sign extension, carry and overflow mistakes, mixed with plain random ones.
constexpr std::array<u64, 8> EDGE_VALUES = {
    0, 1, ~0ull, 0x7fffffffull, 0x80000000ull, 0xffffffff80000000ull, 0x7fffffffffffffffull, 0x8000000000000000ull,
};

u32 field(std::mt19937_64& random, u32 bits) {
    return static_cast<u32>(random()) & ((1u << bits) - 1);
}

template <std::size_t N> u32 pick(std::mt19937_64& random, const std::array<u32, N>& values) {
    return values[random() % N];
}

u32 special(u32 rs, u32 rt, u32 rd, u32 sa, u32 function) {
    return (rs << 21) | (rt << 16) | (rd << 11) | (sa << 6) | function;
}

u32 immediate(u32 opcode, u32 rs, u32 rt, u32 value) {
    return (opcode << 26) | (rs << 21) | (rt << 16) | (value & 0xffff);
}

u32 aluInstruction(std::mt19937_64& random) {
    const u32 rs = field(random, 5), rt = field(random, 5), rd = field(random, 5);
    if (random() % 2) return special(rs, rt, rd, 0, pick(random, ALU_FUNCTIONS));
    return immediate(pick(random, ALU_IMMEDIATE_OPCODES), rs, rt, field(random, 16));
}

u32 shiftInstruction(std::mt19937_64& random) {
    const u32 rs = field(random, 5), rt = field(random, 5), rd = field(random, 5);
    if (random() % 2) return special(rs, rt, rd, 0, pick(random, SHIFT_BY_REGISTER_FUNCTIONS));
    return special(0, rt, rd, field(random, 5), pick(random, SHIFT_BY_AMOUNT_FUNCTIONS));
}

// A branch at `position` whose target is the instruction at `target` (the program's length means its end).
u32 branchInstruction(std::mt19937_64& random, u32 position, u32 target) {
    // Branch offsets count words from the delay slot.
    const u32 offset = target - (position + 1);
    // Comparing a register with itself makes BEQ/BNE outcomes certain, so both directions get exercised.
    const u32 rs = field(random, 5);
    const u32 rt = random() % 4 == 0 ? rs : field(random, 5);
    switch (random() % 5) {
    case 0:
        return immediate(pick(random, COMPARE_BRANCH_OPCODES), rs, rt, offset);
    case 1:
        return immediate(REGIMM, rs, pick(random, REGIMM_BRANCHES), offset);
    case 2:
        return ((random() % 2 ? J : JAL) << 26) | (((PROGRAM_ADDRESS >> 2) + target) & 0x03ffffff);
    case 3:
        return immediate(COP0, COP_BRANCH, field(random, 2), offset);
    default:
        return immediate(COP1, COP_BRANCH, field(random, 2), offset);
    }
}

// ALU instructions with forward branches among them. Every target lies inside the program or at its end, and no
// branch sits in a delay slot or last, so both engines always reach the end.
std::vector<u32> branchProgram(std::mt19937_64& random) {
    const u32 length = 2 + static_cast<u32>(random() % (MAX_BRANCH_PROGRAM_LENGTH - 1));
    std::vector<u32> program(length);
    bool in_delay_slot = false;
    for (u32 position = 0; position < length; ++position) {
        const bool can_branch = !in_delay_slot && position + 1 < length;
        if (can_branch && random() % 3 == 0) {
            // From the instruction after the delay slot up to the program's end.
            const u32 target = position + 2 + static_cast<u32>(random() % (length - position - 1));
            program[position] = branchInstruction(random, position, target);
            in_delay_slot = true;
        } else {
            program[position] = aluInstruction(random);
            in_delay_slot = false;
        }
    }
    return program;
}

// Loads and stores (primary opcode, access size): LB LH LW LBU LHU LWU LD LQ LWC1, SB SH SW SD SQ SWC1, then the
// unaligned LWL LWR LDL LDR SWL SWR SDL SDR, which take any address.
constexpr std::array<std::pair<u32, u32>, 23> MEMORY_OPERATIONS = {{
    {0x20, 1}, {0x21, 2}, {0x23, 4}, {0x24, 1}, {0x25, 2}, {0x27, 4},  {0x37, 8}, {0x1e, 16},
    {0x31, 4}, {0x28, 1}, {0x29, 2}, {0x2b, 4}, {0x3f, 8}, {0x1f, 16}, {0x39, 4}, {0x22, 1},
    {0x26, 1}, {0x1a, 1}, {0x1b, 1}, {0x2a, 1}, {0x2e, 1}, {0x2c, 1},  {0x2d, 1},
}};
constexpr u32 MAX_MEMORY_PROGRAM_LENGTH = 12;
// The register that holds the middle of the data area, so 16-bit offsets reach all of it. Nothing writes it.
constexpr u32 DATA_BASE_REGISTER = 16;
// Loads that write a general-purpose register: 0x20-0x27 (LB to LWU, with LWL and LWR), LD, LQ, LDL and LDR.
bool isLoad(u32 opcode) {
    constexpr u32 FIRST_LOAD = 0x20, LAST_LOAD = 0x27;
    return (opcode >= FIRST_LOAD && opcode <= LAST_LOAD) || opcode == 0x37 || opcode == 0x1e || opcode == 0x1a ||
           opcode == 0x1b;
}

// An access inside the data area, aligned to its size, so none raises an address error.
u32 memoryInstruction(std::mt19937_64& random) {
    const auto [opcode, bytes] = MEMORY_OPERATIONS[random() % MEMORY_OPERATIONS.size()];
    const s32 half = static_cast<s32>(DATA_BYTES / 2);
    const s32 offset =
        static_cast<s32>(random() % (DATA_BYTES - bytes + 1)) / static_cast<s32>(bytes) * static_cast<s32>(bytes) -
        half;
    u32 rt = field(random, 5);
    if (isLoad(opcode) && rt == DATA_BASE_REGISTER) ++rt;
    return immediate(opcode, DATA_BASE_REGISTER, rt, static_cast<u32>(offset));
}

// An ALU instruction that leaves the data base register alone.
u32 aluKeepingBase(std::mt19937_64& random) {
    u32 code = aluInstruction(random);
    const bool special = (code >> 26) == 0;
    const u32 shift = special ? 11 : 16;
    if (((code >> shift) & 0x1f) == DATA_BASE_REGISTER) code += 1u << shift;
    return code;
}

u64 randomValue(std::mt19937_64& random) {
    if (random() % 4 == 0) return EDGE_VALUES[random() % EDGE_VALUES.size()];
    return random();
}

u128 randomRegister(std::mt19937_64& random) {
    u128 value;
    value.lo = randomValue(random);
    value.hi = randomValue(random);
    return value;
}

} // namespace

std::optional<OpClass> parseOpClass(std::string_view name) {
    if (name == "alu") return OpClass::Alu;
    if (name == "shift") return OpClass::Shift;
    if (name == "branch") return OpClass::Branch;
    if (name == "memory") return OpClass::Memory;
    return std::nullopt;
}

u64 cycleTolerance(OpClass op_class, const EeCase& test_case) {
    return op_class == OpClass::Branch ? test_case.program.size() : 0;
}

EeCase makeCase(OpClass op_class, std::mt19937_64& random) {
    EeCase test_case{};
    if (op_class == OpClass::Branch) {
        test_case.program = branchProgram(random);
    } else if (op_class == OpClass::Memory) {
        const u32 length = 1 + static_cast<u32>(random() % MAX_MEMORY_PROGRAM_LENGTH);
        for (u32 index = 0; index < length; ++index)
            test_case.program.push_back(random() % 3 ? memoryInstruction(random) : aluKeepingBase(random));
        test_case.data.resize(DATA_BYTES);
        for (u8& byte : test_case.data) byte = static_cast<u8>(random());
    } else {
        const u32 length = 1 + static_cast<u32>(random() % MAX_PROGRAM_LENGTH);
        for (u32 index = 0; index < length; ++index)
            test_case.program.push_back(op_class == OpClass::Alu ? aluInstruction(random) : shiftInstruction(random));
    }
    for (u128& gpr : test_case.gprs) gpr = randomRegister(random);
    test_case.gprs[0] = {};
    test_case.hi = randomRegister(random);
    test_case.lo = randomRegister(random);
    if (op_class == OpClass::Memory) {
        test_case.gprs[DATA_BASE_REGISTER] = {};
        test_case.gprs[DATA_BASE_REGISTER].lo = DATA_ADDRESS + DATA_BYTES / 2;
    }
    return test_case;
}

} // namespace bax::difftest
