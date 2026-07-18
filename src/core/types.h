#pragma once

#include <cstdint>
#include <type_traits>

namespace Eunshin {

using Value = int;
using Depth = int;
using Bitboard = std::uint64_t;
using Key = std::uint64_t;

inline constexpr Value INF = 32000;
inline constexpr Value MATE = 31000;
// A real evaluation is always strictly inside [-INF, INF].  SearchStack and
// TT-facing code use this distinct sentinel until an evaluation is known.
inline constexpr Value VALUE_NONE = INF + 1;
inline constexpr int MAX_PLY = 128;
inline constexpr int MAX_MOVES = 256;

inline constexpr int COLOR_NB = 2;
inline constexpr int PIECE_TYPE_NB = 6;
inline constexpr int SQUARE_NB = 64;

enum class Color : std::uint8_t {
    White = 0,
    Black = 1,
    None = 2
};

enum class PieceType : std::uint8_t {
    None = 0,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King
};

enum class Piece : std::uint8_t {
    None = 0,

    WhitePawn,
    WhiteKnight,
    WhiteBishop,
    WhiteRook,
    WhiteQueen,
    WhiteKing,

    BlackPawn,
    BlackKnight,
    BlackBishop,
    BlackRook,
    BlackQueen,
    BlackKing
};

// Little-endian rank-file numbering: a1 = 0, h8 = 63.
enum class Square : std::uint8_t {
    A1 = 0, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    None
};

template <typename Enum>
[[nodiscard]] constexpr int index(Enum value) noexcept {
    static_assert(std::is_enum<Enum>::value, "Eunshin::index requires an enum");
    return static_cast<int>(value);
}

[[nodiscard]] constexpr bool isValid(Color color) noexcept {
    return color == Color::White || color == Color::Black;
}

[[nodiscard]] constexpr bool isValid(PieceType type) noexcept {
    return type >= PieceType::Pawn && type <= PieceType::King;
}

[[nodiscard]] constexpr bool isValid(Piece piece) noexcept {
    return piece >= Piece::WhitePawn && piece <= Piece::BlackKing;
}

[[nodiscard]] constexpr bool isValid(Square square) noexcept {
    return index(square) >= 0 && index(square) < SQUARE_NB;
}

[[nodiscard]] constexpr Color opposite(Color color) noexcept {
    return color == Color::White ? Color::Black
         : color == Color::Black ? Color::White
                                 : Color::None;
}

[[nodiscard]] constexpr Color colorOf(Piece piece) noexcept {
    return piece == Piece::None ? Color::None
         : piece >= Piece::BlackPawn ? Color::Black
                                     : Color::White;
}

[[nodiscard]] constexpr PieceType typeOf(Piece piece) noexcept {
    if (piece == Piece::None)
        return PieceType::None;

    const int zeroBased = (index(piece) - 1) % PIECE_TYPE_NB;
    return static_cast<PieceType>(zeroBased + 1);
}

[[nodiscard]] constexpr Piece makePiece(Color color, PieceType type) noexcept {
    if (!isValid(color) || !isValid(type))
        return Piece::None;

    const int colorOffset = color == Color::Black ? PIECE_TYPE_NB : 0;
    return static_cast<Piece>(1 + colorOffset + (index(type) - 1));
}

// Converts Pawn..King to the dense 0..5 index required by board arrays and
// the frozen NNUE feature mapping. None maps to -1.
[[nodiscard]] constexpr int pieceTypeIndex(PieceType type) noexcept {
    return isValid(type) ? index(type) - 1 : -1;
}

[[nodiscard]] constexpr int fileOf(Square square) noexcept {
    return isValid(square) ? index(square) & 7 : -1;
}

[[nodiscard]] constexpr int rankOf(Square square) noexcept {
    return isValid(square) ? index(square) >> 3 : -1;
}

[[nodiscard]] constexpr Square makeSquare(int file, int rank) noexcept {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8
         ? static_cast<Square>(rank * 8 + file)
         : Square::None;
}

static_assert(sizeof(Color) == 1, "Color must remain a small value type");
static_assert(sizeof(PieceType) == 1, "PieceType must remain a small value type");
static_assert(sizeof(Piece) == 1, "Piece must remain a small value type");
static_assert(sizeof(Square) == 1, "Square must remain a small value type");
static_assert(sizeof(Bitboard) == 8, "Bitboards require exactly 64 bits");
static_assert(sizeof(Key) == 8, "Zobrist keys require exactly 64 bits");

} // namespace Eunshin
