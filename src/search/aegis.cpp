#include "search/aegis.h"

#include "core/attacks.h"
#include "core/bitboard.h"
#include "position/position.h"

#include <algorithm>
#include <cstdlib>

namespace Eunshin {
namespace {

constexpr int TT_EXACT_DISAGREEMENT = 160;
constexpr int TT_BOUND_DISAGREEMENT = 200;
constexpr int EVAL_SWING = 200;

[[nodiscard]] bool kingUnderPressure(const Position& position) noexcept {
    const Color defender = position.sideToMove();
    const Color attacker = opposite(defender);
    const Square king = position.kingSquare(defender);
    if (!isValid(king)) return false;

    Bitboard ring = Attacks::king(king);
    int attackedRingSquares = 0;
    while (ring != EMPTY_BB) {
        const Square square = popLsb(ring);
        if (position.attackersTo(square, attacker) != EMPTY_BB)
            ++attackedRingSquares;
    }
    return attackedRingSquares >= 3;
}

[[nodiscard]] bool isPassedPawn(const Position& position,
                                Color color,
                                Square square) noexcept {
    const Color enemy = opposite(color);
    const Bitboard enemyPawns = position.pieces(enemy, PieceType::Pawn);
    const int file = fileOf(square);
    const int rank = rankOf(square);

    for (int adjacent = std::max(0, file - 1);
         adjacent <= std::min(7, file + 1); ++adjacent) {
        Bitboard candidates = enemyPawns & fileMask(adjacent);
        while (candidates != EMPTY_BB) {
            const Square enemyPawn = popLsb(candidates);
            if ((color == Color::White && rankOf(enemyPawn) > rank) ||
                (color == Color::Black && rankOf(enemyPawn) < rank))
                return false;
        }
    }
    return true;
}

[[nodiscard]] bool hasAdvancedPasser(const Position& position,
                                     Color color) noexcept {
    Bitboard pawns = position.pieces(color, PieceType::Pawn);
    while (pawns != EMPTY_BB) {
        const Square square = popLsb(pawns);
        const int rank = rankOf(square);
        const bool advanced = color == Color::White ? rank >= 5 : rank <= 2;
        if (advanced && isPassedPawn(position, color, square)) return true;
    }
    return false;
}

[[nodiscard]] int signalCount(std::uint16_t signals) noexcept {
    int count = 0;
    while (signals != 0) {
        signals = static_cast<std::uint16_t>(signals & (signals - 1));
        ++count;
    }
    return count;
}

} // namespace

AegisAssessment assessAegis(const Position& position,
                            Value staticEval,
                            const TTProbe& tt,
                            Value parentStaticEval,
                            bool rootIterationUnstable) noexcept {
    AegisAssessment result;

    if (tt.hit && tt.data.eval != VALUE_NONE) {
        const int threshold = tt.data.bound == TTBound::Exact
            ? TT_EXACT_DISAGREEMENT : TT_BOUND_DISAGREEMENT;
        if (std::abs(staticEval - tt.data.eval) >= threshold)
            result.signals = static_cast<std::uint16_t>(
                result.signals | TTEvalDisagreement);
    }

    // Parent and child evaluations have opposite side-to-move orientation.
    if (parentStaticEval != VALUE_NONE &&
        std::abs(staticEval + parentStaticEval) >= EVAL_SWING) {
        result.signals = static_cast<std::uint16_t>(
            result.signals | EvaluationSwing);
    }

    if (kingUnderPressure(position))
        result.signals = static_cast<std::uint16_t>(
            result.signals | KingPressure);

    if (hasAdvancedPasser(position, Color::White) ||
        hasAdvancedPasser(position, Color::Black)) {
        result.signals = static_cast<std::uint16_t>(
            result.signals | PasserRace);
    }

    if (rootIterationUnstable)
        result.signals = static_cast<std::uint16_t>(
            result.signals | RootIterationSwing);

    return result;
}

int aegisMarginAllowance(const AegisAssessment& assessment) noexcept {
    // Small and bounded: even all signals together can add only 60 cp.
    return std::min(60, signalCount(assessment.signals) * 15);
}

bool aegisBlocksIIR(const AegisAssessment& assessment) noexcept {
    return assessment.unstable();
}

int aegisRelieveReduction(int reduction,
                          const AegisAssessment& assessment,
                          bool& reliefAlreadyUsed) noexcept {
    if (reduction <= 0 || reliefAlreadyUsed || !assessment.unstable())
        return std::max(0, reduction);
    reliefAlreadyUsed = true;
    return reduction - 1;
}

void addAegisScoreGap(AegisAssessment& assessment) noexcept {
    assessment.signals = static_cast<std::uint16_t>(
        assessment.signals | ScoreGap);
}

} // namespace Eunshin
