#pragma once

#include "types.h"

#include <cstdint>

namespace Eunshin {
namespace Zobrist {

inline constexpr int MATERIAL_COUNT_SLOTS = 17; // Counts 0..16 inclusive.

namespace Detail {

struct Tables final {
    Key piece[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB]{};
    Key side = 0;
    Key castle[16]{};
    Key ep[8]{};
    Key material[COLOR_NB][PIECE_TYPE_NB][MATERIAL_COUNT_SLOTS]{};
    bool initialized = false;
};

extern const Tables* const readOnlyTables;

} // namespace Detail

// Consumes the current Random stream. To reproduce the reference key prefix,
// call Random::reset(), Attacks::initialize(), then Zobrist::initialize().
// Initialization is idempotent and all public lookup functions are read-only.
void initialize() noexcept;

[[nodiscard]] inline bool initialized() noexcept {
    return Detail::readOnlyTables->initialized;
}

[[nodiscard]] inline Key piece(
    Color color, PieceType type, Square square) noexcept {
    const int typeIndex = pieceTypeIndex(type);
    return isValid(color) && typeIndex >= 0 && isValid(square)
         ? Detail::readOnlyTables->piece[index(color)][typeIndex][index(square)]
         : Key{0};
}

[[nodiscard]] inline Key side() noexcept {
    return Detail::readOnlyTables->side;
}

[[nodiscard]] inline Key castle(std::uint8_t rights) noexcept {
    return Detail::readOnlyTables->castle[rights & 0x0FU];
}

[[nodiscard]] inline Key castling(std::uint8_t rights) noexcept {
    return castle(rights);
}

[[nodiscard]] inline Key ep(int file) noexcept {
    return file >= 0 && file < 8
         ? Detail::readOnlyTables->ep[file]
         : Key{0};
}

[[nodiscard]] inline Key enPassantFile(int file) noexcept {
    return ep(file);
}

[[nodiscard]] inline Key enPassantSquare(Square square) noexcept {
    return ep(fileOf(square));
}

[[nodiscard]] inline Key materialCount(
    Color color, PieceType type, int count) noexcept {
    const int typeIndex = pieceTypeIndex(type);
    return isValid(color) && typeIndex >= 0
            && count >= 0 && count < MATERIAL_COUNT_SLOTS
         ? Detail::readOnlyTables->material[index(color)][typeIndex][count]
         : Key{0};
}

[[nodiscard]] inline Key material(
    Color color, PieceType type, int count) noexcept {
    return materialCount(color, type, count);
}

} // namespace Zobrist
} // namespace Eunshin
