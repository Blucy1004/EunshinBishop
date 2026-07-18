#pragma once

#include "bitboard.h"

#include <cstdint>

namespace Eunshin {
namespace Attacks {

namespace Detail {

// Storage is visible only to the inline read path. Mutation is confined to
// attacks.cpp during single-threaded engine startup.
struct Tables final {
    Bitboard pawn[COLOR_NB][SQUARE_NB]{};
    Bitboard knight[SQUARE_NB]{};
    Bitboard king[SQUARE_NB]{};

    Bitboard bishopMask[SQUARE_NB]{};
    Bitboard rookMask[SQUARE_NB]{};
    Bitboard bishopMagic[SQUARE_NB]{};
    Bitboard rookMagic[SQUARE_NB]{};
    std::uint8_t bishopShift[SQUARE_NB]{};
    std::uint8_t rookShift[SQUARE_NB]{};

    Bitboard bishopTable[SQUARE_NB][512]{};
    Bitboard rookTable[SQUARE_NB][4096]{};

    bool initialized = false;
};

extern const Tables* const readOnlyTables;

} // namespace Detail

// Must run once on the startup thread. It consumes the current Random stream
// in the reference order (rook magics, then bishop magics) and is idempotent.
void initialize() noexcept;

[[nodiscard]] inline bool initialized() noexcept {
    return Detail::readOnlyTables->initialized;
}

[[nodiscard]] inline Bitboard pawnAttacks(Color color, Square square) noexcept {
    return isValid(color) && isValid(square)
         ? Detail::readOnlyTables->pawn[index(color)][index(square)]
         : EMPTY_BB;
}

[[nodiscard]] inline Bitboard knightAttacks(Square square) noexcept {
    return isValid(square)
         ? Detail::readOnlyTables->knight[index(square)]
         : EMPTY_BB;
}

[[nodiscard]] inline Bitboard kingAttacks(Square square) noexcept {
    return isValid(square)
         ? Detail::readOnlyTables->king[index(square)]
         : EMPTY_BB;
}

[[nodiscard]] inline Bitboard bishopAttacks(Square square, Bitboard occupied) noexcept {
    if (!isValid(square))
        return EMPTY_BB;

    const int sq = index(square);
    const Bitboard blockers = occupied & Detail::readOnlyTables->bishopMask[sq];
    const Bitboard tableIndex =
        (blockers * Detail::readOnlyTables->bishopMagic[sq])
        >> Detail::readOnlyTables->bishopShift[sq];
    return Detail::readOnlyTables->bishopTable[sq][tableIndex];
}

[[nodiscard]] inline Bitboard rookAttacks(Square square, Bitboard occupied) noexcept {
    if (!isValid(square))
        return EMPTY_BB;

    const int sq = index(square);
    const Bitboard blockers = occupied & Detail::readOnlyTables->rookMask[sq];
    const Bitboard tableIndex =
        (blockers * Detail::readOnlyTables->rookMagic[sq])
        >> Detail::readOnlyTables->rookShift[sq];
    return Detail::readOnlyTables->rookTable[sq][tableIndex];
}

[[nodiscard]] inline Bitboard queenAttacks(Square square, Bitboard occupied) noexcept {
    return bishopAttacks(square, occupied) | rookAttacks(square, occupied);
}

[[nodiscard]] inline Bitboard relevantBishopMask(Square square) noexcept {
    return isValid(square)
         ? Detail::readOnlyTables->bishopMask[index(square)]
         : EMPTY_BB;
}

[[nodiscard]] inline Bitboard relevantRookMask(Square square) noexcept {
    return isValid(square)
         ? Detail::readOnlyTables->rookMask[index(square)]
         : EMPTY_BB;
}

// Concise aliases keep call sites readable without exposing table storage.
[[nodiscard]] inline Bitboard pawn(Color color, Square square) noexcept {
    return pawnAttacks(color, square);
}

[[nodiscard]] inline Bitboard knight(Square square) noexcept {
    return knightAttacks(square);
}

[[nodiscard]] inline Bitboard king(Square square) noexcept {
    return kingAttacks(square);
}

[[nodiscard]] inline Bitboard bishop(Square square, Bitboard occupied) noexcept {
    return bishopAttacks(square, occupied);
}

[[nodiscard]] inline Bitboard rook(Square square, Bitboard occupied) noexcept {
    return rookAttacks(square, occupied);
}

[[nodiscard]] inline Bitboard queen(Square square, Bitboard occupied) noexcept {
    return queenAttacks(square, occupied);
}

} // namespace Attacks
} // namespace Eunshin
