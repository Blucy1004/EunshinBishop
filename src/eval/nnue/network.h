#pragma once

#include "core/types.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace Eunshin {

class Position;

namespace NNUE {

inline constexpr int INPUT_FEATURES = 24576;
inline constexpr int HIDDEN = 256;
inline constexpr int CONTEXTS = 6;
inline constexpr int OUTPUT_INPUTS = 2 * HIDDEN + CONTEXTS;
inline constexpr int QUANTIZATION_SCALE = 256;
inline constexpr Value NETWORK_CP_LIMIT = 30000;

struct LoadResult final {
    bool success = false;
    std::string message;
    std::string path;
    std::uint64_t generation = 0;
};

struct AccumulatorCheck final {
    bool available = false;
    bool exact = false;
    int mismatchPerspective = -1;
    int mismatchNeuron = -1;
    std::int64_t incrementalValue = 0;
    std::int64_t scratchValue = 0;
    Value incrementalWhite = 0;
    Value scratchWhite = 0;
    Value incrementalSideToMove = 0;
    Value scratchSideToMove = 0;
};

[[nodiscard]] int roundAwayFromZero(
    std::int64_t numerator, std::int64_t positiveDenominator) noexcept;

[[nodiscard]] int canonicalFeatureIndex(
    Square pieceSquare,
    Color pieceColor,
    PieceType pieceType,
    Color perspective,
    Square perspectiveKingSquare) noexcept;

class Network final {
public:
    // Opaque immutable parameter block.  It is public only so translation-unit
    // helpers can operate on a captured snapshot; clients receive no instance.
    struct Parameters;

    Network() noexcept;
    ~Network();

    Network(const Network&) = delete;
    Network& operator=(const Network&) = delete;

    // Builds and validates a private candidate.  A failed load leaves the
    // previously published, immutable network available.
    [[nodiscard]] LoadResult load(std::string_view path) noexcept;
    void unload() noexcept;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] std::string loadedPath() const;
    [[nodiscard]] std::string payloadSha256() const;
    [[nodiscard]] std::string lastError() const;

    // White-relative and side-to-move inference entry points.  false means no
    // complete network or no valid pair of kings; output is then set to zero.
    [[nodiscard]] bool inferWhiteScratch(
        const Position& position, Value& output) const noexcept;
    [[nodiscard]] bool inferWhite(
        Position& position, Value& output) const noexcept;
    [[nodiscard]] bool inferSideToMove(
        Position& position, Value& output) const noexcept;

    // Call after a successful Position::doMove().  It derives the child from
    // StateInfo::previous when possible.  Failure merely leaves the child
    // invalid so inferWhite() can recover with a scratch refresh.
    [[nodiscard]] bool updateAccumulatorAfterMove(
        Position& position) const noexcept;

    [[nodiscard]] AccumulatorCheck verifyAccumulator(
        Position& position) const noexcept;

private:
    [[nodiscard]] std::shared_ptr<const Parameters> snapshot() const noexcept;

    std::shared_ptr<const Parameters> active_;
    mutable std::mutex loadMutex_;
    std::string lastError_;
    std::uint64_t lastGeneration_ = 0;
};

} // namespace NNUE
} // namespace Eunshin
