#pragma once

#include "core/types.h"

namespace Eunshin {

struct Score final {
    int mg = 0;
    int eg = 0;

    constexpr Score& operator+=(Score other) noexcept {
        mg += other.mg;
        eg += other.eg;
        return *this;
    }

    constexpr Score& operator-=(Score other) noexcept {
        mg -= other.mg;
        eg -= other.eg;
        return *this;
    }
};

[[nodiscard]] constexpr Score makeScore(int mg, int eg) noexcept {
    return Score{mg, eg};
}

[[nodiscard]] constexpr Score operator+(Score lhs, Score rhs) noexcept {
    lhs += rhs;
    return lhs;
}

[[nodiscard]] constexpr Score operator-(Score lhs, Score rhs) noexcept {
    lhs -= rhs;
    return lhs;
}

[[nodiscard]] constexpr Score operator-(Score score) noexcept {
    return Score{-score.mg, -score.eg};
}

[[nodiscard]] constexpr Score operator*(Score score, int factor) noexcept {
    return Score{score.mg * factor, score.eg * factor};
}

inline constexpr int MAX_PHASE = 24;

// Components are accumulated from White's point of view.  Consumers must add
// every component first and taper exactly once; tapering components separately
// can introduce integer-rounding drift.
struct ClassicalBreakdown final {
    Score material{};
    Score psqt{};
    Score pawns{};
    Score mobility{};
    Score kingSafety{};
    Score threats{};
    Score passedPawns{};
    Score space{};
    Score miscellaneous{};
    int phase = 0;

    [[nodiscard]] constexpr Score total() const noexcept {
        return material + psqt + pawns + mobility + kingSafety + threats
             + passedPawns + space + miscellaneous;
    }
};

[[nodiscard]] constexpr Value taper(Score score, int phase) noexcept {
    const int boundedPhase = phase < 0 ? 0 : (phase > MAX_PHASE ? MAX_PHASE : phase);
    return (score.mg * boundedPhase
          + score.eg * (MAX_PHASE - boundedPhase)) / MAX_PHASE;
}

} // namespace Eunshin
