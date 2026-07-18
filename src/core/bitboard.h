#pragma once

#include "types.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace Eunshin {

inline constexpr Bitboard EMPTY_BB = 0ULL;
inline constexpr Bitboard FULL_BB = ~EMPTY_BB;

inline constexpr Bitboard FILE_A = 0x0101010101010101ULL;
inline constexpr Bitboard FILE_B = FILE_A << 1;
inline constexpr Bitboard FILE_C = FILE_A << 2;
inline constexpr Bitboard FILE_D = FILE_A << 3;
inline constexpr Bitboard FILE_E = FILE_A << 4;
inline constexpr Bitboard FILE_F = FILE_A << 5;
inline constexpr Bitboard FILE_G = FILE_A << 6;
inline constexpr Bitboard FILE_H = FILE_A << 7;

inline constexpr Bitboard RANK_1 = 0x00000000000000FFULL;
inline constexpr Bitboard RANK_2 = RANK_1 << 8;
inline constexpr Bitboard RANK_3 = RANK_1 << 16;
inline constexpr Bitboard RANK_4 = RANK_1 << 24;
inline constexpr Bitboard RANK_5 = RANK_1 << 32;
inline constexpr Bitboard RANK_6 = RANK_1 << 40;
inline constexpr Bitboard RANK_7 = RANK_1 << 48;
inline constexpr Bitboard RANK_8 = RANK_1 << 56;

[[nodiscard]] constexpr Bitboard bit(Square square) noexcept {
    return isValid(square) ? (Bitboard{1} << index(square)) : EMPTY_BB;
}

[[nodiscard]] constexpr Bitboard fileMask(int file) noexcept {
    return file >= 0 && file < 8 ? FILE_A << file : EMPTY_BB;
}

[[nodiscard]] constexpr Bitboard rankMask(int rank) noexcept {
    return rank >= 0 && rank < 8 ? RANK_1 << (rank * 8) : EMPTY_BB;
}

[[nodiscard]] constexpr bool isSet(Bitboard board, Square square) noexcept {
    return (board & bit(square)) != EMPTY_BB;
}

[[nodiscard]] constexpr bool hasSingleBit(Bitboard board) noexcept {
    return board != EMPTY_BB && (board & (board - 1)) == EMPTY_BB;
}

[[nodiscard]] constexpr bool moreThanOne(Bitboard board) noexcept {
    return board != EMPTY_BB && (board & (board - 1)) != EMPTY_BB;
}

[[nodiscard]] inline int popcount(Bitboard board) noexcept {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    return static_cast<int>(__popcnt64(board));
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(board);
#else
    // Portable SWAR fallback for uncommon C++17 toolchains.
    board -= (board >> 1) & 0x5555555555555555ULL;
    board = (board & 0x3333333333333333ULL)
          + ((board >> 2) & 0x3333333333333333ULL);
    board = (board + (board >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return static_cast<int>((board * 0x0101010101010101ULL) >> 56);
#endif
}

// All scan helpers are defined for zero. They return Square::None instead of
// invoking a compiler intrinsic with an invalid zero operand.
[[nodiscard]] inline Square lsb(Bitboard board) noexcept {
    if (board == EMPTY_BB)
        return Square::None;

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    unsigned long result = 0;
    _BitScanForward64(&result, board);
    return static_cast<Square>(result);
#elif defined(_MSC_VER)
    unsigned long result = 0;
    const unsigned long low = static_cast<unsigned long>(board);
    if (_BitScanForward(&result, low) != 0)
        return static_cast<Square>(result);
    const unsigned long high = static_cast<unsigned long>(board >> 32);
    _BitScanForward(&result, high);
    return static_cast<Square>(result + 32);
#elif defined(__GNUC__) || defined(__clang__)
    return static_cast<Square>(__builtin_ctzll(board));
#else
    int result = 0;
    while ((board & 1ULL) == 0ULL) {
        board >>= 1;
        ++result;
    }
    return static_cast<Square>(result);
#endif
}

[[nodiscard]] inline Square msb(Bitboard board) noexcept {
    if (board == EMPTY_BB)
        return Square::None;

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    unsigned long result = 0;
    _BitScanReverse64(&result, board);
    return static_cast<Square>(result);
#elif defined(_MSC_VER)
    unsigned long result = 0;
    const unsigned long high = static_cast<unsigned long>(board >> 32);
    if (_BitScanReverse(&result, high) != 0)
        return static_cast<Square>(result + 32);
    const unsigned long low = static_cast<unsigned long>(board);
    _BitScanReverse(&result, low);
    return static_cast<Square>(result);
#elif defined(__GNUC__) || defined(__clang__)
    return static_cast<Square>(63 - __builtin_clzll(board));
#else
    int result = 0;
    while ((board >>= 1) != EMPTY_BB)
        ++result;
    return static_cast<Square>(result);
#endif
}

[[nodiscard]] inline Square popLsb(Bitboard& board) noexcept {
    const Square square = lsb(board);
    if (square != Square::None)
        board &= board - 1;
    return square;
}

constexpr void clearLsb(Bitboard& board) noexcept {
    if (board != EMPTY_BB)
        board &= board - 1;
}

[[nodiscard]] constexpr Bitboard north(Bitboard board) noexcept {
    return board << 8;
}

[[nodiscard]] constexpr Bitboard south(Bitboard board) noexcept {
    return board >> 8;
}

[[nodiscard]] constexpr Bitboard east(Bitboard board) noexcept {
    return (board & ~FILE_H) << 1;
}

[[nodiscard]] constexpr Bitboard west(Bitboard board) noexcept {
    return (board & ~FILE_A) >> 1;
}

[[nodiscard]] constexpr Bitboard northEast(Bitboard board) noexcept {
    return (board & ~FILE_H) << 9;
}

[[nodiscard]] constexpr Bitboard northWest(Bitboard board) noexcept {
    return (board & ~FILE_A) << 7;
}

[[nodiscard]] constexpr Bitboard southEast(Bitboard board) noexcept {
    return (board & ~FILE_H) >> 7;
}

[[nodiscard]] constexpr Bitboard southWest(Bitboard board) noexcept {
    return (board & ~FILE_A) >> 9;
}

static_assert(bit(Square::A1) == 0x1ULL, "Square and bitboard numbering disagree");
static_assert(bit(Square::H8) == 0x8000000000000000ULL,
              "Square and bitboard numbering disagree");
static_assert(bit(Square::None) == EMPTY_BB, "Invalid-square bit lookup must be safe");

} // namespace Eunshin
