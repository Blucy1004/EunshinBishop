#include "random.h"

namespace Eunshin {
namespace Random {
namespace {

std::uint64_t streamState = DEFAULT_SEED;

} // namespace

void reset(std::uint64_t seed) noexcept {
    streamState = seed;
}

std::uint64_t next() noexcept {
    // xorshift64* sequence from the reference single-file engine.
    streamState ^= streamState >> 12;
    streamState ^= streamState << 25;
    streamState ^= streamState >> 27;
    return streamState * 0x2545F4914F6CDD1DULL;
}

std::uint64_t sparse() noexcept {
    return next() & next() & next();
}

std::uint64_t state() noexcept {
    return streamState;
}

} // namespace Random
} // namespace Eunshin
