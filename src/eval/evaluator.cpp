#include "eval/evaluator.h"

#include "position/position.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace Eunshin {
namespace {

[[nodiscard]] constexpr Value clampEvaluation(Value value) noexcept {
    return value < -NNUE::NETWORK_CP_LIMIT ? -NNUE::NETWORK_CP_LIMIT
         : value > NNUE::NETWORK_CP_LIMIT ? NNUE::NETWORK_CP_LIMIT
                                           : value;
}

[[nodiscard]] bool evaluationUsesNetwork(const SearchConfig& config) noexcept {
    if (!config.useNNUE)
        return false;
    return config.nnueOutputMode == NNUEOutputMode::Residual
        ? config.residualScale > 0
        : config.absoluteBlend > 0;
}

} // namespace

ResidualGuardResult applyResidualGuard(
    Value classical, Value scaledResidual, int phase) noexcept {
    ResidualGuardResult result;
    if (scaledResidual == 0)
        return result;

    const int boundedPhase = std::clamp(phase, 0, MAX_PHASE);
    // Preserve ordinary tactical corrections exactly.  Only the excess above
    // a 4-pawn endgame / 6-pawn opening knee is compressed, asymptotically to
    // twice that knee.  This is continuous at the knee and never hard-zeros a
    // correction.
    const int knee = 400 + (200 * boundedPhase) / MAX_PHASE;
    const std::int64_t magnitude = std::llabs(
        static_cast<std::int64_t>(scaledResidual));
    std::int64_t guardedMagnitude = magnitude;
    if (magnitude > knee) {
        const std::int64_t excess = magnitude - knee;
        guardedMagnitude = knee + NNUE::roundAwayFromZero(
            excess * knee, excess + knee);
    }

    std::int64_t applied = scaledResidual < 0
        ? -guardedMagnitude : guardedMagnitude;
    // Do not manufacture an opposite-sign correction if a hostile position
    // already has an out-of-range Classical value.  The final evaluator clamp
    // handles that pathological input; normal positions reserve mate headroom
    // by limiting only a same-direction residual that would cross the cap.
    if (classical >= -NNUE::NETWORK_CP_LIMIT
        && classical <= NNUE::NETWORK_CP_LIMIT) {
        const std::int64_t minimumCorrection =
            -static_cast<std::int64_t>(NNUE::NETWORK_CP_LIMIT) - classical;
        const std::int64_t maximumCorrection =
            static_cast<std::int64_t>(NNUE::NETWORK_CP_LIMIT) - classical;
        applied = std::clamp(applied, minimumCorrection, maximumCorrection);
    }

    result.applied = static_cast<Value>(applied);
    result.factorPercent = std::clamp(NNUE::roundAwayFromZero(
        std::llabs(applied) * 100, magnitude), 0, 100);
    return result;
}

NNUE::LoadResult Evaluator::loadNetwork(std::string_view path) noexcept {
    return network_.load(path);
}

void Evaluator::unloadNetwork() noexcept {
    network_.unload();
}

EvalResult Evaluator::evaluate(
    Position& position, const SearchConfig& config) noexcept {
    EvalResult result;
    ClassicalBreakdown breakdown;
    result.classical = classical_.evaluate(position, &breakdown);
    result.finalScore = clampEvaluation(result.classical);
    result.nnueRequested = config.useNNUE;

    // Keep the disabled/zero-scale path cold: even an atomic shared_ptr
    // snapshot and its reference-count traffic are unnecessary there.
    if (!evaluationUsesNetwork(config))
        return result;

    result.nnueAvailable = network_.ready();
    if (!result.nnueAvailable)
        return result;

    Value networkScore = 0;
    if (!network_.inferSideToMove(position, networkScore))
        return result;

    result.nnueUsed = true;
    result.networkRaw = networkScore;
    if (config.nnueOutputMode == NNUEOutputMode::Residual) {
        result.scaledResidual = NNUE::roundAwayFromZero(
            static_cast<std::int64_t>(networkScore) * config.residualScale,
            100);
        if (config.residualGuard) {
            const ResidualGuardResult guarded = applyResidualGuard(
                result.classical, result.scaledResidual, breakdown.phase);
            result.networkApplied = guarded.applied;
            result.guardFactorPercent = guarded.factorPercent;
        } else {
            result.networkApplied = result.scaledResidual;
        }
        const std::int64_t combined = static_cast<std::int64_t>(result.classical)
                                    + result.networkApplied;
        result.finalScore = clampEvaluation(static_cast<Value>(std::clamp<std::int64_t>(
            combined,
            std::numeric_limits<Value>::min(),
            std::numeric_limits<Value>::max())));
        return result;
    }

    const int blend = std::clamp(config.absoluteBlend, 0, 100);
    result.finalScore = clampEvaluation(NNUE::roundAwayFromZero(
        static_cast<std::int64_t>(result.classical) * (100 - blend)
        + static_cast<std::int64_t>(networkScore) * blend,
        100));
    result.networkApplied = result.finalScore - result.classical;
    return result;
}

bool Evaluator::updateAccumulatorAfterMove(
    Position& position, const SearchConfig& config) noexcept {
    if (!evaluationUsesNetwork(config) || !network_.ready())
        return false;
    return network_.updateAccumulatorAfterMove(position);
}

} // namespace Eunshin
