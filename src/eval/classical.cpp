#include "eval/classical.h"

#include "core/attacks.h"
#include "core/bitboard.h"
#include "position/position.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace Eunshin {
namespace {

constexpr int PIECE_VALUE[PIECE_TYPE_NB] = {100, 320, 330, 500, 900, 0};
constexpr int PHASE_WEIGHT[PIECE_TYPE_NB] = {0, 1, 1, 2, 4, 0};

// Tables are in visual order (index 0 = a8).  White therefore uses square^56.
constexpr int PST_PAWN[SQUARE_NB] = {
      0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,
      5,  5, 10, 25, 25, 10,  5,  5,
      0,  0,  0, 20, 20,  0,  0,  0,
      5, -5,-10,  0,  0,-10, -5,  5,
      5, 10, 10,-20,-20, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0
};
constexpr int PST_KNIGHT[SQUARE_NB] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};
constexpr int PST_BISHOP[SQUARE_NB] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};
constexpr int PST_ROOK[SQUARE_NB] = {
      0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      0,  0,  0,  5,  5,  0,  0,  0
};
constexpr int PST_QUEEN[SQUARE_NB] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};
constexpr int PST_KING_MG[SQUARE_NB] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
};
constexpr int PST_KING_EG[SQUARE_NB] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50
};
constexpr const int* PST[5] = {
    PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN
};
constexpr int PASSED_BONUS[8] = {0, 5, 10, 20, 35, 60, 100, 0};

[[nodiscard]] constexpr Bitboard adjacentFiles(int file) noexcept {
    return (file > 0 ? fileMask(file - 1) : EMPTY_BB)
         | (file < 7 ? fileMask(file + 1) : EMPTY_BB);
}

[[nodiscard]] Bitboard passedPawnMask(Color color, Square square) noexcept {
    const int rank = rankOf(square);
    const int file = fileOf(square);
    const Bitboard files = fileMask(file) | adjacentFiles(file);
    Bitboard result = EMPTY_BB;
    if (color == Color::White) {
        for (int r = rank + 1; r < 8; ++r)
            result |= files & rankMask(r);
    } else {
        for (int r = rank - 1; r >= 0; --r)
            result |= files & rankMask(r);
    }
    return result;
}

[[nodiscard]] int kingDistance(Square first, Square second) noexcept {
    return std::max(std::abs(rankOf(first) - rankOf(second)),
                    std::abs(fileOf(first) - fileOf(second)));
}

constexpr void addSigned(Score& score, int sign, int mg, int eg) noexcept {
    score.mg += sign * mg;
    score.eg += sign * eg;
}

[[nodiscard]] constexpr int visualIndex(Color color, Square square) noexcept {
    return color == Color::White ? index(square) ^ 56 : index(square);
}

} // namespace

