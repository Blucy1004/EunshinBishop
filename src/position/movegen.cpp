#include "position/movegen.h"

#include "core/attacks.h"
#include "core/bitboard.h"
#include "position/position.h"

#include <cctype>

namespace Eunshin {
namespace {

void addPromotions(MoveList& list, Square from, Square to) noexcept {
    list.add(Move(from, to, MoveType::PromotionQueen));
    list.add(Move(from, to, MoveType::PromotionRook));
    list.add(Move(from, to, MoveType::PromotionBishop));
    list.add(Move(from, to, MoveType::PromotionKnight));
}

MoveType promotionTypeFromSuffix(char suffix) noexcept {
    switch (static_cast<char>(std::tolower(static_cast<unsigned char>(suffix)))) {
    case 'n': return MoveType::PromotionKnight;
    case 'b': return MoveType::PromotionBishop;
    case 'r': return MoveType::PromotionRook;
    case 'q': return MoveType::PromotionQueen;
    default: return MoveType::None;
    }
}

} // namespace

void generateMoves(const Position& position,
                   MoveList& list,
                   GenerationType type) noexcept {
    list.clear();
    const Color us = position.sideToMove();
    const Color them = opposite(us);
    const Bitboard own = position.pieces(us);
    const Bitboard enemyWithoutKing =
        position.pieces(them) & ~position.pieces(them, PieceType::King);
    const Bitboard targets = type == GenerationType::Captures
        ? enemyWithoutKing
        : (~own & ~position.pieces(them, PieceType::King));
    const Bitboard occupied = position.occupied();

    Bitboard pawns = position.pieces(us, PieceType::Pawn);
    const int push = us == Color::White ? 8 : -8;
    const int startRank = us == Color::White ? 1 : 6;
    const int promotionRank = us == Color::White ? 7 : 0;
    while (pawns) {
        const Square from = popLsb(pawns);
        Bitboard captures = Attacks::pawn(us, from) & enemyWithoutKing;
        while (captures) {
            const Square to = popLsb(captures);
            if (rankOf(to) == promotionRank) addPromotions(list, from, to);
            else list.add(Move(from, to, MoveType::Normal));
        }

        if (isValid(position.epSquare()) &&
            (Attacks::pawn(us, from) & bit(position.epSquare()))) {
            const Move ep(from, position.epSquare(), MoveType::EnPassant);
            if (position.isPseudoLegal(ep)) list.add(ep);
        }

        const int toIndex = index(from) + push;
        if (toIndex < 0 || toIndex >= SQUARE_NB) continue;
        const Square to = static_cast<Square>(toIndex);
        if (position.pieceOn(to) != Piece::None) continue;

        if (rankOf(to) == promotionRank) {
            // Quiet promotions belong in capture/quiescence generation too.
            addPromotions(list, from, to);
        } else if (type == GenerationType::All) {
            list.add(Move(from, to, MoveType::Normal));
            if (rankOf(from) == startRank) {
                const Square doubleTo = static_cast<Square>(index(from) + 2 * push);
                if (position.pieceOn(doubleTo) == Piece::None)
                    list.add(Move(from, doubleTo, MoveType::Normal));
            }
        }
    }

    auto generateLeapers = [&](PieceType pieceType, auto attacks) noexcept {
        Bitboard pieces = position.pieces(us, pieceType);
        while (pieces) {
            const Square from = popLsb(pieces);
            Bitboard destinations = attacks(from) & targets;
            while (destinations)
                list.add(Move(from, popLsb(destinations), MoveType::Normal));
        }
    };

    generateLeapers(PieceType::Knight,
                    [](Square square) noexcept { return Attacks::knight(square); });

    Bitboard bishops = position.pieces(us, PieceType::Bishop);
    while (bishops) {
        const Square from = popLsb(bishops);
        Bitboard destinations = Attacks::bishop(from, occupied) & targets;
        while (destinations)
            list.add(Move(from, popLsb(destinations), MoveType::Normal));
    }

    Bitboard rooks = position.pieces(us, PieceType::Rook);
    while (rooks) {
        const Square from = popLsb(rooks);
        Bitboard destinations = Attacks::rook(from, occupied) & targets;
        while (destinations)
            list.add(Move(from, popLsb(destinations), MoveType::Normal));
    }

    Bitboard queens = position.pieces(us, PieceType::Queen);
    while (queens) {
        const Square from = popLsb(queens);
        Bitboard destinations = Attacks::queen(from, occupied) & targets;
        while (destinations)
            list.add(Move(from, popLsb(destinations), MoveType::Normal));
    }

    generateLeapers(PieceType::King,
                    [](Square square) noexcept { return Attacks::king(square); });

    if (type == GenerationType::All) {
        const Square kingFrom = us == Color::White ? Square::E1 : Square::E8;
        const Square kingTo = us == Color::White ? Square::G1 : Square::G8;
        const Move kingSide(kingFrom, kingTo, MoveType::Castle);
        if (position.isPseudoLegal(kingSide)) list.add(kingSide);

        const Square queenTo = us == Color::White ? Square::C1 : Square::C8;
        const Move queenSide(kingFrom, queenTo, MoveType::Castle);
        if (position.isPseudoLegal(queenSide)) list.add(queenSide);
    }
}

void generateLegalMoves(Position& position, MoveList& list) noexcept {
    MoveList pseudo;
    generateMoves(position, pseudo, GenerationType::All);
    list.clear();
    for (std::size_t i = 0; i < pseudo.size; ++i) {
        StateInfo next;
        const Move move = pseudo[i];
        if (!position.doMove(move, next, false)) continue;
        position.undoMove(move);
        list.add(move);
    }
}

Move moveFromUci(const Position& position, std::string_view text) noexcept {
    if (text.size() != 4 && text.size() != 5) return Move::none();
    const char fromFile = static_cast<char>(std::tolower(
        static_cast<unsigned char>(text[0])));
    const char toFile = static_cast<char>(std::tolower(
        static_cast<unsigned char>(text[2])));
    if (fromFile < 'a' || fromFile > 'h' || toFile < 'a' || toFile > 'h' ||
        text[1] < '1' || text[1] > '8' || text[3] < '1' || text[3] > '8')
        return Move::none();

    const Square from = makeSquare(fromFile - 'a', text[1] - '1');
    const Square to = makeSquare(toFile - 'a', text[3] - '1');
    const MoveType requestedPromotion = text.size() == 5
        ? promotionTypeFromSuffix(text[4]) : MoveType::None;
    if (text.size() == 5 && requestedPromotion == MoveType::None)
        return Move::none();

    MoveList pseudo;
    generateMoves(position, pseudo, GenerationType::All);
    for (std::size_t i = 0; i < pseudo.size; ++i) {
        const Move move = pseudo[i];
        if (move.from() != from || move.to() != to) continue;
        if (text.size() == 5) {
            if (move.type() != requestedPromotion) continue;
        } else if (move.isPromotion()) {
            continue;
        }
        if (position.isLegal(move)) return move;
    }
    return Move::none();
}

std::uint64_t perft(Position& position, Depth depth) noexcept {
    if (depth <= 0) return 1;
    MoveList pseudo;
    generateMoves(position, pseudo, GenerationType::All);
    std::uint64_t nodes = 0;
    for (std::size_t i = 0; i < pseudo.size; ++i) {
        const Move move = pseudo[i];
        StateInfo next;
        if (!position.doMove(move, next, false)) continue;
        nodes += perft(position, depth - 1);
        position.undoMove(move);
    }
    return nodes;
}

} // namespace Eunshin
