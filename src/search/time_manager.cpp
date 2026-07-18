#include "search/time_manager.h"

#include <algorithm>
#include <limits>

namespace Eunshin {
namespace {

constexpr std::int64_t MAX_SUPPORTED_TIME_MS = 30LL * 24 * 60 * 60 * 1000;
constexpr int DEFAULT_MOVES_TO_GO = 30;
constexpr int MIN_ESTIMATED_MOVES_TO_GO = 12;
constexpr int MAX_MOVES_TO_GO = 100;
constexpr int SCORE_STABLE_THRESHOLD = 25;
constexpr int SCORE_SWING_THRESHOLD = 70;

[[nodiscard]] std::int64_t boundedNonNegative(std::int64_t value) noexcept {
    return std::max<std::int64_t>(0,
        std::min<std::int64_t>(value, MAX_SUPPORTED_TIME_MS));
}

[[nodiscard]] std::int64_t saturatingAdd(std::int64_t lhs,
                                         std::int64_t rhs) noexcept {
    lhs = boundedNonNegative(lhs);
    rhs = boundedNonNegative(rhs);
    if (lhs >= MAX_SUPPORTED_TIME_MS - rhs) return MAX_SUPPORTED_TIME_MS;
    return lhs + rhs;
}

[[nodiscard]] std::int64_t saturatingScale(std::int64_t value,
                                           int numerator,
                                           int denominator) noexcept {
    value = boundedNonNegative(value);
    if (value == 0 || numerator <= 0 || denominator <= 0) return 0;
    if (value > MAX_SUPPORTED_TIME_MS / numerator)
        return MAX_SUPPORTED_TIME_MS;
    return std::min<std::int64_t>(MAX_SUPPORTED_TIME_MS,
                                  value * numerator / denominator);
}

[[nodiscard]] int estimatedMovesToGo(int requested, int gamePly) noexcept {
    if (requested > 0)
        return std::max(1, std::min(requested, MAX_MOVES_TO_GO));

    // Retain the reference's 30-move default near the opening, then become
    // mildly less conservative as the game advances.  This is bounded well
    // above one move so a missing movestogo can never consume the whole clock.
    const int boundedPly = std::max(0, std::min(gamePly, 144));
    return std::max(MIN_ESTIMATED_MOVES_TO_GO,
                    DEFAULT_MOVES_TO_GO - boundedPly / 8);
}

[[nodiscard]] std::uint64_t addCounter(std::uint64_t current,
                                       int amount) noexcept {
    if (amount <= 0) return current;
    const std::uint64_t increment = static_cast<std::uint64_t>(amount);
    if (current > std::numeric_limits<std::uint64_t>::max() - increment)
        return std::numeric_limits<std::uint64_t>::max();
    return current + increment;
}

} // namespace

void TimeManager::initialize(const SearchLimits& limits,
                             Color sideToMove,
                             int gamePly,
                             int moveOverheadMs) noexcept {
    initialize(limits, sideToMove, gamePly, moveOverheadMs, Clock::now());
}

void TimeManager::initialize(const SearchLimits& limits,
                             Color sideToMove,
                             int gamePly,
                             int moveOverheadMs,
                             TimePoint start) noexcept {
    limits_ = limits;
    startTime_ = start;
    softDeadline_ = TimePoint::max();
    hardDeadline_ = TimePoint::max();
    optimumTimeMs_ = 0;
    maximumTimeMs_ = 0;
    timeLimited_ = false;
    emergencyMode_ = false;
    lastIteration_ = IterationInfo{};
    stability_ = IterationStability{};

    if (limits_.infinite) return;

    const std::int64_t overhead = boundedNonNegative(moveOverheadMs);

    if (limits_.moveTimeMs >= 0) {
        const std::int64_t requested = boundedNonNegative(limits_.moveTimeMs);
        const std::int64_t usable = requested > overhead ? requested - overhead : 0;
        optimumTimeMs_ = usable;
        maximumTimeMs_ = usable;
        timeLimited_ = true;
    } else {
        const std::int64_t remaining = sideToMove == Color::White
            ? limits_.whiteTimeMs
            : sideToMove == Color::Black ? limits_.blackTimeMs : -1;
        if (remaining < 0) return;

        const std::int64_t increment = boundedNonNegative(
            sideToMove == Color::White ? limits_.whiteIncrementMs
                                       : limits_.blackIncrementMs);
        const std::int64_t boundedRemaining = boundedNonNegative(remaining);
        const std::int64_t usable = boundedRemaining > overhead
            ? boundedRemaining - overhead : 0;
        const int moves = estimatedMovesToGo(limits_.movesToGo, gamePly);

        const std::int64_t base = usable / moves;
        const std::int64_t incrementShare = increment / 2;
        optimumTimeMs_ = std::min(usable, saturatingAdd(base, incrementShare));
        if (usable > 0 && optimumTimeMs_ == 0) optimumTimeMs_ = 1;

        maximumTimeMs_ = std::min(usable,
            std::max(optimumTimeMs_, saturatingScale(optimumTimeMs_, 4, 1)));
        timeLimited_ = true;

        const std::int64_t emergencyThreshold = std::max<std::int64_t>(
            1000, saturatingScale(overhead, 8, 1));
        emergencyMode_ = usable <= emergencyThreshold;
    }

    softDeadline_ = startTime_ + std::chrono::milliseconds(optimumTimeMs_);
    hardDeadline_ = startTime_ + std::chrono::milliseconds(maximumTimeMs_);
}

bool TimeManager::immediateStopRequested(
    std::uint64_t visitedNodes,
    const std::atomic_bool& externalStop) const noexcept {
    if (externalStop.load(std::memory_order_relaxed)) return true;
    return limits_.nodes > 0 && visitedNodes >= limits_.nodes;
}

bool TimeManager::shouldStop(std::uint64_t visitedNodes,
                             const std::atomic_bool& externalStop,
                             bool checkClock) const noexcept {
    if (immediateStopRequested(visitedNodes, externalStop)) return true;
    return checkClock && hardTimeReached();
}

bool TimeManager::hardTimeReached() const noexcept {
    return hardTimeReached(Clock::now());
}

bool TimeManager::hardTimeReached(TimePoint now) const noexcept {
    return timeLimited_ && now >= hardDeadline_;
}

bool TimeManager::softTimeReached() const noexcept {
    return softTimeReached(Clock::now());
}

bool TimeManager::softTimeReached(TimePoint now) const noexcept {
    return timeLimited_ && now >= softDeadline_;
}

void TimeManager::recordCompletedIteration(const IterationInfo& info) noexcept {
    const bool hadPrevious = stability_.completedIterations > 0;
    const bool validPreviousScore = lastIteration_.score != VALUE_NONE;
    const bool validScore = info.score != VALUE_NONE;

    stability_.bestMoveChanged = hadPrevious &&
        !lastIteration_.bestMove.isNone() && !info.bestMove.isNone() &&
        lastIteration_.bestMove != info.bestMove;

    if (!hadPrevious || stability_.bestMoveChanged)
        stability_.bestMoveStableIterations = 1;
    else if (stability_.bestMoveStableIterations <
             std::numeric_limits<int>::max())
        ++stability_.bestMoveStableIterations;

    stability_.lastScoreSwing = 0;
    if (hadPrevious && validPreviousScore && validScore) {
        stability_.lastScoreSwing = std::abs(info.score - lastIteration_.score);
        if (stability_.lastScoreSwing <= SCORE_STABLE_THRESHOLD) {
            if (stability_.scoreStableIterations <
                std::numeric_limits<int>::max())
                ++stability_.scoreStableIterations;
        } else {
            stability_.scoreStableIterations = 1;
        }
    } else {
        stability_.scoreStableIterations = 1;
    }

    stability_.totalRootMoveChanges = addCounter(
        stability_.totalRootMoveChanges, info.rootMoveChanges);
    stability_.totalAspirationFailures = addCounter(
        stability_.totalAspirationFailures, info.aspirationFailures);
    stability_.aegisRootUnstable = info.aegisRootUnstable;
    stability_.limboRootRisk = info.limboRootRisk;
    if (stability_.completedIterations < std::numeric_limits<int>::max())
        ++stability_.completedIterations;

    lastIteration_ = info;
    lastIteration_.rootMoveChanges = std::max(0, info.rootMoveChanges);
    lastIteration_.aspirationFailures = std::max(0, info.aspirationFailures);
}

bool TimeManager::shouldStartNextIteration() const noexcept {
    return shouldStartNextIteration(Clock::now());
}

bool TimeManager::shouldStartNextIteration(TimePoint now) const noexcept {
    if (limits_.depth > 0 && lastIteration_.depth >= limits_.depth)
        return false;
    if (!timeLimited_) return true;
    if (hardTimeReached(now)) return false;
    return elapsedTimeMs(now) < nextIterationBudgetMs();
}

std::int64_t TimeManager::elapsedTimeMs() const noexcept {
    return elapsedTimeMs(Clock::now());
}

std::int64_t TimeManager::elapsedTimeMs(TimePoint now) const noexcept {
    if (now <= startTime_) return 0;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - startTime_).count();
    return boundedNonNegative(elapsed);
}

std::int64_t TimeManager::nextIterationBudgetMs() const noexcept {
    if (!timeLimited_) return MAX_SUPPORTED_TIME_MS;
    if (maximumTimeMs_ <= 0 || optimumTimeMs_ <= 0) return 0;

    // Emergency mode deliberately ignores all extension signals: finishing a
    // move inside the hard reserve is more important than another iteration.
    if (emergencyMode_) return optimumTimeMs_;

    int percent = 100;
    if (stability_.completedIterations < 2 || stability_.bestMoveChanged)
        percent += 25;
    if (stability_.lastScoreSwing >= SCORE_SWING_THRESHOLD)
        percent += 30;
    else if (stability_.lastScoreSwing > SCORE_STABLE_THRESHOLD)
        percent += 15;

    percent += 10 * std::min(3, std::max(0, lastIteration_.rootMoveChanges));
    percent += 15 * std::min(3, std::max(0, lastIteration_.aspirationFailures));
    if (lastIteration_.aegisRootUnstable) percent += 20;
    if (lastIteration_.limboRootRisk) percent += 20;

    return std::min(maximumTimeMs_,
                    saturatingScale(optimumTimeMs_, percent, 100));
}

} // namespace Eunshin