Value ClassicalEvaluator::evaluate(
    const Position& position, ClassicalBreakdown* breakdown) const noexcept {
    ClassicalBreakdown result{};

    if (!isValid(position.kingSquare(Color::White)) ||
        !isValid(position.kingSquare(Color::Black))) {
        if (breakdown)
            *breakdown = result;
        return 0;
    }

    for (int colorIndex = 0; colorIndex < COLOR_NB; ++colorIndex) {
        const Color color = static_cast<Color>(colorIndex);
        const Color opponent = opposite(color);
        const int sign = color == Color::White ? 1 : -1;
        const Bitboard own = position.pieces(color);

        for (PieceType type = PieceType::Pawn;
             type <= PieceType::Queen;
             type = static_cast<PieceType>(index(type) + 1)) {
            Bitboard pieces = position.pieces(color, type);
            const int typeIndex = pieceTypeIndex(type);
            result.phase += PHASE_WEIGHT[typeIndex] * popcount(pieces);
            while (pieces != EMPTY_BB) {
                const Square square = popLsb(pieces);
                addSigned(result.material, sign,
                          PIECE_VALUE[typeIndex], PIECE_VALUE[typeIndex]);
                const int psqt = PST[typeIndex][visualIndex(color, square)];
                addSigned(result.psqt, sign, psqt, psqt);
            }
        }

        const Square myKing = position.kingSquare(color);
        const Square opponentKing = position.kingSquare(opponent);
        addSigned(result.psqt, sign,
                  PST_KING_MG[visualIndex(color, myKing)],
                  PST_KING_EG[visualIndex(color, myKing)]);

        if (popcount(position.pieces(color, PieceType::Bishop)) >= 2)
            addSigned(result.miscellaneous, sign, 30, 40);

        const Bitboard myPawns = position.pieces(color, PieceType::Pawn);
        const Bitboard opponentPawns = position.pieces(opponent, PieceType::Pawn);
        const Bitboard opponentPawnAttacks = color == Color::White
            ? (((opponentPawns >> 7) & ~FILE_A)
             | ((opponentPawns >> 9) & ~FILE_H))
            : (((opponentPawns << 7) & ~FILE_H)
             | ((opponentPawns << 9) & ~FILE_A));
        const Bitboard nonPawnPieces = position.occupied()
            & ~position.pieces(Color::White, PieceType::Pawn)
            & ~position.pieces(Color::Black, PieceType::Pawn)
            & ~position.pieces(Color::White, PieceType::King)
            & ~position.pieces(Color::Black, PieceType::King);
        const bool pawnRaceLike = popcount(nonPawnPieces) <= 2;

        Bitboard pawns = myPawns;
        while (pawns != EMPTY_BB) {
            const Square square = popLsb(pawns);
            const int file = fileOf(square);
            const int rank = rankOf(square);
            const int relativeRank = color == Color::White ? rank : 7 - rank;

            if (popcount(myPawns & fileMask(file)) > 1)
                addSigned(result.pawns, sign, -8, -12);
            if ((myPawns & adjacentFiles(file)) == EMPTY_BB)
                addSigned(result.pawns, sign, -10, -14);

            if ((passedPawnMask(color, square) & opponentPawns) == EMPTY_BB) {
                addSigned(result.passedPawns, sign,
                          PASSED_BONUS[relativeRank] / 2,
                          PASSED_BONUS[relativeRank]);

                if ((Attacks::pawn(opponent, square) & myPawns) != EMPTY_BB)
                    addSigned(result.passedPawns, sign, 8, 18);
                if ((myPawns & adjacentFiles(file) & rankMask(rank)) != EMPTY_BB)
                    addSigned(result.passedPawns, sign, 10, 20);

                if (relativeRank >= 3) {
                    const Square stopSquare = static_cast<Square>(
                        index(square) + (color == Color::White ? 8 : -8));
                    addSigned(result.passedPawns, sign, 0,
                              kingDistance(opponentKing, stopSquare) * 5
                            - kingDistance(myKing, stopSquare) * 3);
                }

                if (pawnRaceLike && relativeRank >= 2) {
                    const Square promotionSquare = color == Color::White
                        ? makeSquare(file, 7)
                        : makeSquare(file, 0);
                    const int pawnSteps = 7 - relativeRank;
                    const int opponentSteps = kingDistance(opponentKing, promotionSquare);
                    const int tempoEdge = position.sideToMove() == color ? 1 : 0;
                    if (opponentSteps > pawnSteps + tempoEdge)
                        addSigned(result.passedPawns, sign, 0, 90);
                }
            }
        }

        Bitboard rooks = position.pieces(color, PieceType::Rook);
        while (rooks != EMPTY_BB) {
            const Square square = popLsb(rooks);
            const int file = fileOf(square);
            const int relativeRank = color == Color::White
                ? rankOf(square) : 7 - rankOf(square);

            if ((myPawns & fileMask(file)) == EMPTY_BB) {
                if ((opponentPawns & fileMask(file)) == EMPTY_BB)
                    addSigned(result.miscellaneous, sign, 20, 10);
                else
                    addSigned(result.miscellaneous, sign, 10, 5);
            }

            Bitboard filePassers = fileMask(file) & (myPawns | opponentPawns);
            while (filePassers != EMPTY_BB) {
                const Square pawnSquare = popLsb(filePassers);
                const Piece pawn = position.pieceOn(pawnSquare);
                const Color pawnColor = colorOf(pawn);
                if (!isValid(pawnColor))
                    continue;
                if ((passedPawnMask(pawnColor, pawnSquare)
                     & position.pieces(opposite(pawnColor), PieceType::Pawn)) == EMPTY_BB) {
                    const bool rookBehind = pawnColor == Color::White
                        ? index(square) < index(pawnSquare)
                        : index(square) > index(pawnSquare);
                    if (rookBehind)
                        addSigned(result.passedPawns, sign, 0,
                                  pawnColor == color ? 20 : 15);
                }
            }

            if (relativeRank == 6) {
                const int opponentKingRank = color == Color::White
                    ? rankOf(opponentKing) : 7 - rankOf(opponentKing);
                const Bitboard seventh = color == Color::White ? RANK_7 : RANK_2;
                if (opponentKingRank == 7 || (opponentPawns & seventh) != EMPTY_BB)
                    addSigned(result.miscellaneous, sign, 20, 30);
            }

            const int mobility = popcount(
                Attacks::rook(square, position.occupied()) & ~own);
            addSigned(result.mobility, sign, mobility * 2, mobility * 3);
        }

        Bitboard knights = position.pieces(color, PieceType::Knight);
        while (knights != EMPTY_BB) {
            const Square square = popLsb(knights);
            const int mobility = popcount(Attacks::knight(square) & ~own);
            addSigned(result.mobility, sign, mobility * 3, mobility * 3);
            const int file = fileOf(square);
            const int relativeRank = color == Color::White
                ? rankOf(square) : 7 - rankOf(square);
            if (relativeRank >= 3 && relativeRank <= 5
                && (Attacks::pawn(opponent, square) & myPawns) != EMPTY_BB
                && (passedPawnMask(color, square)
                    & adjacentFiles(file) & opponentPawns) == EMPTY_BB) {
                addSigned(result.miscellaneous, sign, 22, 12);
            }
        }

        Bitboard bishops = position.pieces(color, PieceType::Bishop);
        while (bishops != EMPTY_BB) {
            const Square square = popLsb(bishops);
            const int mobility = popcount(
                Attacks::bishop(square, position.occupied()) & ~own);
            addSigned(result.mobility, sign, mobility * 3, mobility * 3);
        }

        Bitboard queens = position.pieces(color, PieceType::Queen);
        while (queens != EMPTY_BB) {
            const Square square = popLsb(queens);
            const int mobility = popcount(
                Attacks::queen(square, position.occupied()) & ~own);
            addSigned(result.mobility, sign, mobility, mobility * 2);
        }

        constexpr int THREAT_PENALTY[PIECE_TYPE_NB] = {0, 28, 28, 45, 70, 0};
        constexpr int QUEEN_THREAT_PENALTY = 110;
        Bitboard threatened = opponentPawnAttacks & own
            & ~myPawns & ~position.pieces(color, PieceType::King);
        while (threatened != EMPTY_BB) {
            const Piece piece = position.pieceOn(popLsb(threatened));
            const PieceType type = typeOf(piece);
            const int penalty = type == PieceType::Queen
                ? QUEEN_THREAT_PENALTY
                : THREAT_PENALTY[pieceTypeIndex(type)];
            addSigned(result.threats, sign, -penalty, -penalty * 2 / 3);
        }

        const Bitboard ownQueens = position.pieces(color, PieceType::Queen);
        if (ownQueens != EMPTY_BB) {
            const Square queenSquare = lsb(ownQueens);
            Bitboard minorAttacks = EMPTY_BB;
            Bitboard enemyKnights = position.pieces(opponent, PieceType::Knight);
            while (enemyKnights != EMPTY_BB)
                minorAttacks |= Attacks::knight(popLsb(enemyKnights));
            Bitboard enemyBishops = position.pieces(opponent, PieceType::Bishop);
            while (enemyBishops != EMPTY_BB)
                minorAttacks |= Attacks::bishop(
                    popLsb(enemyBishops), position.occupied());
            if ((minorAttacks & bit(queenSquare)) != EMPTY_BB)
                addSigned(result.threats, sign, -140, -90);
        }

        const int homeRankBase = color == Color::White ? 0 : 56;
        const Square queenHome = static_cast<Square>(homeRankBase + 3);
        if ((position.pieces(color, PieceType::Queen) & bit(queenHome)) == EMPTY_BB) {
            const Bitboard homeMinors = bit(static_cast<Square>(homeRankBase + 1))
                | bit(static_cast<Square>(homeRankBase + 2))
                | bit(static_cast<Square>(homeRankBase + 5))
                | bit(static_cast<Square>(homeRankBase + 6));
            const int undeveloped = popcount(
                (position.pieces(color, PieceType::Knight)
                 | position.pieces(color, PieceType::Bishop)) & homeMinors);
            if (undeveloped >= 2)
                addSigned(result.miscellaneous, sign, -16 * undeveloped, 0);
        }

        const std::uint8_t kingSideRight = color == Color::White
            ? WhiteKingSide : BlackKingSide;
        const std::uint8_t queenSideRight = color == Color::White
            ? WhiteQueenSide : BlackQueenSide;
        const Bitboard pawnHomeRank = color == Color::White ? RANK_2 : RANK_7;
        if ((position.castlingRights() & kingSideRight) != 0) {
            const Bitboard kingSideFiles = FILE_F | FILE_G | FILE_H;
            const int moved = 3 - popcount(myPawns & pawnHomeRank & kingSideFiles);
            addSigned(result.kingSafety, sign, -moved * 10, 0);
        }
        if ((position.castlingRights() & queenSideRight) != 0) {
            const Bitboard queenSideFiles = FILE_A | FILE_B | FILE_C;
            const int moved = 3 - popcount(myPawns & pawnHomeRank & queenSideFiles);
            addSigned(result.kingSafety, sign, -moved * 4, 0);
        }

        const int kingFile = fileOf(myKing);
        const int kingRank = rankOf(myKing);
        int shield = 0;
        for (int fileDelta = -1; fileDelta <= 1; ++fileDelta) {
            const int file = kingFile + fileDelta;
            if (file < 0 || file > 7)
                continue;
            const Bitboard filePawns = myPawns & fileMask(file);
            const int firstRank = color == Color::White ? kingRank + 1 : kingRank - 1;
            const int secondRank = color == Color::White ? kingRank + 2 : kingRank - 2;
            if (firstRank >= 0 && firstRank < 8
                && (filePawns & bit(makeSquare(file, firstRank))) != EMPTY_BB) {
                shield += 12;
            } else if (secondRank >= 0 && secondRank < 8
                       && (filePawns & bit(makeSquare(file, secondRank))) != EMPTY_BB) {
                shield += 5;
            }
            if (filePawns == EMPTY_BB)
                shield -= 12;
            if ((filePawns | (opponentPawns & fileMask(file))) == EMPTY_BB)
                shield -= 6;
        }
        addSigned(result.kingSafety, sign, shield, 0);

        const Bitboard ring = Attacks::king(myKing);
        int danger = 0;
        int attackers = 0;
        Bitboard enemyKnights = position.pieces(opponent, PieceType::Knight);
        while (enemyKnights != EMPTY_BB) {
            const Bitboard attacked = Attacks::knight(popLsb(enemyKnights)) & ring;
            if (attacked != EMPTY_BB) {
                ++attackers;
                danger += 20 * popcount(attacked);
            }
        }
        Bitboard enemyBishops = position.pieces(opponent, PieceType::Bishop);
        while (enemyBishops != EMPTY_BB) {
            const Bitboard attacked = Attacks::bishop(
                popLsb(enemyBishops), position.occupied()) & ring;
            if (attacked != EMPTY_BB) {
                ++attackers;
                danger += 20 * popcount(attacked);
            }
        }
        Bitboard enemyRooks = position.pieces(opponent, PieceType::Rook);
        while (enemyRooks != EMPTY_BB) {
            const Bitboard attacked = Attacks::rook(
                popLsb(enemyRooks), position.occupied()) & ring;
            if (attacked != EMPTY_BB) {
                ++attackers;
                danger += 35 * popcount(attacked);
            }
        }

        Square ringQueen = Square::None;
        Bitboard enemyQueens = position.pieces(opponent, PieceType::Queen);
        while (enemyQueens != EMPTY_BB) {
            const Square square = popLsb(enemyQueens);
            const Bitboard attacked = Attacks::queen(square, position.occupied()) & ring;
            if (attacked != EMPTY_BB) {
                ++attackers;
                danger += 55 * popcount(attacked);
                ringQueen = square;
            }
        }

        for (int fileDelta = -1; fileDelta <= 1; ++fileDelta) {
            const int file = kingFile + fileDelta;
            if (file < 0 || file > 7 || (myPawns & fileMask(file)) != EMPTY_BB)
                continue;
            const Bitboard enemyRQ = (position.pieces(opponent, PieceType::Rook)
                                     | position.pieces(opponent, PieceType::Queen))
                                    & fileMask(file);
            if (enemyRQ != EMPTY_BB)
                danger += 15;
        }

        if (isValid(ringQueen)) {
            const Bitboard queenAttack = Attacks::queen(
                ringQueen, position.occupied()) & ring;
            int queenRingPressure = 14 * popcount(queenAttack);
            if ((Attacks::king(ringQueen) & ring) != EMPTY_BB)
                queenRingPressure += 22;
            if (queenRingPressure != 0)
                addSigned(result.kingSafety, sign,
                          -queenRingPressure, -queenRingPressure / 2);
            if ((Attacks::king(ringQueen) & ring) != EMPTY_BB)
                danger += 25;
        }

        constexpr int ATTACK_PERCENT[8] = {0, 0, 25, 50, 80, 100, 100, 100};
        const int percent = ATTACK_PERCENT[std::min(attackers, 7)];
        addSigned(result.kingSafety, sign, -(danger * percent) / 100, 0);

        if (isValid(ringQueen)) {
            const int queenRankFromBack = color == Color::White
                ? rankOf(ringQueen) : 7 - rankOf(ringQueen);
            if (queenRankFromBack <= 1) {
                const int fileDistance = std::abs(fileOf(ringQueen) - kingFile);
                const int penalty = fileDistance <= 3 ? 40 : 20;
                addSigned(result.kingSafety, sign, -penalty, -penalty / 2);
            }
        }

        Bitboard escapeSquares = Attacks::king(myKing) & ~own;
        int freeSquares = 0;
        while (escapeSquares != EMPTY_BB) {
            const Square square = popLsb(escapeSquares);
            if (!position.isSquareAttacked(square, opponent))
                ++freeSquares;
        }
        if (attackers >= 2 && freeSquares <= 1) {
            const int mateThreat = (2 - freeSquares) * 60;
            addSigned(result.kingSafety, sign, -mateThreat, -mateThreat);
        }

        const int backRank = color == Color::White ? 0 : 7;
        if (kingRank == backRank) {
            const Bitboard forwardKingSquares = Attacks::king(myKing) & ~rankMask(backRank);
            const bool walledIn = (forwardKingSquares & ~position.occupied()) == EMPTY_BB;
            if (walledIn) {
                const Bitboard backRankMask = rankMask(backRank);
                const bool defended = ((position.pieces(color, PieceType::Rook)
                                      | position.pieces(color, PieceType::Queen))
                                     & backRankMask) != EMPTY_BB;
                bool threatenedBackRank = false;
                Bitboard enemyRQ = position.pieces(opponent, PieceType::Rook)
                                 | position.pieces(opponent, PieceType::Queen);
                while (enemyRQ != EMPTY_BB) {
                    const Square square = popLsb(enemyRQ);
                    if ((Attacks::rook(square, position.occupied())
                         & backRankMask & bit(myKing)) != EMPTY_BB) {
                        threatenedBackRank = true;
                    }
                }
                if (threatenedBackRank && !defended)
                    addSigned(result.kingSafety, sign, -50, -30);
            }
        }
    }

    result.phase = std::min(result.phase, MAX_PHASE);
    const Value whiteScore = taper(result.total(), result.phase);
    if (breakdown)
        *breakdown = result;
    return (position.sideToMove() == Color::White ? whiteScore : -whiteScore) + 10;
}

} // namespace Eunshin
