#include "bax/jit/a64_branch.h"

namespace bax::jit {
namespace {

constexpr std::uint32_t B_OPCODE = 0x14000000;
constexpr std::uint32_t IMM26_MASK = 0x03ffffff;
constexpr std::int64_t INSTRUCTION_BYTES = 4;

} // namespace

std::optional<std::uint32_t> encodeBranch(std::uintptr_t from, std::uintptr_t to) {
    const auto offset = static_cast<std::int64_t>(to - from);
    if (offset % INSTRUCTION_BYTES != 0) return std::nullopt;
    if (offset < -BRANCH_RANGE_BYTES || offset >= BRANCH_RANGE_BYTES) return std::nullopt;
    const auto words = static_cast<std::uint32_t>(offset / INSTRUCTION_BYTES);
    return B_OPCODE | (words & IMM26_MASK);
}

} // namespace bax::jit
