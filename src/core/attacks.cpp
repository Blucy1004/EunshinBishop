#include "attacks.h"

#include "random.h"

#include <cstddef>
#include <cstring>

namespace Eunshin {
namespace Attacks {
namespace {

Detail::Tables mutableTables;

[[nodiscard]] Bitboard slidingAttacks(
    Square square, Bitboard blockers, bool rook) noexcept {
    static constexpr int ROOK_DIRECTIONS[4][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };
    static constexpr int BISHOP_DIRECTIONS[4][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    Bitboard result = EMPTY_BB;
    const int originRank = rankOf(square);
    const int originFile = fileOf(square);

    for (int direction = 0; direction < 4; ++direction) {
        const int rankDelta = rook
            ? ROOK_DIRECTIONS[direction][0]
            : BISHOP_DIRECTIONS[direction][0];
        const int fileDelta = rook
            ? ROOK_DIRECTIONS[direction][1]
            : BISHOP_DIRECTIONS[direction][1];

        for (int rank = originRank + rankDelta, file = originFile + fileDelta;
             rank >= 0 && rank < 8 && file >= 0 && file < 8;
             rank += rankDelta, file += fileDelta) {
            const Square target = makeSquare(file, rank);
            result |= bit(target);
            if (isSet(blockers, target))
                break;
        }
    }

    return result;
}

[[nodiscard]] Bitboard slidingMask(Square square, bool rook) noexcept {
    Bitboard result = EMPTY_BB;
    const int originRank = rankOf(square);
    const int originFile = fileOf(square);

    if (rook) {
        for (int rank = originRank + 1; rank < 7; ++rank)
            result |= bit(makeSquare(originFile, rank));
        for (int rank = originRank - 1; rank > 0; --rank)
            result |= bit(makeSquare(originFile, rank));
        for (int file = originFile + 1; file < 7; ++file)
            result |= bit(makeSquare(file, originRank));
        for (int file = originFile - 1; file > 0; --file)
            result |= bit(makeSquare(file, originRank));
    } else {
        static constexpr int DIRECTIONS[4][2] = {
            {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
        };

        for (int direction = 0; direction < 4; ++direction) {
            for (int rank = originRank + DIRECTIONS[direction][0],
                     file = originFile + DIRECTIONS[direction][1];
                 rank > 0 && rank < 7 && file > 0 && file < 7;
                 rank += DIRECTIONS[direction][0],
                     file += DIRECTIONS[direction][1]) {
                result |= bit(makeSquare(file, rank));
            }
        }
    }

    return result;
}

[[nodiscard]] Bitboard occupancyFromIndex(
    int subset, int bitCount, Bitboard mask) noexcept {
    Bitboard occupied = EMPTY_BB;
    for (int bitIndex = 0; bitIndex < bitCount; ++bitIndex) {
        const Square square = popLsb(mask);
        if ((subset & (1 << bitIndex)) != 0)
            occupied |= bit(square);
    }
    return occupied;
}

void initializeMagics(bool rook) noexcept {
    for (int squareIndex = 0; squareIndex < SQUARE_NB; ++squareIndex) {
        const Square square = static_cast<Square>(squareIndex);
        const Bitboard mask = slidingMask(square, rook);
        const int relevantBits = popcount(mask);
        const int occupancyCount = 1 << relevantBits;

        Bitboard occupancies[4096];
        Bitboard referenceAttacks[4096];
        Bitboard used[4096];

        for (int indexValue = 0; indexValue < occupancyCount; ++indexValue) {
            occupancies[indexValue] =
                occupancyFromIndex(indexValue, relevantBits, mask);
            referenceAttacks[indexValue] =
                slidingAttacks(square, occupancies[indexValue], rook);
        }

        Bitboard magic = 0;
        for (;;) {
            magic = Random::sparse();
            if (popcount((mask * magic) >> 56) < 6)
                continue;

            std::memset(used, 0,
                        sizeof(Bitboard) * static_cast<std::size_t>(occupancyCount));
            bool collision = false;
            for (int indexValue = 0;
                 indexValue < occupancyCount && !collision;
                 ++indexValue) {
                const Bitboard tableIndex =
                    (occupancies[indexValue] * magic) >> (64 - relevantBits);
                if (used[tableIndex] == EMPTY_BB)
                    used[tableIndex] = referenceAttacks[indexValue];
                else if (used[tableIndex] != referenceAttacks[indexValue])
                    collision = true;
            }

            if (!collision)
                break;
        }

        if (rook) {
            mutableTables.rookMask[squareIndex] = mask;
            mutableTables.rookMagic[squareIndex] = magic;
            mutableTables.rookShift[squareIndex] =
                static_cast<std::uint8_t>(64 - relevantBits);
            std::memset(mutableTables.rookTable[squareIndex], 0,
                        sizeof(mutableTables.rookTable[squareIndex]));
            for (int indexValue = 0; indexValue < occupancyCount; ++indexValue) {
                const Bitboard tableIndex =
                    (occupancies[indexValue] * magic) >> (64 - relevantBits);
                mutableTables.rookTable[squareIndex][tableIndex] =
                    referenceAttacks[indexValue];
            }
        } else {
            mutableTables.bishopMask[squareIndex] = mask;
            mutableTables.bishopMagic[squareIndex] = magic;
            mutableTables.bishopShift[squareIndex] =
                static_cast<std::uint8_t>(64 - relevantBits);
            std::memset(mutableTables.bishopTable[squareIndex], 0,
                        sizeof(mutableTables.bishopTable[squareIndex]));
            for (int indexValue = 0; indexValue < occupancyCount; ++indexValue) {
                const Bitboard tableIndex =
                    (occupancies[indexValue] * magic) >> (64 - relevantBits);
                mutableTables.bishopTable[squareIndex][tableIndex] =
                    referenceAttacks[indexValue];
            }
        }
    }
}

void initializeLeapers() noexcept {
    static constexpr int KNIGHT_DELTAS[8][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };
    static constexpr int KING_DELTAS[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    for (int squareIndex = 0; squareIndex < SQUARE_NB; ++squareIndex) {
        const Square square = static_cast<Square>(squareIndex);
        const int rank = rankOf(square);
        const int file = fileOf(square);
        Bitboard knightResult = EMPTY_BB;
        Bitboard kingResult = EMPTY_BB;

        for (int delta = 0; delta < 8; ++delta) {
            knightResult |= bit(makeSquare(
                file + KNIGHT_DELTAS[delta][1],
                rank + KNIGHT_DELTAS[delta][0]));
            kingResult |= bit(makeSquare(
                file + KING_DELTAS[delta][1],
                rank + KING_DELTAS[delta][0]));
        }

        mutableTables.knight[squareIndex] = knightResult;
        mutableTables.king[squareIndex] = kingResult;

        const Bitboard origin = bit(square);
        mutableTables.pawn[index(Color::White)][squareIndex] =
            ((origin << 7) & ~FILE_H) | ((origin << 9) & ~FILE_A);
        mutableTables.pawn[index(Color::Black)][squareIndex] =
            ((origin >> 7) & ~FILE_A) | ((origin >> 9) & ~FILE_H);
    }
}

} // namespace

namespace Detail {

const Tables* const readOnlyTables = &mutableTables;

} // namespace Detail

void initialize() noexcept {
    if (mutableTables.initialized)
        return;

    initializeLeapers();
    // This ordering is part of the reference PRNG/Zobrist contract.
    initializeMagics(true);
    initializeMagics(false);
    mutableTables.initialized = true;
}

} // namespace Attacks
} // namespace Eunshin
