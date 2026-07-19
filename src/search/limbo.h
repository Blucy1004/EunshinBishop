#pragma once

#include "core/types.h"
#include "search/search_stack.h"

#include <cstdint>

namespace Eunshin {

class Position;

// LIMBO: Localized Instability Monitor & Bounded deepening Override
// (specification item 19).  A bounded, frontier-only search policy: it may
// grant a single extra ply of verification to a small number of candidate
// moves at the last real ply before quiescence, instead of trusting that
// frontier score outright.  LIMBO never fires inside quiescence, never
// chains consecutive extensions along one line, always respects a cooldown,
// and always leaves TimeManager's hard deadline in control -- it can make a
// search slower, never later than the deadline.  It is a policy decision
// only: SearchWorker/Search own the SearchStack fields it reads bounds from.
namespace Limbo {

// Everything shouldVerify() needs to stay within its stated bounds: how deep
// the search tree still plans to go, which candidate at this node is being
// considered, whether this is the root (where only the best/second-best
// candidate may be verified), whether AEGIS already owns this node's
// instability response, and how much of the search budget remains.
struct FrontierContext final {
    Depth depth = 0;
    int moveIndex = 0;
    bool isRoot = false;
    bool aegisActiveHere = false;
    bool timeLimited = false;
    std::int64_t elapsedMs = 0;
    std::int64_t maximumMs = 0;
    int completedIterations = 0;
};

[[nodiscard]] bool shouldVerify(
    const Position& position,
    const SearchStack& ss,
    const FrontierContext& context) noexcept;

// At most one extra ply; callers must never call this more than once per
// candidate and must gate every call behind shouldVerify().
[[nodiscard]] Depth verificationDepth(Depth normalDepth) noexcept;

} // namespace Limbo
} // namespace Eunshin
