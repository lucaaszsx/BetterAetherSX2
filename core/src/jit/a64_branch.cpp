#include "bax/jit/a64_branch.h"

namespace bax::jit {
namespace {

constexpr std::uint32_t B_OPCODE = 0x14000000;
constexpr std::uint32_t IMM26_MASK = 0x03ffffff;
constexpr std::int64_t INSTRUCTION_BYTES = 4;

} // namespace

// The distance is taken unsigned with an explicit direction, so it is exact for any pointer width and is checked
// against the range before it becomes a signed offset.
std::optional<std::uint32_t> encodeBranch(std::uintptr_t from, std::uintptr_t to) {
    const bool forward = to >= from;
    const std::uintptr_t distance = forward ? to - from : from - to;
    if (distance % INSTRUCTION_BYTES != 0) return std::nullopt;
    const auto range = static_cast<std::uintptr_t>(BRANCH_RANGE_BYTES);
    if (forward ? distance >= range : distance > range) return std::nullopt;
    const auto offset = static_cast<std::int64_t>(distance);
    const auto words = static_cast<std::uint32_t>((forward ? offset : -offset) / INSTRUCTION_BYTES);
    return B_OPCODE | (words & IMM26_MASK);
}

} // namespace bax::jit
