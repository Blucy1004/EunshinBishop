#include "search/limbo.h"

#include "position/position.h"

namespace Eunshin {
namespace Limbo {
namespace {

// A small number of candidates only: root may verify its best and
// second-best move; any other frontier node may verify only the first two
// moves MovePicker offers (already the highest-ordered, most consequential
// ones thanks to TT/killer/history staging).
constexpr int MAX_CANDIDATE_INDEX = 2;

// Never spend verification budget past this fraction of the maximum time
// budget: TimeManager's hard deadline stays authoritative, and LIMBO must
// never be the reason a 10+0.1 game loses on time.
constexpr std::int64_t TIME_BUDGET_PERCENT = 70;

} // namespace

bool shouldVerify(const Position& position,
                  const SearchStack& ss,
                  const FrontierContext& context) noexcept {
    // Frontier-only: exactly the last real ply before the child would drop
    // into quiescence.  Quiescence itself never deepens (no caller in
    // qsearch may pass depth == 1 here).
    if (context.depth != 1) return false;

    // Never stack onto a check extension, and never overlap AEGIS's role.
    if (position.inCheck()) return false;
    if (context.aegisActiveHere) return false;

    // No consecutive extension along this line, and respect the cooldown
    // the caller decays every ply.
    if (ss.limboChain != 0) return false;
    if (ss.limboCooldown != 0) return false;

    // Few candidates only.
    const int limit = context.isRoot ? MAX_CANDIDATE_INDEX : MAX_CANDIDATE_INDEX;
    if (context.moveIndex < 1 || context.moveIndex > limit) return false;

    // Remaining time and iteration budget: refuse to spend verification
    // time with no completed-iteration baseline, and refuse once the
    // search has already used most of its time budget.
    if (context.timeLimited) {
        if (context.completedIterations == 0) return false;
        if (context.maximumMs > 0 &&
            context.elapsedMs * 100 >= context.maximumMs * TIME_BUDGET_PERCENT)
            return false;
    }

    return true;
}

Depth verificationDepth(Depth normalDepth) noexcept {
    return normalDepth + 1;
}

} // namespace Limbo
} // namespace Eunshin
