#pragma once

#include "core/types.h"
#include "search/search_stack.h"
#include "search/tt.h"

#include <cstdint>

namespace Eunshin {

class Position;

// AEGIS is deliberately a bounded search-safety policy.  It can make an
// existing reduction or pruning margin a little more conservative, but it
// cannot extend a line or start an independent re-search.
struct AegisAssessment final {
    std::uint16_t signals = StableNode;

    [[nodiscard]] bool unstable() const noexcept {
        return signals != StableNode;
    }
};

[[nodiscard]] AegisAssessment assessAegis(
    const Position& position,
    Value staticEval,
    const TTProbe& tt,
    Value parentStaticEval,
    bool rootIterationUnstable) noexcept;

[[nodiscard]] int aegisMarginAllowance(
    const AegisAssessment& assessment) noexcept;

[[nodiscard]] bool aegisBlocksIIR(
    const AegisAssessment& assessment) noexcept;

// At most one ply is returned to an LMR search at a node.  The caller owns the
// node-local `reliefAlreadyUsed` flag.
[[nodiscard]] int aegisRelieveReduction(
    int reduction,
    const AegisAssessment& assessment,
    bool& reliefAlreadyUsed) noexcept;

void addAegisScoreGap(AegisAssessment& assessment) noexcept;

} // namespace Eunshin
