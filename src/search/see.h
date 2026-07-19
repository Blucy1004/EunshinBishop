#pragma once

#include "core/move.h"
#include "core/types.h"

namespace Eunshin {

class Position;

// Specification item 20: an independent strict static-exchange evaluator.
// Unlike MovePicker's frozen-reference approximate swap list (ordering-only,
// see move_picker.cpp), this module replays the capture sequence with real
// Position::doMove/legal move generation so pinned attackers, king-capture
// legality, en passant occupancy, promotion/underpromotion, discovered
// attacks, and recapture sequencing all fall out of the already-verified
// move machinery instead of a second hand-rolled bitboard model.
//
// `move` must already be legal in `position`; See is consulted only from the
// optional SEEPruning path (default off) and never replaces move ordering.
namespace See {

[[nodiscard]] Value see(const Position& position, Move move) noexcept;

[[nodiscard]] bool seeGe(const Position& position, Move move, Value threshold) noexcept;

} // namespace See
} // namespace Eunshin
