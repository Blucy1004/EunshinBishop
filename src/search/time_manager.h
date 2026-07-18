#pragma once

#include "core/move.h"
#include "core/types.h"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace Eunshin {

// Negative clock values mean that the corresponding UCI limit was not
// supplied.  Zero depth/nodes mean unlimited; an explicit zero movetime is a
// valid immediate hard limit.  `infinite` disables clock-derived limits but
// does not disable an explicit node or depth limit.
struct SearchLimits final {
    std::int64_t whiteTimeMs = -1;
    std::int64_t blackTimeMs = -1;
    std::int64_t whiteIncrementMs = 0;
    std::int64_t blackIncrementMs = 0;
    int movesToGo = 0;
    std::int64_t moveTimeMs = -1;
    int depth = 0;
    std::uint64_t nodes = 0;
    bool infinite = false;
};

// Only completed iterations are passed to TimeManager.  An interrupted
// aspiration re-search must never be recorded here, which preserves the last
// fully usable best move, score, and PV in the search layer.
struct IterationInfo final {
    int depth = 0;
    Move bestMove = Move::none();
    Value score = VALUE_NONE;
    int rootMoveChanges = 0;
    int aspirationFailures = 0;
    bool aegisRootUnstable = false;
    bool limboRootRisk = false;
};

struct IterationStability final {
    int completedIterations = 0;
    int bestMoveStableIterations = 0;
    int scoreStableIterations = 0;
    int lastScoreSwing = 0;
    std::uint64_t totalRootMoveChanges = 0;
    std::uint64_t totalAspirationFailures = 0;
    bool bestMoveChanged = false;
    bool aegisRootUnstable = false;
    bool limboRootRisk = false;
};

// Single-Worker time control.  The search owns clock-check amortization: it
// calls immediateStopRequested() at every node and hardTimeReached() only at
// its chosen clock-check interval.  This keeps external stop and node limits
// exact without putting steady_clock::now() in every node.
class TimeManager final {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void initialize(const SearchLimits& limits,
                    Color sideToMove,
                    int gamePly,
                    int moveOverheadMs) noexcept;

    // The explicit start overload makes deadline behavior deterministic in
    // unit tests without introducing a mutable or virtual clock in production.
    void initialize(const SearchLimits& limits,
                    Color sideToMove,
                    int gamePly,
                    int moveOverheadMs,
                    TimePoint start) noexcept;

    [[nodiscard]] bool immediateStopRequested(
        std::uint64_t visitedNodes,
        const std::atomic_bool& externalStop) const noexcept;

    // Convenience wrapper for the search's amortized check site.  Passing
    // checkClock=false still checks the atomic stop and exact node limit.
    [[nodiscard]] bool shouldStop(
        std::uint64_t visitedNodes,
        const std::atomic_bool& externalStop,
        bool checkClock = true) const noexcept;

    [[nodiscard]] bool hardTimeReached() const noexcept;
    [[nodiscard]] bool hardTimeReached(TimePoint now) const noexcept;
    [[nodiscard]] bool softTimeReached() const noexcept;
    [[nodiscard]] bool softTimeReached(TimePoint now) const noexcept;

    void recordCompletedIteration(const IterationInfo& info) noexcept;

    // Called after recordCompletedIteration().  Stable searches stop at the
    // soft budget; volatile searches receive a bounded extension that can
    // never pass maximumTimeMs().
    [[nodiscard]] bool shouldStartNextIteration() const noexcept;
    [[nodiscard]] bool shouldStartNextIteration(TimePoint now) const noexcept;

    [[nodiscard]] std::int64_t elapsedTimeMs() const noexcept;
    [[nodiscard]] std::int64_t elapsedTimeMs(TimePoint now) const noexcept;
    [[nodiscard]] std::int64_t optimumTimeMs() const noexcept {
        return optimumTimeMs_;
    }
    [[nodiscard]] std::int64_t maximumTimeMs() const noexcept {
        return maximumTimeMs_;
    }
    [[nodiscard]] TimePoint startTime() const noexcept { return startTime_; }
    [[nodiscard]] TimePoint softDeadline() const noexcept { return softDeadline_; }
    [[nodiscard]] TimePoint hardDeadline() const noexcept { return hardDeadline_; }
    [[nodiscard]] bool timeLimited() const noexcept { return timeLimited_; }
    [[nodiscard]] bool emergencyMode() const noexcept { return emergencyMode_; }
    [[nodiscard]] const SearchLimits& limits() const noexcept { return limits_; }
    [[nodiscard]] const IterationInfo& lastIteration() const noexcept {
        return lastIteration_;
    }
    [[nodiscard]] const IterationStability& stability() const noexcept {
        return stability_;
    }

private:
    [[nodiscard]] std::int64_t nextIterationBudgetMs() const noexcept;

    SearchLimits limits_{};
    TimePoint startTime_{};
    TimePoint softDeadline_ = TimePoint::max();
    TimePoint hardDeadline_ = TimePoint::max();
    std::int64_t optimumTimeMs_ = 0;
    std::int64_t maximumTimeMs_ = 0;
    bool timeLimited_ = false;
    bool emergencyMode_ = false;

    IterationInfo lastIteration_{};
    IterationStability stability_{};
};

} // namespace Eunshin
