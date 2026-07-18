#include "search/move_picker.h"

#include "core/attacks.h"
#include "core/bitboard.h"
#include "position/movegen.h"
#include "position/position.h"
#include "search/history.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace Eunshin {
namespace {

constexpr int pieceValue(PieceType type) noexcept {
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

PieceType capturedType(const Position& position, Move move) noexcept {
    if (move.isEnPassant()) return PieceType::Pawn;
    return typeOf(position.pieceOn(move.to()));
}

bool isCapture(const Position& position, Move move) noexcept {
    return move.isEnPassant() || isValid(position.pieceOn(move.to()));
}

Bitboard exchangeAttackers(const Position& position,
                           Square square,
                           Bitboard occupied) noexcept {
    Bitboard attackers = EMPTY_BB;
    attackers |= Attacks::pawn(Color::Black, square) &
                 position.pieces(Color::White, PieceType::Pawn);
    attackers |= Attacks::pawn(Color::White, square) &
                 position.pieces(Color::Black, PieceType::Pawn);
    attackers |= Attacks::knight(square) & position.pieces(PieceType::Knight);
    attackers |= Attacks::king(square) & position.pieces(PieceType::King);
    attackers |= Attacks::bishop(square, occupied) &
                 (position.pieces(PieceType::Bishop) |
                  position.pieces(PieceType::Queen));
    attackers |= Attacks::rook(square, occupied) &
                 (position.pieces(PieceType::Rook) |
                  position.pieces(PieceType::Queen));
    return attackers;
}

// This is the frozen reference's swap-list SEE, used only for move ordering.
// The stricter pin/king-legality SEE and its independent pruning tests are a
// later numbered feature; SEEPruning never consults this private helper.
int orderingSee(const Position& position, Move move) noexcept {
    std::array<int, 32> gain{};
    int depth = 0;
    Bitboard occupied = position.occupied();
    PieceType attacker = typeOf(position.pieceOn(move.from()));
    Color side = position.sideToMove();
    PieceType captured = capturedType(position, move);

    if (move.isEnPassant()) {
        const Square capturedSquare = makeSquare(
            fileOf(move.to()),
            rankOf(move.to()) + (side == Color::White ? -1 : 1));
        occupied &= ~bit(capturedSquare);
    }

    gain[0] = pieceValue(captured);
    if (move.isPromotion()) {
        gain[0] += pieceValue(move.promotionType()) - pieceValue(PieceType::Pawn);
        attacker = move.promotionType();
    }

    Bitboard fromSet = bit(move.from());
    do {
        if (depth + 1 >= static_cast<int>(gain.size())) break;
        ++depth;
        gain[static_cast<std::size_t>(depth)] =
            pieceValue(attacker) - gain[static_cast<std::size_t>(depth - 1)];
        if (std::max(-gain[static_cast<std::size_t>(depth - 1)],
                     gain[static_cast<std::size_t>(depth)]) < 0)
            break;

        occupied ^= fromSet;
        const Bitboard attackers =
            exchangeAttackers(position, move.to(), occupied) & occupied;
        side = opposite(side);
        fromSet = EMPTY_BB;
        for (int typeIndex = 0; typeIndex < PIECE_TYPE_NB; ++typeIndex) {
            const PieceType type = static_cast<PieceType>(typeIndex + 1);
            const Bitboard candidates = attackers & position.pieces(side, type);
            if (candidates) {
                fromSet = bit(lsb(candidates));
                attacker = type;
                break;
            }
        }
    } while (fromSet);

    while (--depth > 0) {
        gain[static_cast<std::size_t>(depth - 1)] = -std::max(
            -gain[static_cast<std::size_t>(depth - 1)],
            gain[static_cast<std::size_t>(depth)]);
    }
    return gain[0];
}

// Preserve the frozen reference's inexpensive ordering bonus. It is allowed
// to be approximate because it only ranks already-legal moves.
bool givesCheckForOrdering(const Position& position, Move move) noexcept {
    const Color us = position.sideToMove();
    const Square enemyKing = position.kingSquare(opposite(us));
    if (!isValid(enemyKing)) return false;

    const PieceType type = move.isPromotion()
        ? move.promotionType()
        : typeOf(position.pieceOn(move.from()));
    switch (type) {
    case PieceType::Pawn:
        return (Attacks::pawn(us, move.to()) & bit(enemyKing)) != EMPTY_BB;
    case PieceType::Knight:
        return (Attacks::knight(move.to()) & bit(enemyKing)) != EMPTY_BB;
    case PieceType::Bishop:
        return (Attacks::bishop(move.to(), position.occupied()) &
                bit(enemyKing)) != EMPTY_BB;
    case PieceType::Rook:
        return (Attacks::rook(move.to(), position.occupied()) &
                bit(enemyKing)) != EMPTY_BB;
    case PieceType::Queen:
        return (Attacks::queen(move.to(), position.occupied()) &
                bit(enemyKing)) != EMPTY_BB;
    default:
        return false;
    }
}

// Qsearch admission is a correctness decision rather than a score bonus, so
// use the post-move board. This covers discovered checks and occupancy changes.
bool givesCheckExact(Position& position, Move move) noexcept {
    StateInfo next;
    if (!position.doMove(move, next)) return false;
    const bool result = position.inCheck();
    position.undoMove(move);
    return result;
}

int attackBonus(Position& position, Move move) noexcept {
    if (givesCheckForOrdering(position, move)) return 300;
    const Square enemyKing = position.kingSquare(opposite(position.sideToMove()));
    return isValid(enemyKing) &&
           (Attacks::king(enemyKing) & bit(move.to())) != EMPTY_BB
        ? 60 : 0;
}

} // namespace

MovePicker::MovePicker(Position& position,
                       const HistoryTables& histories,
                       Move ttMove,
                       std::array<Move, 2> killers,
                       Move counterMove,
                       PickerMode mode,
                       int quiescencePly) noexcept
    : position_(position),
      histories_(histories),
      ttMove_(ttMove),
      killers_(killers),
      counterMove_(counterMove),
      mode_(mode),
      quiescencePly_(quiescencePly),
      quiescenceEvasion_(mode == PickerMode::Quiescence &&
                         position.inCheck()) {}

Move MovePicker::next() noexcept {
    while (stage_ != MoveStage::Done) {
        ensureGenerated(stage_);
        ScoredMove* best = bestInStage(stage_);
        if (best) {
            best->consumed = true;
            ++returned_;
            lastStage_ = stage_;
            return best->move;
        }
        stage_ = nextStage(stage_);
    }
    lastStage_ = MoveStage::Done;
    return Move::none();
}

void MovePicker::ensureGenerated(MoveStage stage) noexcept {
    const std::size_t stageIndex = static_cast<std::size_t>(stage);
    if (stageIndex >= generated_.size() || generated_[stageIndex]) return;
    generated_[stageIndex] = true;

    switch (stage) {
    case MoveStage::TT:
        if (eligible(ttMove_)) add(ttMove_, 1 << 22, MoveStage::TT);
        break;
    case MoveStage::GoodCapture:
        generateCaptures();
        break;
    case MoveStage::Killer1:
        if (mode_ != PickerMode::Quiescence)
            generateSpecial(killers_[0], MoveStage::Killer1);
        break;
    case MoveStage::Killer2:
        if (mode_ != PickerMode::Quiescence)
            generateSpecial(killers_[1], MoveStage::Killer2);
        break;
    case MoveStage::CounterMove:
        if (mode_ != PickerMode::Quiescence)
            generateSpecial(counterMove_, MoveStage::CounterMove);
        break;
    case MoveStage::Quiet:
        generateQuiets();
        break;
    case MoveStage::BadCapture:
    case MoveStage::Done:
        break;
    }
}

void MovePicker::generateCaptures() noexcept {
    MoveList pseudo;
    generateMoves(position_, pseudo, GenerationType::Captures);
    for (std::size_t index = 0; index < pseudo.size; ++index) {
        const Move move = pseudo[index];
        if (contains(move) || !eligible(move)) continue;

        const bool capture = isCapture(position_, move);
        const PieceType moved = typeOf(position_.pieceOn(move.from()));
        const PieceType captured = capturedType(position_, move);
        const int tacticalBonus = attackBonus(position_, move);
        int score = 0;
        MoveStage moveStage = MoveStage::GoodCapture;
        if (capture) {
            const int see = orderingSee(position_, move);
            score = (1 << 20) + pieceTypeIndex(captured) * 1000 -
                    pieceTypeIndex(moved) * 10 + see * 4 + tacticalBonus +
                    histories_.captureScore(position_.sideToMove(), moved,
                                            move.to(), captured);
            moveStage = see >= 0 ? MoveStage::GoodCapture
                                 : MoveStage::BadCapture;
        } else {
            // Preserve the frozen reference: a non-capture promotion is a
            // forcing/good move and does not receive a SEE term.
            score = (1 << 20) +
                    pieceTypeIndex(move.promotionType()) * 1000 +
                    tacticalBonus;
        }
        add(move, score, moveStage);
    }
}

void MovePicker::generateQuiets() noexcept {
    MoveList pseudo;
    generateMoves(position_, pseudo, GenerationType::All);
    for (std::size_t index = 0; index < pseudo.size; ++index) {
        const Move move = pseudo[index];
        if (contains(move) || isCapture(position_, move) || move.isPromotion() ||
            !eligible(move))
            continue;
        const int score = histories_.mainScore(position_.sideToMove(),
                                               move.from(), move.to()) +
                          attackBonus(position_, move);
        add(move, score, MoveStage::Quiet);
    }
}

void MovePicker::generateSpecial(Move candidate, MoveStage stage) noexcept {
    if (candidate.isNone() || contains(candidate) ||
        isCapture(position_, candidate) || candidate.isPromotion() ||
        !eligible(candidate))
        return;
    int score = histories_.mainScore(position_.sideToMove(),
                                     candidate.from(), candidate.to()) +
                attackBonus(position_, candidate);
    if (stage == MoveStage::Killer1) score += 1 << 19;
    else if (stage == MoveStage::Killer2) score += (1 << 19) - 1;
    add(candidate, score, stage);
}

void MovePicker::add(Move move, int score, MoveStage stage) noexcept {
    if (move.isNone() || contains(move) || size_ == moves_.size()) return;
    moves_[size_++] = ScoredMove{move, score, stage, false};
}

bool MovePicker::eligible(Move move) noexcept {
    if (move.isNone() || !position_.isLegal(move)) return false;
    if (mode_ != PickerMode::Quiescence || quiescenceEvasion_) return true;
    if (isCapture(position_, move) || move.isPromotion()) return true;
    return quiescencePly_ <= 3 && givesCheckExact(position_, move);
}

bool MovePicker::contains(Move move) const noexcept {
    if (move.isNone()) return false;
    for (std::size_t index = 0; index < size_; ++index)
        if (moves_[index].move == move) return true;
    return false;
}

MovePicker::ScoredMove* MovePicker::bestInStage(MoveStage stage) noexcept {
    ScoredMove* best = nullptr;
    for (std::size_t index = 0; index < size_; ++index) {
        ScoredMove& candidate = moves_[index];
        if (candidate.consumed || candidate.stage != stage) continue;
        if (!best || candidate.score > best->score) best = &candidate;
    }
    return best;
}

MoveStage MovePicker::nextStage(MoveStage stage) noexcept {
    switch (stage) {
    case MoveStage::TT:           return MoveStage::GoodCapture;
    case MoveStage::GoodCapture:  return MoveStage::Killer1;
    case MoveStage::Killer1:      return MoveStage::Killer2;
    case MoveStage::Killer2:      return MoveStage::CounterMove;
    case MoveStage::CounterMove:  return MoveStage::Quiet;
    case MoveStage::Quiet:        return MoveStage::BadCapture;
    case MoveStage::BadCapture:   return MoveStage::Done;
    case MoveStage::Done:         return MoveStage::Done;
    }
    return MoveStage::Done;
}

} // namespace Eunshin
