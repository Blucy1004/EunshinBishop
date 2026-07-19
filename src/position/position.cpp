#include "position/position.h"

#include "core/attacks.h"
#include "core/bitboard.h"
#include "core/zobrist.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>

namespace Eunshin {
namespace {

constexpr std::uint8_t castlingMaskForSquare(Square square) noexcept {
    switch (square) {
    case Square::A1: return static_cast<std::uint8_t>(AllCastling & ~WhiteQueenSide);
    case Square::E1: return static_cast<std::uint8_t>(AllCastling &
                                                      ~(WhiteKingSide | WhiteQueenSide));
    case Square::H1: return static_cast<std::uint8_t>(AllCastling & ~WhiteKingSide);
    case Square::A8: return static_cast<std::uint8_t>(AllCastling & ~BlackQueenSide);
    case Square::E8: return static_cast<std::uint8_t>(AllCastling &
                                                      ~(BlackKingSide | BlackQueenSide));
    case Square::H8: return static_cast<std::uint8_t>(AllCastling & ~BlackKingSide);
    default:         return AllCastling;
    }
}

constexpr bool isPromotionRank(Color color, Square square) noexcept {
    return rankOf(square) == (color == Color::White ? 7 : 0);
}

constexpr int pawnPush(Color color) noexcept {
    return color == Color::White ? 8 : -8;
}

constexpr int pawnStartRank(Color color) noexcept {
    return color == Color::White ? 1 : 6;
}

constexpr int saturatingIncrement(int value) noexcept {
    return value == std::numeric_limits<int>::max() ? value : value + 1;
}

constexpr Square castleRookFrom(Color color, Square kingTo) noexcept {
    if (color == Color::White)
        return kingTo == Square::G1 ? Square::H1 : Square::A1;
    return kingTo == Square::G8 ? Square::H8 : Square::A8;
}

constexpr Square castleRookTo(Color color, Square kingTo) noexcept {
    if (color == Color::White)
        return kingTo == Square::G1 ? Square::F1 : Square::D1;
    return kingTo == Square::G8 ? Square::F8 : Square::D8;
}

} // namespace

Piece Position::pieceOn(Square square) const noexcept {
    return isValid(square) ? board_[static_cast<std::size_t>(index(square))]
                           : Piece::None;
}

Bitboard Position::pieces(Color color) const noexcept {
    return isValid(color) ? byColor_[static_cast<std::size_t>(index(color))]
                          : EMPTY_BB;
}

Bitboard Position::pieces(PieceType type) const noexcept {
    const int typeIndex = pieceTypeIndex(type);
    return typeIndex >= 0 ? byType_[static_cast<std::size_t>(typeIndex)]
                          : EMPTY_BB;
}

Bitboard Position::pieces(Color color, PieceType type) const noexcept {
    return pieces(color) & pieces(type);
}

Square Position::kingSquare(Color color) const noexcept {
    return lsb(pieces(color, PieceType::King));
}

Bitboard Position::attackersTo(Square square, Color by) const noexcept {
    if (!isValid(square) || !isValid(by)) return EMPTY_BB;

    Bitboard attackers = EMPTY_BB;
    attackers |= Attacks::pawn(opposite(by), square) & pieces(by, PieceType::Pawn);
    attackers |= Attacks::knight(square) & pieces(by, PieceType::Knight);
    attackers |= Attacks::king(square) & pieces(by, PieceType::King);
    attackers |= Attacks::bishop(square, occupied_) &
                 (pieces(by, PieceType::Bishop) | pieces(by, PieceType::Queen));
    attackers |= Attacks::rook(square, occupied_) &
                 (pieces(by, PieceType::Rook) | pieces(by, PieceType::Queen));
    return attackers;
}

bool Position::isSquareAttacked(Square square, Color by) const noexcept {
    return attackersTo(square, by) != EMPTY_BB;
}

bool Position::inCheck() const noexcept {
    const Square king = kingSquare(sideToMove_);
    return isValid(king) && isSquareAttacked(king, opposite(sideToMove_));
}

Key Position::key() const noexcept {
    return state_ ? state_->positionKey : Key{0};
}

Key Position::pawnKey() const noexcept {
    return state_ ? state_->pawnKey : Key{0};
}

Key Position::materialKey() const noexcept {
    return state_ ? state_->materialKey : Key{0};
}

int Position::rule50() const noexcept {
    return state_ ? state_->rule50 : 0;
}

int Position::pliesFromNull() const noexcept {
    return state_ ? state_->pliesFromNull : 0;
}

int Position::fullmoveNumber() const noexcept {
    return state_ ? state_->fullmoveNumber : 1;
}

std::uint8_t Position::castlingRights() const noexcept {
    return state_ ? state_->castlingRights : static_cast<std::uint8_t>(NoCastling);
}

Square Position::epSquare() const noexcept {
    return state_ ? state_->epSquare : Square::None;
}

void Position::clear(StateInfo& rootState) noexcept {
    board_.fill(Piece::None);
    byColor_.fill(EMPTY_BB);
    byType_.fill(EMPTY_BB);
    occupied_ = EMPTY_BB;
    sideToMove_ = Color::White;
    rootState = StateInfo{};
    state_ = &rootState;
}

void Position::putPieceRaw(Piece piece, Square square) noexcept {
    assert(isValid(piece) && isValid(square));
    assert(pieceOn(square) == Piece::None);
    const Color color = colorOf(piece);
    const int typeIndex = pieceTypeIndex(typeOf(piece));
    const Bitboard squareBit = bit(square);
    board_[static_cast<std::size_t>(index(square))] = piece;
    byColor_[static_cast<std::size_t>(index(color))] |= squareBit;
    byType_[static_cast<std::size_t>(typeIndex)] |= squareBit;
    occupied_ |= squareBit;
}

void Position::removePieceRaw(Piece piece, Square square) noexcept {
    assert(isValid(piece) && isValid(square));
    assert(pieceOn(square) == piece);
    const Color color = colorOf(piece);
    const int typeIndex = pieceTypeIndex(typeOf(piece));
    const Bitboard squareBit = bit(square);
    board_[static_cast<std::size_t>(index(square))] = Piece::None;
    byColor_[static_cast<std::size_t>(index(color))] &= ~squareBit;
    byType_[static_cast<std::size_t>(typeIndex)] &= ~squareBit;
    occupied_ &= ~squareBit;
}

void Position::movePieceRaw(Piece piece, Square from, Square to) noexcept {
    assert(pieceOn(from) == piece && pieceOn(to) == Piece::None);
    removePieceRaw(piece, from);
    putPieceRaw(piece, to);
}

void Position::putPieceHashed(Piece piece, Square square) noexcept {
    assert(state_ != nullptr);
    const Color color = colorOf(piece);
    const PieceType type = typeOf(piece);
    const int before = popcount(pieces(color, type));
    state_->materialKey ^= Zobrist::materialCount(color, type, before);
    putPieceRaw(piece, square);
    state_->materialKey ^= Zobrist::materialCount(color, type, before + 1);
    state_->positionKey ^= Zobrist::piece(color, type, square);
    if (type == PieceType::Pawn)
        state_->pawnKey ^= Zobrist::piece(color, type, square);
}

void Position::removePieceHashed(Piece piece, Square square) noexcept {
    assert(state_ != nullptr);
    const Color color = colorOf(piece);
    const PieceType type = typeOf(piece);
    const int before = popcount(pieces(color, type));
    state_->materialKey ^= Zobrist::materialCount(color, type, before);
    removePieceRaw(piece, square);
    state_->materialKey ^= Zobrist::materialCount(color, type, before - 1);
    state_->positionKey ^= Zobrist::piece(color, type, square);
    if (type == PieceType::Pawn)
        state_->pawnKey ^= Zobrist::piece(color, type, square);
}

void Position::movePieceHashed(Piece piece, Square from, Square to) noexcept {
    assert(state_ != nullptr);
    const Color color = colorOf(piece);
    const PieceType type = typeOf(piece);
    movePieceRaw(piece, from, to);
    state_->positionKey ^= Zobrist::piece(color, type, from);
    state_->positionKey ^= Zobrist::piece(color, type, to);
    if (type == PieceType::Pawn) {
        state_->pawnKey ^= Zobrist::piece(color, type, from);
        state_->pawnKey ^= Zobrist::piece(color, type, to);
    }
}

std::uint8_t Position::updatedCastlingRights(Square from, Square to) const noexcept {
    return static_cast<std::uint8_t>(
        castlingRights() & castlingMaskForSquare(from) & castlingMaskForSquare(to));
}

bool Position::isPseudoLegal(Move move) const noexcept {
    if (!state_ || move.isNone() || !isValid(move.from()) || !isValid(move.to()) ||
        move.from() == move.to())
        return false;

    const Color us = sideToMove_;
    const Color them = opposite(us);
    const Piece moving = pieceOn(move.from());
    const Piece target = pieceOn(move.to());
    if (!isValid(moving) || colorOf(moving) != us) return false;
    if (isValid(target) && colorOf(target) == us) return false;
    if (typeOf(target) == PieceType::King) return false;

    const PieceType movingType = typeOf(moving);
    const Bitboard destination = bit(move.to());

    if (move.isCastle()) {
        if (movingType != PieceType::King || target != Piece::None) return false;
        const bool kingSide = fileOf(move.to()) > fileOf(move.from());
        const Square expectedFrom = us == Color::White ? Square::E1 : Square::E8;
        const Square expectedTo = us == Color::White
            ? (kingSide ? Square::G1 : Square::C1)
            : (kingSide ? Square::G8 : Square::C8);
        const std::uint8_t right = us == Color::White
            ? (kingSide ? WhiteKingSide : WhiteQueenSide)
            : (kingSide ? BlackKingSide : BlackQueenSide);
        if (move.from() != expectedFrom || move.to() != expectedTo ||
            !(castlingRights() & right))
            return false;

        const Square rookFrom = castleRookFrom(us, move.to());
        const Piece rook = makePiece(us, PieceType::Rook);
        if (pieceOn(rookFrom) != rook) return false;

        const Square transit = castleRookTo(us, move.to());
        if (pieceOn(transit) != Piece::None || pieceOn(move.to()) != Piece::None)
            return false;
        if (!kingSide) {
            const Square bFile = us == Color::White ? Square::B1 : Square::B8;
            if (pieceOn(bFile) != Piece::None) return false;
        }
        return !isSquareAttacked(move.from(), them) &&
               !isSquareAttacked(transit, them) &&
               !isSquareAttacked(move.to(), them);
    }

    if (move.isEnPassant()) {
        if (movingType != PieceType::Pawn || target != Piece::None ||
            move.to() != epSquare() ||
            !(Attacks::pawn(us, move.from()) & destination))
            return false;
        const Square capturedSquare = makeSquare(
            fileOf(move.to()), rankOf(move.to()) + (us == Color::White ? -1 : 1));
        return pieceOn(capturedSquare) == makePiece(them, PieceType::Pawn);
    }

    if (move.isPromotion()) {
        if (movingType != PieceType::Pawn || !isPromotionRank(us, move.to()))
            return false;
        const int delta = index(move.to()) - index(move.from());
        const int push = pawnPush(us);
        if (delta == push) return target == Piece::None;
        return (Attacks::pawn(us, move.from()) & destination) &&
               isValid(target) && colorOf(target) == them;
    }

    if (move.type() != MoveType::Normal) return false;

    switch (movingType) {
    case PieceType::Pawn: {
        if (isPromotionRank(us, move.to())) return false;
        const int delta = index(move.to()) - index(move.from());
        const int push = pawnPush(us);
        if (delta == push) return target == Piece::None;
        if (delta == 2 * push && rankOf(move.from()) == pawnStartRank(us) &&
            target == Piece::None) {
            const Square middle = static_cast<Square>(index(move.from()) + push);
            return pieceOn(middle) == Piece::None;
        }
        return (Attacks::pawn(us, move.from()) & destination) &&
               isValid(target) && colorOf(target) == them;
    }
    case PieceType::Knight:
        return (Attacks::knight(move.from()) & destination) != EMPTY_BB;
    case PieceType::Bishop:
        return (Attacks::bishop(move.from(), occupied_) & destination) != EMPTY_BB;
    case PieceType::Rook:
        return (Attacks::rook(move.from(), occupied_) & destination) != EMPTY_BB;
    case PieceType::Queen:
        return (Attacks::queen(move.from(), occupied_) & destination) != EMPTY_BB;
    case PieceType::King:
        return (Attacks::king(move.from()) & destination) != EMPTY_BB;
    default:
        return false;
    }
}

bool Position::isLegal(Move move) const noexcept {
    if (!state_) return false;
    Position probe = *this;
    StateInfo rootCopy = *state_;
    probe.state_ = &rootCopy;
    StateInfo next;
    return probe.doMove(move, next);
}

Position Position::snapshotForSearch(StateInfo& rootState) const noexcept {
    Position snapshot = *this;
    if (!state_) {
        rootState = StateInfo{};
        snapshot.state_ = nullptr;
        return snapshot;
    }

    rootState = *state_;
    snapshot.state_ = &rootState;
    return snapshot;
}

bool Position::doMove(Move move, StateInfo& newState) noexcept {
    if (!state_ || &newState == state_ || !isPseudoLegal(move)) return false;

    StateInfo* const oldState = state_;
    const Color us = sideToMove_;
    const Color them = opposite(us);
    const Piece moving = pieceOn(move.from());

    Piece captured = Piece::None;
    Square capturedSquare = Square::None;
    if (move.isEnPassant()) {
        capturedSquare = makeSquare(fileOf(move.to()),
                                    rankOf(move.to()) +
                                    (us == Color::White ? -1 : 1));
        captured = pieceOn(capturedSquare);
    } else if (isValid(pieceOn(move.to()))) {
        capturedSquare = move.to();
        captured = pieceOn(move.to());
    }

    newState = StateInfo{};
    newState.previous = oldState;
    newState.positionKey = oldState->positionKey;
    newState.pawnKey = oldState->pawnKey;
    newState.materialKey = oldState->materialKey;
    newState.rule50 = saturatingIncrement(oldState->rule50);
    newState.pliesFromNull = saturatingIncrement(oldState->pliesFromNull);
    newState.fullmoveNumber = us == Color::Black
        ? saturatingIncrement(oldState->fullmoveNumber)
        : oldState->fullmoveNumber;
    newState.castlingRights = oldState->castlingRights;
    newState.epSquare = Square::None;
    newState.move = move;
    newState.movedPiece = moving;
    newState.capturedPiece = captured;
    newState.capturedSquare = capturedSquare;
    newState.nullMove = false;
    // Preserve the accepted checkpoint-2 state-transition contract. The
    // evaluator can update this copied storage incrementally from `previous`.
    newState.accumulator = oldState->accumulator;
    if (newState.accumulator.validMask != 0)
        newState.accumulator.validMask = 0;

    state_ = &newState;

    newState.positionKey ^= Zobrist::castling(oldState->castlingRights);
    if (isValid(oldState->epSquare))
        newState.positionKey ^= Zobrist::enPassantSquare(oldState->epSquare);

    if (isValid(captured)) removePieceHashed(captured, capturedSquare);

    if (move.isCastle()) {
        movePieceHashed(moving, move.from(), move.to());
        const Square rookFrom = castleRookFrom(us, move.to());
        const Square rookTo = castleRookTo(us, move.to());
        movePieceHashed(makePiece(us, PieceType::Rook), rookFrom, rookTo);
    } else if (move.isPromotion()) {
        removePieceHashed(moving, move.from());
        putPieceHashed(makePiece(us, move.promotionType()), move.to());
    } else {
        movePieceHashed(moving, move.from(), move.to());
    }

    if (typeOf(moving) == PieceType::Pawn || isValid(captured))
        newState.rule50 = 0;

    newState.castlingRights = updatedCastlingRights(move.from(), move.to());
    if (typeOf(moving) == PieceType::Pawn && move.type() == MoveType::Normal &&
        std::abs(index(move.to()) - index(move.from())) == 16) {
        newState.epSquare = static_cast<Square>(
            (index(move.from()) + index(move.to())) / 2);
    }

    newState.positionKey ^= Zobrist::castling(newState.castlingRights);
    if (isValid(newState.epSquare))
        newState.positionKey ^= Zobrist::enPassantSquare(newState.epSquare);

    sideToMove_ = them;
    newState.positionKey ^= Zobrist::side();

    const Square ownKing = kingSquare(us);
    if (!isValid(ownKing) || isSquareAttacked(ownKing, them)) {
        undoMove(move);
        return false;
    }

    const Square nextKing = kingSquare(sideToMove_);
    newState.checkers = attackersTo(nextKing, us);

#ifndef NDEBUG
    assert(isConsistent());
#endif
    return true;
}

void Position::undoMove(Move move) noexcept {
    assert(state_ != nullptr && state_->previous != nullptr);
    if (!state_ || !state_->previous || state_->nullMove || state_->move != move)
        return;

    StateInfo* const current = state_;
    const Color us = opposite(sideToMove_);

    if (move.isCastle()) {
        movePieceRaw(current->movedPiece, move.to(), move.from());
        const Square rookFrom = castleRookFrom(us, move.to());
        const Square rookTo = castleRookTo(us, move.to());
        movePieceRaw(makePiece(us, PieceType::Rook), rookTo, rookFrom);
    } else if (move.isPromotion()) {
        removePieceRaw(makePiece(us, move.promotionType()), move.to());
        putPieceRaw(current->movedPiece, move.from());
    } else {
        movePieceRaw(current->movedPiece, move.to(), move.from());
    }

    if (isValid(current->capturedPiece))
        putPieceRaw(current->capturedPiece, current->capturedSquare);

    sideToMove_ = us;
    state_ = current->previous;

#ifndef NDEBUG
    assert(isConsistent());
#endif
}

bool Position::doNullMove(StateInfo& newState) noexcept {
    if (!state_ || &newState == state_ || inCheck()) return false;

    StateInfo* const oldState = state_;
    newState = *oldState;
    newState.previous = oldState;
    newState.move = Move::none();
    newState.movedPiece = Piece::None;
    newState.capturedPiece = Piece::None;
    newState.capturedSquare = Square::None;
    newState.nullMove = true;
    newState.pliesFromNull = 0;
    newState.checkers = EMPTY_BB;

    if (isValid(oldState->epSquare))
        newState.positionKey ^= Zobrist::enPassantSquare(oldState->epSquare);
    newState.epSquare = Square::None;
    newState.positionKey ^= Zobrist::side();

    state_ = &newState;
    sideToMove_ = opposite(sideToMove_);
    const Square nextKing = kingSquare(sideToMove_);
    newState.checkers = attackersTo(nextKing, opposite(sideToMove_));

#ifndef NDEBUG
    assert(isConsistent());
#endif
    return true;
}

void Position::undoNullMove() noexcept {
    assert(state_ != nullptr && state_->previous != nullptr && state_->nullMove);
    if (!state_ || !state_->previous || !state_->nullMove) return;
    sideToMove_ = opposite(sideToMove_);
    state_ = state_->previous;
#ifndef NDEBUG
    assert(isConsistent());
#endif
}

bool Position::isRepetition(int requiredPriorOccurrences) const noexcept {
    if (!state_ || requiredPriorOccurrences <= 0) return false;
    const int limit = std::min(state_->rule50, state_->pliesFromNull);
    if (limit < 2) return false;

    int distance = 0;
    int found = 0;
    const StateInfo* cursor = state_->previous;
    while (cursor && distance < limit) {
        ++distance;
        if ((distance & 1) == 0 && cursor->positionKey == state_->positionKey &&
            ++found >= requiredPriorOccurrences)
            return true;
        cursor = cursor->previous;
    }
    return false;
}

Key Position::recomputePositionKey() const noexcept {
    Key result = 0;
    for (int squareIndex = 0; squareIndex < SQUARE_NB; ++squareIndex) {
        const Square square = static_cast<Square>(squareIndex);
        const Piece piece = pieceOn(square);
        if (isValid(piece))
            result ^= Zobrist::piece(colorOf(piece), typeOf(piece), square);
    }
    if (sideToMove_ == Color::Black) result ^= Zobrist::side();
    result ^= Zobrist::castling(castlingRights());
    if (isValid(epSquare())) result ^= Zobrist::enPassantSquare(epSquare());
    return result;
}

Key Position::recomputePawnKey() const noexcept {
    Key result = 0;
    for (Color color : {Color::White, Color::Black}) {
        Bitboard pawns = pieces(color, PieceType::Pawn);
        while (pawns) {
            const Square square = popLsb(pawns);
            result ^= Zobrist::piece(color, PieceType::Pawn, square);
        }
    }
    return result;
}

Key Position::recomputeMaterialKey() const noexcept {
    Key result = 0;
    for (Color color : {Color::White, Color::Black}) {
        for (int typeIndex = 0; typeIndex < PIECE_TYPE_NB; ++typeIndex) {
            const PieceType type = static_cast<PieceType>(typeIndex + 1);
            result ^= Zobrist::materialCount(color, type,
                                             popcount(pieces(color, type)));
        }
    }
    return result;
}

bool Position::isConsistent() const noexcept {
    if (!state_ || !isValid(sideToMove_) ||
        (state_->castlingRights & ~static_cast<std::uint8_t>(AllCastling)) != 0)
        return false;

    std::array<Bitboard, 2> expectedColor{};
    std::array<Bitboard, 6> expectedType{};
    for (int squareIndex = 0; squareIndex < SQUARE_NB; ++squareIndex) {
        const Square square = static_cast<Square>(squareIndex);
        const Piece piece = board_[static_cast<std::size_t>(squareIndex)];
        if (!isValid(piece)) {
            if (piece != Piece::None) return false;
            continue;
        }
        const Color color = colorOf(piece);
        const int typeIndex = pieceTypeIndex(typeOf(piece));
        expectedColor[static_cast<std::size_t>(index(color))] |= bit(square);
        expectedType[static_cast<std::size_t>(typeIndex)] |= bit(square);
    }

    if (expectedColor != byColor_ || expectedType != byType_ ||
        occupied_ != (byColor_[0] | byColor_[1]) ||
        (byColor_[0] & byColor_[1]) != EMPTY_BB)
        return false;
    if (!hasSingleBit(pieces(Color::White, PieceType::King)) ||
        !hasSingleBit(pieces(Color::Black, PieceType::King)))
        return false;
    if (isValid(epSquare()) && rankOf(epSquare()) != 2 && rankOf(epSquare()) != 5)
        return false;
    if (state_->positionKey != recomputePositionKey() ||
        state_->pawnKey != recomputePawnKey() ||
        state_->materialKey != recomputeMaterialKey())
        return false;
    const Bitboard expectedCheckers =
        attackersTo(kingSquare(sideToMove_), opposite(sideToMove_));
    return state_->checkers == expectedCheckers;
}

std::string Position::consistencyError() const {
    if (!state_) return "Position has no StateInfo";
    if (!isValid(sideToMove_)) return "invalid side to move";
    if (!hasSingleBit(pieces(Color::White, PieceType::King)))
        return "white king count is not one";
    if (!hasSingleBit(pieces(Color::Black, PieceType::King)))
        return "black king count is not one";
    if (occupied_ != (byColor_[0] | byColor_[1]))
        return "occupied bitboard differs from color occupancy";
    if (byColor_[0] & byColor_[1]) return "color occupancies overlap";
    if (state_->positionKey != recomputePositionKey())
        return "position Zobrist key mismatch";
    if (state_->pawnKey != recomputePawnKey()) return "pawn key mismatch";
    if (state_->materialKey != recomputeMaterialKey()) return "material key mismatch";
    if (!isConsistent()) return "mailbox and piece bitboards disagree";
    return {};
}

bool Position::sameBoard(const Position& other) const noexcept {
    if (board_ != other.board_ || byColor_ != other.byColor_ ||
        byType_ != other.byType_ || occupied_ != other.occupied_ ||
        sideToMove_ != other.sideToMove_)
        return false;
    if (!state_ || !other.state_) return state_ == other.state_;
    return state_->positionKey == other.state_->positionKey &&
           state_->pawnKey == other.state_->pawnKey &&
           state_->materialKey == other.state_->materialKey &&
           state_->rule50 == other.state_->rule50 &&
           state_->pliesFromNull == other.state_->pliesFromNull &&
           state_->fullmoveNumber == other.state_->fullmoveNumber &&
           state_->castlingRights == other.state_->castlingRights &&
           state_->epSquare == other.state_->epSquare &&
           state_->checkers == other.state_->checkers &&
           state_->accumulator == other.state_->accumulator;
}

} // namespace Eunshin
