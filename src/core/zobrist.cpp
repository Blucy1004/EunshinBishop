#include "zobrist.h"

#include "random.h"

namespace Eunshin {
namespace Zobrist {
namespace {

Detail::Tables mutableTables;

} // namespace

namespace Detail {

const Tables* const readOnlyTables = &mutableTables;

} // namespace Detail

void initialize() noexcept {
    if (mutableTables.initialized)
        return;

    // Keep the exact reference order through EP keys. Material-count keys are
    // a Q-architecture extension and therefore consume only the stream suffix.
    for (int color = 0; color < COLOR_NB; ++color) {
        for (int type = 0; type < PIECE_TYPE_NB; ++type) {
            for (int square = 0; square < SQUARE_NB; ++square)
                mutableTables.piece[color][type][square] = Random::next();
        }
    }

    mutableTables.side = Random::next();

    for (Key& key : mutableTables.castle)
        key = Random::next();

    for (Key& key : mutableTables.ep)
        key = Random::next();

    for (int color = 0; color < COLOR_NB; ++color) {
        for (int type = 0; type < PIECE_TYPE_NB; ++type) {
            for (int count = 0; count < MATERIAL_COUNT_SLOTS; ++count)
                mutableTables.material[color][type][count] = Random::next();
        }
    }

    mutableTables.initialized = true;
}

} // namespace Zobrist
} // namespace Eunshin
