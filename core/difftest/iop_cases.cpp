// SPDX-License-Identifier: GPL-3.0-or-later
#include "iop_cases.h"

#include <utility>

namespace bax::difftest {
namespace {

constexpr u32 MAX_PROGRAM_LENGTH = 16;
// MIPS I function codes and opcodes, from the R3000A manual.
constexpr std::array<u32, 18> ALU_FUNCTIONS = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2a,
                                               0x2b, 0x18, 0x19, 0x1a, 0x1b, 0x10, 0x11, 0x12, 0x13};
constexpr std::array<u32, 8> ALU_IMMEDIATE_OPCODES = {0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
constexpr std::array<u32, 6> SHIFT_FUNCTIONS = {0x00, 0x02, 0x03, 0x04, 0x06, 0x07};
constexpr std::array<u32, 4> COMPARE_BRANCH_OPCODES = {0x04, 0x05, 0x06, 0x07};
constexpr std::array<u32, 4> REGIMM_BRANCHES = {0x00, 0x01, 0x10, 0x11};
// Loads and stores (opcode, access size): LB LH LWL LW LBU LHU LWR, SB SH SWL SW SWR.
constexpr std::array<std::pair<u32, u32>, 12> MEMORY_OPERATIONS = {{
    {0x20, 1},
    {0x21, 2},
    {0x22, 1},
    {0x23, 4},
    {0x24, 1},
    {0x25, 2},
    {0x26, 1},
    {0x28, 1},
    {0x29, 2},
    {0x2a, 1},
    {0x2b, 4},
    {0x2e, 1},
}};
constexpr u32 REGIMM = 0x01, J = 0x02, JAL = 0x03, JR = 0x08, JALR = 0x09;
// The register JR and JALR jump through, and the one the memory class addresses from; no other instruction in a
// case writes either.
constexpr u32 JUMP_REGISTER = 25;
constexpr u32 DATA_BASE_REGISTER = 16;
// psxJ treats a delay slot whose upper half is 0x2400 (addiu $0, $0, n) as an IRX import stub; cases never make one.
constexpr u32 IRX_IMPORT_MARKER = 0x2400;
constexpr u32 BRANCH_TO_SELF = 0x1000ffff; // beq $0, $0, -1
constexpr u32 NOP = 0;

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

// An ALU or shift instruction that writes neither of the reserved registers and is no IRX import stub.
u32 computeInstruction(std::mt19937_64& random, IopClass iop_class) {
    for (;;) {
        const u32 rs = field(random, 5), rt = field(random, 5), rd = field(random, 5);
        u32 code = 0;
        if (iop_class == IopClass::Shift)
            code = special(rs, rt, rd, field(random, 5), pick(random, SHIFT_FUNCTIONS));
        else if (random() % 2)
            code = special(rs, rt, rd, 0, pick(random, ALU_FUNCTIONS));
        else
            code = immediate(pick(random, ALU_IMMEDIATE_OPCODES), rs, rt, field(random, 16));
        const u32 destination = (code >> 26) == 0 ? rd : rt;
        if (destination != JUMP_REGISTER && destination != DATA_BASE_REGISTER && (code >> 16) != IRX_IMPORT_MARKER)
            return code;
    }
}

u32 branchInstruction(std::mt19937_64& random, u32 position, u32 target, std::optional<u32>& register_target) {
    const u32 offset = target - (position + 1);
    const u32 rs = field(random, 5);
    const u32 rt = random() % 4 == 0 ? rs : field(random, 5);
    const u32 kind = random() % 4;
    if (kind == 3 && !register_target) {
        register_target = target;
        u32 link = field(random, 5);
        if (link == JUMP_REGISTER || link == DATA_BASE_REGISTER) link = 31;
        return random() % 2 ? special(JUMP_REGISTER, 0, 0, 0, JR) : special(JUMP_REGISTER, 0, link, 0, JALR);
    }
    if (kind == 0) return immediate(pick(random, COMPARE_BRANCH_OPCODES), rs, rt, offset);
    if (kind == 1) return immediate(REGIMM, rs, pick(random, REGIMM_BRANCHES), offset);
    return ((random() % 2 ? J : JAL) << 26) | (((IOP_PROGRAM_ADDRESS >> 2) + target) & 0x03ffffff);
}

// An access inside the data area, aligned to its size.
u32 memoryInstruction(std::mt19937_64& random) {
    const auto [opcode, bytes] = MEMORY_OPERATIONS[random() % MEMORY_OPERATIONS.size()];
    const s32 half = static_cast<s32>(IOP_DATA_BYTES / 2);
    const s32 offset =
        static_cast<s32>(random() % (IOP_DATA_BYTES - bytes + 1)) / static_cast<s32>(bytes) * static_cast<s32>(bytes) -
        half;
    u32 rt = field(random, 5);
    constexpr u32 FIRST_STORE = 0x28;
    if (opcode < FIRST_STORE && (rt == DATA_BASE_REGISTER || rt == JUMP_REGISTER)) rt = 1;
    return immediate(opcode, DATA_BASE_REGISTER, rt, static_cast<u32>(offset));
}

} // namespace

std::optional<IopClass> parseIopClass(std::string_view name) {
    if (name == "iop-alu") return IopClass::Alu;
    if (name == "iop-shift") return IopClass::Shift;
    if (name == "iop-branch") return IopClass::Branch;
    if (name == "iop-memory") return IopClass::Memory;
    return std::nullopt;
}

IopCase makeIopCase(IopClass iop_class, std::mt19937_64& random) {
    IopCase test_case{};
    const u32 length = 2 + static_cast<u32>(random() % (MAX_PROGRAM_LENGTH - 1));
    std::optional<u32> register_target;
    bool in_delay_slot = false;
    for (u32 position = 0; position < length; ++position) {
        const bool can_branch = iop_class == IopClass::Branch && !in_delay_slot && position + 1 < length;
        if (can_branch && random() % 3 == 0) {
            const u32 target = position + 2 + static_cast<u32>(random() % (length - position - 1));
            test_case.program.push_back(branchInstruction(random, position, target, register_target));
            in_delay_slot = true;
            continue;
        }
        const bool memory = iop_class == IopClass::Memory && random() % 3 != 0;
        test_case.program.push_back(
            memory ? memoryInstruction(random)
                   : computeInstruction(random, iop_class == IopClass::Shift ? IopClass::Shift : IopClass::Alu));
        in_delay_slot = false;
    }
    test_case.program.push_back(BRANCH_TO_SELF);
    test_case.program.push_back(NOP);
    for (u32& gpr : test_case.gprs) gpr = static_cast<u32>(random());
    test_case.gprs[0] = 0;
    test_case.gprs[DATA_BASE_REGISTER] = IOP_DATA_ADDRESS + IOP_DATA_BYTES / 2;
    if (register_target) test_case.gprs[JUMP_REGISTER] = IOP_PROGRAM_ADDRESS + *register_target * 4;
    test_case.data.resize(IOP_DATA_BYTES);
    for (u8& byte : test_case.data) byte = static_cast<u8>(random());
    return test_case;
}

} // namespace bax::difftest
