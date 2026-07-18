#pragma once

#include "eval/classical.h"
#include "eval/nnue/network.h"
#include "search/search_config.h"

#include <string_view>

namespace Eunshin {

class Position;

struct ResidualGuardResult final {
    Value applied = 0;
    int factorPercent = 100;
};

// Pure, deterministic correction-only policy.  `classical` is consulted only
// to reserve mate-score headroom and is never scaled or replaced.
[[nodiscard]] ResidualGuardResult applyResidualGuard(
    Value classical, Value scaledResidual, int phase) noexcept;

struct EvalResult final {
    Value classical = 0;
    Value networkRaw = 0;
    Value scaledResidual = 0;
    Value networkApplied = 0;
    Value finalScore = 0;
    int guardFactorPercent = 100;
    bool nnueRequested = false;
    bool nnueAvailable = false;
    bool nnueUsed = false;
};

class Evaluator final {
public:
    [[nodiscard]] NNUE::LoadResult loadNetwork(std::string_view path) noexcept;
    void unloadNetwork() noexcept;

    [[nodiscard]] EvalResult evaluate(
        Position& position, const SearchConfig& config) noexcept;

    // The caller invokes this only after a successful doMove.  A false return
    // is a recoverable cache miss, not an evaluation failure.
    [[nodiscard]] bool updateAccumulatorAfterMove(
        Position& position, const SearchConfig& config) noexcept;

    [[nodiscard]] const ClassicalEvaluator& classical() const noexcept {
        return classical_;
    }

    [[nodiscard]] const NNUE::Network& network() const noexcept {
        return network_;
    }

private:
    ClassicalEvaluator classical_{};
    NNUE::Network network_{};
};

} // namespace Eunshin
