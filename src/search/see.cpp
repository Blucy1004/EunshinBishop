#include "search/see.h"

#include "position/movegen.h"
#include "position/position.h"
#include "position/state.h"

#include <algorithm>
#include <array>

namespace Eunshin {
namespace See {
namespace {

constexpr int MAX_EXCHANGE_PLY = 32;

constexpr Value pieceValue(PieceType type) noexcept {
    switch (type) {
    case PieceType::Pawn:   return 100;
    case PieceType::Knight: return 320;
    case PieceType::Bishop: return 330;
    case PieceType::Rook:   return 500;
    case PieceType::Queen:  return 900;
    case PieceType::King:   return 20000;
    default:                return 0;
    }
}

[[nodiscard]] PieceType capturedTypeOf(const Position& position, Move move) noexcept {
    if (move.isEnPassant()) return PieceType::Pawn;
    return typeOf(position.pieceOn(move.to()));
}

struct Candidate {
    Move move;
    PieceType attacker;   // Pre-move type: always Pawn for a promoting move.
    Value attackerValue;
};

// Collects the pseudo-legal recaptures landing on `target`, sorted cheapest
// attacker first, keeping exactly one representative per (from, to) pair so
// the four promotion-type moves a promoting pawn generates collapse to a
// single logical attacker.  Queen is kept as that representative: it is the
// value-maximizing choice for the side recapturing, and the classic
// swap-list algorithm this mirrors already assumes one canonical outcome per
// attacker rather than search-widening underpromotion as a separate branch.
// Legality (pins, king-into-check) is deliberately NOT filtered here: the
// caller tries candidates in order and lets Position::doMove reject illegal
// ones, so a pinned piece is skipped in favor of the next cheapest attacker.
[[nodiscard]] std::array<Candidate, MAX_MOVES> collectCandidates(
    const Position& pos, Square target, std::size_t& count) noexcept {
    std::array<Candidate, MAX_MOVES> candidates{};
    count = 0;

    MoveList pseudo;
    generateMoves(pos, pseudo, GenerationType::Captures);

    for (std::size_t i = 0; i < pseudo.size; ++i) {
        const Move move = pseudo[i];
        if (move.to() != target) continue;
        if (move.isPromotion() && move.type() != MoveType::PromotionQueen)
            continue;

        const PieceType attacker = typeOf(pos.pieceOn(move.from()));
        if (!isValid(attacker)) continue;

        candidates[count++] = Candidate{move, attacker, pieceValue(attacker)};
    }

    std::sort(candidates.begin(), candidates.begin() + static_cast<long>(count),
              [](const Candidate& lhs, const Candidate& rhs) noexcept {
                  return lhs.attackerValue < rhs.attackerValue;
              });
    return candidates;
}

} // namespace

Value see(const Position& position, Move move) noexcept {
    // Castling exchanges no material and is never a SEE candidate.
    if (move.isNone() || move.isCastle()) return 0;

    std::array<Value, MAX_EXCHANGE_PLY> gain{};
    gain[0] = pieceValue(capturedTypeOf(position, move));
    if (move.isPromotion())
        gain[0] += pieceValue(move.promotionType()) - pieceValue(PieceType::Pawn);

    // A local scratch chain: replaying the exchange with real doMove() lets
    // Position's own legality machinery (pins, king-into-check, en passant
    // occupancy, castling-rights/promotion bookkeeping) settle every edge
    // case this module has to get right, instead of re-deriving it with a
    // second bitboard model.  `states` outlives every doMove() below and is
    // simply discarded (no undoMove needed) once `see` returns.
    StateInfo rootState{};
    Position scratch = position.snapshotForSearch(rootState);
    std::array<StateInfo, MAX_EXCHANGE_PLY> states{};

    if (!scratch.doMove(move, states[1]))
        return gain[0];

    // `attacker` is the piece now occupying `target`, about to potentially
    // be recaptured; it starts as the root mover (or its promoted type) and
    // is updated to whichever recapture is actually applied below.  This
    // mirrors MovePicker's frozen-reference ordering SEE gain/attacker
    // bookkeeping exactly, so the same well-tested backward min-max pass
    // applies unchanged.
    PieceType attacker = move.isPromotion()
        ? move.promotionType()
        : typeOf(position.pieceOn(move.from()));
    const Square target = move.to();

    int depth = 0;
    bool haveAttacker = true;

    while (haveAttacker) {
        if (depth + 1 >= MAX_EXCHANGE_PLY) break;
        ++depth;
        gain[static_cast<std::size_t>(depth)] =
            pieceValue(attacker) - gain[static_cast<std::size_t>(depth - 1)];
        if (std::max(-gain[static_cast<std::size_t>(depth - 1)],
                     gain[static_cast<std::size_t>(depth)]) < 0)
            break;

        std::size_t count = 0;
        const std::array<Candidate, MAX_MOVES> candidates =
            collectCandidates(scratch, target, count);

        haveAttacker = false;
        for (std::size_t i = 0; i < count; ++i) {
            if (depth + 1 >= MAX_EXCHANGE_PLY) break;
            const Candidate& candidate = candidates[i];
            StateInfo& nextState = states[static_cast<std::size_t>(depth + 1)];
            if (!scratch.doMove(candidate.move, nextState))
                continue; // Pinned or otherwise illegal: try the next attacker.

            attacker = candidate.move.isPromotion()
                ? candidate.move.promotionType()
                : candidate.attacker;
            haveAttacker = true;
            break;
        }
    }

    while (--depth > 0) {
        gain[static_cast<std::size_t>(depth - 1)] = -std::max(
            -gain[static_cast<std::size_t>(depth - 1)],
            gain[static_cast<std::size_t>(depth)]);
    }
    return gain[0];
}

bool seeGe(const Position& position, Move move, Value threshold) noexcept {
    return see(position, move) >= threshold;
}

} // namespace See
} // namespace Eunshin
