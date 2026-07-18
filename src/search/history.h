#pragma once

#include "core/move.h"
#include "core/types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Eunshin {

class HistoryTables final {
public:
    static constexpr int MAX_HISTORY = 16384;

    void clear() noexcept;
    void decay() noexcept;

    [[nodiscard]] int mainScore(Color color, Square from, Square to) const noexcept;
    void updateMain(Color color, Square from, Square to, int bonus) noexcept;

    [[nodiscard]] int captureScore(Color color,
                                   PieceType moved,
                                   Square to,
                                   PieceType captured) const noexcept;
    void updateCapture(Color color,
                       PieceType moved,
                       Square to,
                       PieceType captured,
                       int bonus) noexcept;

    [[nodiscard]] Move counterMove(Color color,
                                   PieceType moved,
                                   Square to) const noexcept;
    void setCounterMove(Color color,
                        PieceType moved,
                        Square to,
                        Move reply) noexcept;

private:
    static constexpr std::size_t MAIN_SIZE =
        COLOR_NB * SQUARE_NB * SQUARE_NB;
    static constexpr std::size_t CAPTURE_SIZE =
        COLOR_NB * PIECE_TYPE_NB * SQUARE_NB * PIECE_TYPE_NB;
    static constexpr std::size_t COUNTER_SIZE =
        COLOR_NB * PIECE_TYPE_NB * SQUARE_NB;

    [[nodiscard]] static std::size_t mainIndex(Color color,
                                               Square from,
                                               Square to) noexcept;
    [[nodiscard]] static std::size_t captureIndex(Color color,
                                                  PieceType moved,
                                                  Square to,
                                                  PieceType captured) noexcept;
    [[nodiscard]] static std::size_t counterIndex(Color color,
                                                  PieceType moved,
                                                  Square to) noexcept;
    static void gravity(std::int16_t& entry, int bonus) noexcept;

    std::array<std::int16_t, MAIN_SIZE> main_{};
    std::array<std::int16_t, CAPTURE_SIZE> capture_{};
    std::array<Move, COUNTER_SIZE> counter_{};
};

} // namespace Eunshin
