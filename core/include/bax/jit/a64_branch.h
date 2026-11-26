#pragma once

#include <cstdint>
#include <optional>

namespace bax::jit {

// The farthest an AArch64 B instruction reaches in either direction: a signed 26-bit word offset.
inline constexpr std::int64_t BRANCH_RANGE_BYTES = std::int64_t{1} << 27;

// Encodes an unconditional B at executable address `from` that jumps to `to`. Nothing when the target is not
// word-aligned relative to `from`, or lies outside +-128 MB; the caller then jumps through a register instead.
std::optional<std::uint32_t> encodeBranch(std::uintptr_t from, std::uintptr_t to);

} // namespace bax::jit
