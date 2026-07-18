#include "search/history.h"

#include <algorithm>
#include <cstdlib>

namespace Eunshin {

void HistoryTables::clear() noexcept {
    main_.fill(0);
    capture_.fill(0);
    counter_.fill(Move::none());
}

void HistoryTables::decay() noexcept {
    for (std::int16_t& value : main_)
        value = static_cast<std::int16_t>(value / 2);
    for (std::int16_t& value : capture_)
        value = static_cast<std::int16_t>(value / 2);
}

int HistoryTables::mainScore(Color color, Square from, Square to) const noexcept {
    if (!isValid(color) || !isValid(from) || !isValid(to)) return 0;
    return main_[mainIndex(color, from, to)];
}

void HistoryTables::updateMain(Color color,
                               Square from,
                               Square to,
                               int bonus) noexcept {
    if (!isValid(color) || !isValid(from) || !isValid(to)) return;
    gravity(main_[mainIndex(color, from, to)], bonus);
}

int HistoryTables::captureScore(Color color,
                                PieceType moved,
                                Square to,
                                PieceType captured) const noexcept {
    if (!isValid(color) || !isValid(moved) || !isValid(to) ||
        !isValid(captured))
        return 0;
    return capture_[captureIndex(color, moved, to, captured)];
}

void HistoryTables::updateCapture(Color color,
                                  PieceType moved,
                                  Square to,
                                  PieceType captured,
                                  int bonus) noexcept {
    if (!isValid(color) || !isValid(moved) || !isValid(to) ||
        !isValid(captured))
        return;
    gravity(capture_[captureIndex(color, moved, to, captured)], bonus);
}

Move HistoryTables::counterMove(Color color,
                                PieceType moved,
                                Square to) const noexcept {
    if (!isValid(color) || !isValid(moved) || !isValid(to)) return Move::none();
    return counter_[counterIndex(color, moved, to)];
}

void HistoryTables::setCounterMove(Color color,
                                   PieceType moved,
                                   Square to,
                                   Move reply) noexcept {
    if (!isValid(color) || !isValid(moved) || !isValid(to)) return;
    counter_[counterIndex(color, moved, to)] = reply;
}

std::size_t HistoryTables::mainIndex(Color color,
                                     Square from,
                                     Square to) noexcept {
    return (static_cast<std::size_t>(index(color)) * SQUARE_NB +
            static_cast<std::size_t>(index(from))) * SQUARE_NB +
           static_cast<std::size_t>(index(to));
}

std::size_t HistoryTables::captureIndex(Color color,
                                        PieceType moved,
                                        Square to,
                                        PieceType captured) noexcept {
    const std::size_t colorIndex = static_cast<std::size_t>(index(color));
    const std::size_t movedIndex =
        static_cast<std::size_t>(pieceTypeIndex(moved));
    const std::size_t toIndex = static_cast<std::size_t>(index(to));
    const std::size_t capturedIndex =
        static_cast<std::size_t>(pieceTypeIndex(captured));
    return (((colorIndex * PIECE_TYPE_NB + movedIndex) * SQUARE_NB + toIndex) *
            PIECE_TYPE_NB) + capturedIndex;
}

std::size_t HistoryTables::counterIndex(Color color,
                                        PieceType moved,
                                        Square to) noexcept {
    return (static_cast<std::size_t>(index(color)) * PIECE_TYPE_NB +
            static_cast<std::size_t>(pieceTypeIndex(moved))) * SQUARE_NB +
           static_cast<std::size_t>(index(to));
}

void HistoryTables::gravity(std::int16_t& entry, int bonus) noexcept {
    const int bounded = std::max(-MAX_HISTORY, std::min(bonus, MAX_HISTORY));
    const int current = entry;
    const int updated = current + bounded -
        current * std::abs(bounded) / MAX_HISTORY;
    entry = static_cast<std::int16_t>(
        std::max(-MAX_HISTORY, std::min(updated, MAX_HISTORY)));
}

} // namespace Eunshin
