#pragma once

#include "core/move.h"
#include "core/types.h"
#include "search/time_manager.h"

#include <array>
#include <cstdint>

namespace Eunshin {

class Position;
class SearchWorker;

// A principal variation is copied directly out of the triangular PV table.
// It never depends on a later TT walk, so a replacement or colliding TT entry
// cannot corrupt the line printed for a completed iteration.
struct PVLine final {
    std::array<Move, MAX_PLY> moves{};
    int length = 0;

    void clear() noexcept {
        moves.fill(Move::none());
        length = 0;
    }

    [[nodiscard]] Move first() const noexcept {
        return length > 0 ? moves[0] : Move::none();
    }

    [[nodiscard]] Move ponder() const noexcept {
        return length > 1 ? moves[1] : Move::none();
    }
};

enum class StopReason : std::uint8_t {
    None,
    DepthLimit,
    NodeLimit,
    TimeLimit,
    ExternalStop,
    MateFound,
    NoEvaluator,
    NoLegalMoves,
    Rejected
};

// Emitted only after a whole iterative-deepening iteration, including all
// required aspiration re-searches, has completed successfully.
struct SearchInfo final {
    int depth = 0;
    int selDepth = 0;
    Value score = VALUE_NONE;
    std::uint64_t nodes = 0;
    std::uint64_t qnodes = 0;
    std::int64_t timeMs = 0;
    int hashfull = 0;
    PVLine pv{};
};

using SearchInfoCallback = void (*)(const SearchInfo&, void*) noexcept;

struct SearchResult final {
    Move bestMove = Move::none();
    Move ponder = Move::none();
    Value score = VALUE_NONE;
    int completedDepth = 0;
    int selDepth = 0;
    std::uint64_t nodes = 0;
    std::uint64_t qnodes = 0;
    std::int64_t timeMs = 0;
    PVLine pv{};
    StopReason stopReason = StopReason::None;
    bool completedAnyIteration = false;
};

// Synchronous single-worker search.  Ownership of any frontend thread lives
// above this boundary; requestStop() is the asynchronous cancellation path.
[[nodiscard]] SearchResult runSearch(
    Position& position,
    SearchWorker& worker,
    const SearchLimits& limits,
    SearchInfoCallback callback = nullptr,
    void* userData = nullptr) noexcept;

} // namespace Eunshin
