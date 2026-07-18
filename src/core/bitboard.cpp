#include "bitboard.h"

namespace Eunshin {

// The primitive operations intentionally remain inline in bitboard.h because
// they sit on move-generation and attack hot paths. This translation unit is
// retained as the stable home for future non-inline bitboard algorithms.
static_assert(sizeof(Bitboard) * 8 == SQUARE_NB,
              "One bitboard bit must correspond to one board square");

} // namespace Eunshin
