#pragma once

#include <cstdint>

namespace Eunshin {
namespace Random {

inline constexpr std::uint64_t DEFAULT_SEED = 0x9E3779B97F4A7C15ULL;

// The engine initializes deterministic tables on one startup thread. Keeping
// this stream explicit preserves the reference order: reset, rook magics,
// bishop magics, then Zobrist keys.
void reset(std::uint64_t seed = DEFAULT_SEED) noexcept;

[[nodiscard]] std::uint64_t next() noexcept;
[[nodiscard]] std::uint64_t sparse() noexcept;
[[nodiscard]] std::uint64_t state() noexcept;

} // namespace Random
} // namespace Eunshin
