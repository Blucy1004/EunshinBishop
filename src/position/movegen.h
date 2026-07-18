#pragma once

#include "core/move.h"
#include "core/types.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace Eunshin {

class Position;

enum class GenerationType : std::uint8_t {
    All,
    Captures
};

struct MoveList {
    std::array<Move, MAX_MOVES> moves{};
    std::size_t size = 0;

    constexpr void clear() noexcept { size = 0; }

    constexpr bool add(Move move) noexcept {
        if (size >= moves.size()) return false;
        moves[size++] = move;
        return true;
    }

    constexpr Move operator[](std::size_t index) const noexcept {
        return moves[index];
    }
};

void generateMoves(const Position& position,
                   MoveList& list,
                   GenerationType type = GenerationType::All) noexcept;

void generateLegalMoves(Position& position, MoveList& list) noexcept;

[[nodiscard]] Move moveFromUci(const Position& position,
                               std::string_view text) noexcept;

[[nodiscard]] std::uint64_t perft(Position& position, Depth depth) noexcept;

} // namespace Eunshin
