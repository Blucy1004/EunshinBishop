#pragma once

#include "eval/score.h"

namespace Eunshin {

class Position;

class ClassicalEvaluator final {
public:
    // Returns centipawns from the side-to-move point of view.  The optional
    // breakdown remains White-relative MG/EG data so it can be summed and
    // tapered without losing a centipawn to per-category rounding.
    [[nodiscard]] Value evaluate(
        const Position& position,
        ClassicalBreakdown* breakdown = nullptr) const noexcept;
};

} // namespace Eunshin
