#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Eunshin::NNUE {

inline constexpr std::size_t kPerspectiveCount = 2;
inline constexpr std::size_t kAccumulatorWidth = 256;

using Accumulator = std::array<std::int32_t, kAccumulatorWidth>;

// Dependency-leaf storage contract shared by Position and the future NNUE
// evaluator.  Checkpoint 1 preserves this state exactly across make/unmake;
// evaluator-owned delta application is introduced in its dedicated phase.
struct AccumulatorState {
    std::array<Accumulator, kPerspectiveCount> values{};
    std::uint8_t validMask = 0;
    std::uint64_t generation = 0;

    constexpr void invalidate(std::uint64_t nextGeneration = 0) noexcept {
        validMask = 0;
        generation = nextGeneration;
    }

    friend bool operator==(const AccumulatorState& lhs,
                           const AccumulatorState& rhs) noexcept {
        return lhs.values == rhs.values &&
               lhs.validMask == rhs.validMask &&
               lhs.generation == rhs.generation;
    }

    friend bool operator!=(const AccumulatorState& lhs,
                           const AccumulatorState& rhs) noexcept {
        return !(lhs == rhs);
    }
};

} // namespace Eunshin::NNUE
