#pragma once

#include "core/move.h"
#include "core/nnue_state.h"
#include "core/types.h"

#include <cstdint>

namespace Eunshin {

enum CastlingRight : std::uint8_t {
    NoCastling = 0,
    WhiteKingSide = 1U << 0,
    WhiteQueenSide = 1U << 1,
    BlackKingSide = 1U << 2,
    BlackQueenSide = 1U << 3,
    AllCastling = WhiteKingSide | WhiteQueenSide |
                  BlackKingSide | BlackQueenSide
};

struct StateInfo {
    StateInfo* previous = nullptr;

    Key positionKey = 0;
    Key pawnKey = 0;
    Key materialKey = 0;

    int rule50 = 0;
    int pliesFromNull = 0;
    int fullmoveNumber = 1;

    std::uint8_t castlingRights = NoCastling;
    Square epSquare = Square::None;

    Move move{};
    Piece movedPiece = Piece::None;
    Piece capturedPiece = Piece::None;
    Square capturedSquare = Square::None;

    Bitboard checkers = 0;
    bool nullMove = false;

    NNUE::AccumulatorState accumulator{};
};

} // namespace Eunshin
