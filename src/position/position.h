#pragma once

#include "core/move.h"
#include "core/types.h"
#include "position/state.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace Eunshin {

class Position;

bool setFromFen(Position& position,
                std::string_view fen,
                StateInfo& rootState,
                std::string* error);
std::string toFen(const Position& position);

class Position {
public:
    Position() noexcept = default;

    [[nodiscard]] Piece pieceOn(Square square) const noexcept;
    [[nodiscard]] Bitboard pieces(Color color) const noexcept;
    [[nodiscard]] Bitboard pieces(PieceType type) const noexcept;
    [[nodiscard]] Bitboard pieces(Color color, PieceType type) const noexcept;
    [[nodiscard]] Bitboard occupied() const noexcept { return occupied_; }

    [[nodiscard]] Color sideToMove() const noexcept { return sideToMove_; }
    [[nodiscard]] Square kingSquare(Color color) const noexcept;
    [[nodiscard]] Bitboard attackersTo(Square square, Color by) const noexcept;
    [[nodiscard]] bool isSquareAttacked(Square square, Color by) const noexcept;
    [[nodiscard]] bool inCheck() const noexcept;

    [[nodiscard]] const StateInfo* state() const noexcept { return state_; }
    [[nodiscard]] StateInfo* state() noexcept { return state_; }
    [[nodiscard]] Key key() const noexcept;
    [[nodiscard]] Key pawnKey() const noexcept;
    [[nodiscard]] Key materialKey() const noexcept;
    [[nodiscard]] int rule50() const noexcept;
    [[nodiscard]] int pliesFromNull() const noexcept;
    [[nodiscard]] int fullmoveNumber() const noexcept;
    [[nodiscard]] std::uint8_t castlingRights() const noexcept;
    [[nodiscard]] Square epSquare() const noexcept;

    [[nodiscard]] bool isPseudoLegal(Move move) const noexcept;
    [[nodiscard]] bool isLegal(Move move) const noexcept;

    // Creates an independent search root while preserving the immutable game
    // history chain used for repetition detection. The returned Position is
    // rebound to rootState, so search-side accumulator refreshes never mutate
    // the Engine's current StateInfo.
    [[nodiscard]] Position snapshotForSearch(StateInfo& rootState) const noexcept;

    bool doMove(Move move, StateInfo& newState) noexcept;
    void undoMove(Move move) noexcept;
    bool doNullMove(StateInfo& newState) noexcept;
    void undoNullMove() noexcept;

    [[nodiscard]] bool isRepetition(int requiredPriorOccurrences = 1) const noexcept;
    [[nodiscard]] bool isFiftyMoveDraw() const noexcept { return rule50() >= 100; }

    [[nodiscard]] bool isConsistent() const noexcept;
    [[nodiscard]] std::string consistencyError() const;
    [[nodiscard]] bool sameBoard(const Position& other) const noexcept;

private:
    std::array<Piece, 64> board_{};
    std::array<Bitboard, 2> byColor_{};
    std::array<Bitboard, 6> byType_{};
    Bitboard occupied_ = 0;
    Color sideToMove_ = Color::White;
    StateInfo* state_ = nullptr;

    void clear(StateInfo& rootState) noexcept;
    void putPieceRaw(Piece piece, Square square) noexcept;
    void removePieceRaw(Piece piece, Square square) noexcept;
    void movePieceRaw(Piece piece, Square from, Square to) noexcept;
    void putPieceHashed(Piece piece, Square square) noexcept;
    void removePieceHashed(Piece piece, Square square) noexcept;
    void movePieceHashed(Piece piece, Square from, Square to) noexcept;

    [[nodiscard]] Key recomputePositionKey() const noexcept;
    [[nodiscard]] Key recomputePawnKey() const noexcept;
    [[nodiscard]] Key recomputeMaterialKey() const noexcept;
    [[nodiscard]] std::uint8_t updatedCastlingRights(Square from,
                                                     Square to) const noexcept;

    friend bool setFromFen(Position&, std::string_view, StateInfo&, std::string*);
    friend std::string toFen(const Position&);
};

} // namespace Eunshin
