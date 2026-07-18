#include "tt.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Eunshin {
namespace {

constexpr std::size_t MEBIBYTE = 1024U * 1024U;
constexpr Value MATE_TT_THRESHOLD = MATE - MAX_PLY;

[[nodiscard]] std::size_t highestPowerOfTwo(std::size_t value) noexcept {
    if (value == 0)
        return 0;

    std::size_t result = 1;
    while (result <= value / 2)
        result *= 2;
    return result;
}

[[nodiscard]] int boundedPly(int ply) noexcept {
    return std::max(0, std::min(ply, MAX_PLY));
}

} // namespace

bool TTEntry::occupied() const noexcept {
    return bound() != TTBound::None;
}

Move TTEntry::move() const noexcept {
    return Move::fromRaw(move16);
}

Value TTEntry::storedValue() const noexcept {
    return static_cast<Value>(value16);
}

Value TTEntry::staticEval() const noexcept {
    return static_cast<Value>(eval16);
}

Depth TTEntry::depth() const noexcept {
    return static_cast<Depth>(depth8);
}

TTBound TTEntry::bound() const noexcept {
    return static_cast<TTBound>(generationBound8 & 0x03U);
}

bool TTEntry::pv() const noexcept {
    return (generationBound8 & 0x04U) != 0;
}

std::uint8_t TTEntry::generation() const noexcept {
    return static_cast<std::uint8_t>(generationBound8 & 0xF8U);
}

TranspositionTable::TranspositionTable(std::size_t megabytes) {
    resize(megabytes);
}

void TranspositionTable::resize(std::size_t megabytes) {
    if (megabytes == 0) {
        std::vector<TTCluster>{}.swap(clusters_);
        clusterMask_ = 0;
        generation_ = 0;
        return;
    }

    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    const std::size_t bytes = megabytes > maximum / MEBIBYTE
        ? maximum
        : megabytes * MEBIBYTE;
    const std::size_t requestedClusters = bytes / sizeof(TTCluster);
    const std::size_t count = highestPowerOfTwo(requestedClusters);

    if (count == 0) {
        std::vector<TTCluster>{}.swap(clusters_);
        clusterMask_ = 0;
        generation_ = 0;
        return;
    }

    if (count == clusters_.size()) {
        generation_ = 0;
        clear();
        return;
    }

    // Allocate before swapping so allocation failure leaves the old table
    // intact. resize() is a cold configuration path, never a search hot path.
    std::vector<TTCluster> replacement(count);
    clusters_.swap(replacement);
    clusterMask_ = count - 1;
    generation_ = 0;
}

void TranspositionTable::clear() noexcept {
    std::fill(clusters_.begin(), clusters_.end(), TTCluster{});
}

void TranspositionTable::newSearch() noexcept {
    generation_ = static_cast<std::uint8_t>(
        (generation_ + GENERATION_DELTA) & GENERATION_MASK);

    // Five generation bits are stored in each entry. Clearing at wrap avoids
    // treating entries from 32 searches ago as if they were freshly written.
    if (generation_ == 0)
        clear();
}

TTEntry* TranspositionTable::probe(Key key, bool& found) noexcept {
    found = false;
    if (clusters_.empty())
        return nullptr;

    TTCluster& cluster = clusters_[clusterIndex(key)];
    const std::uint16_t tag = keyTag(key);

    for (TTEntry& entry : cluster.entry) {
        if (entry.occupied() && entry.key16 == tag) {
            found = true;
            entry.generationBound8 = static_cast<std::uint8_t>(
                generation_ | (entry.generationBound8 &
                               static_cast<std::uint8_t>(PV_MASK | BOUND_MASK)));
            return &entry;
        }
    }

    return replacementEntry(cluster);
}

TTProbe TranspositionTable::probe(Key key, int ply) noexcept {
    bool found = false;
    TTEntry* entry = probe(key, found);
    if (!found || entry == nullptr)
        return {};

    TTProbe result;
    result.hit = true;
    result.data.move = entry->move();
    result.data.value = valueFromTT(entry->storedValue(), ply);
    result.data.eval = entry->staticEval();
    result.data.depth = entry->depth();
    result.data.bound = entry->bound();
    result.data.pv = entry->pv();
    return result;
}

void TranspositionTable::store(Key key, const TTData& data, int ply) noexcept {
    store(key, data.value, data.pv, data.bound, data.depth,
          data.move, data.eval, ply);
}

void TranspositionTable::store(Key key, Value value, bool pv, TTBound bound,
                               Depth depth, Move move, Value eval,
                               int ply) noexcept {
    if (clusters_.empty() || bound == TTBound::None)
        return;

    TTCluster& cluster = clusters_[clusterIndex(key)];
    const std::uint16_t tag = keyTag(key);
    TTEntry* target = nullptr;
    bool sameKey = false;

    for (TTEntry& entry : cluster.entry) {
        if (entry.occupied() && entry.key16 == tag) {
            target = &entry;
            sameKey = true;
            break;
        }
    }

    if (target == nullptr)
        target = replacementEntry(cluster);

    if (sameKey) {
        const bool atLeastAsDeep = depth >= target->depth();
        const bool usefulExactUpgrade =
            bound == TTBound::Exact && target->bound() != TTBound::Exact &&
            depth + 4 >= target->depth();
        const bool sameOrShallower = depth <= target->depth();
        const bool losesExact = sameOrShallower &&
            target->bound() == TTBound::Exact && bound != TTBound::Exact;
        const bool losesPv = sameOrShallower && target->pv() && !pv &&
            !usefulExactUpgrade;
        if ((!atLeastAsDeep && !usefulExactUpgrade) || losesExact || losesPv) {
            if (target->move().isNone() && !move.isNone())
                target->move16 = move.raw();
            target->generationBound8 = static_cast<std::uint8_t>(
                generation_ |
                (target->generationBound8 &
                 static_cast<std::uint8_t>(PV_MASK | BOUND_MASK)));
            return;
        }
    }

    write(*target, tag, value, pv, bound, depth, move, eval, ply, sameKey);
}

int TranspositionTable::hashfull() const noexcept {
    if (clusters_.empty())
        return 0;

    constexpr std::size_t SAMPLE_ENTRIES = 1000;
    const std::size_t totalEntries = clusters_.size() * TTCluster::ENTRY_COUNT;
    const std::size_t sampleSize = std::min(SAMPLE_ENTRIES, totalEntries);
    std::size_t current = 0;
    std::size_t visited = 0;

    for (const TTCluster& cluster : clusters_) {
        for (const TTEntry& entry : cluster.entry) {
            if (visited == sampleSize)
                return static_cast<int>((current * 1000U) / sampleSize);

            if (entry.occupied() && entry.generation() == generation_)
                ++current;
            ++visited;
        }
    }

    return sampleSize == 0
        ? 0
        : static_cast<int>((current * 1000U) / sampleSize);
}

std::size_t TranspositionTable::sizeMegabytes() const noexcept {
    return sizeBytes() / MEBIBYTE;
}

Value TranspositionTable::valueToTT(Value value, int ply) noexcept {
    const int distance = boundedPly(ply);
    if (value >= MATE_TT_THRESHOLD)
        return value + distance;
    if (value <= -MATE_TT_THRESHOLD)
        return value - distance;
    return value;
}

Value TranspositionTable::valueFromTT(Value value, int ply) noexcept {
    const int distance = boundedPly(ply);
    if (value >= MATE_TT_THRESHOLD)
        return value - distance;
    if (value <= -MATE_TT_THRESHOLD)
        return value + distance;
    return value;
}

std::uint16_t TranspositionTable::keyTag(Key key) noexcept {
    return static_cast<std::uint16_t>(key >> 48U);
}

std::int16_t TranspositionTable::packValue(Value value) noexcept {
    constexpr Value minimum = static_cast<Value>(
        std::numeric_limits<std::int16_t>::min());
    constexpr Value maximum = static_cast<Value>(
        std::numeric_limits<std::int16_t>::max());
    return static_cast<std::int16_t>(std::max(minimum, std::min(value, maximum)));
}

std::int8_t TranspositionTable::packDepth(Depth depth) noexcept {
    constexpr Depth minimum = static_cast<Depth>(
        std::numeric_limits<std::int8_t>::min());
    constexpr Depth maximum = static_cast<Depth>(
        std::numeric_limits<std::int8_t>::max());
    return static_cast<std::int8_t>(std::max(minimum, std::min(depth, maximum)));
}

std::size_t TranspositionTable::clusterIndex(Key key) const noexcept {
    return static_cast<std::size_t>(key) & clusterMask_;
}

unsigned TranspositionTable::age(const TTEntry& entry) const noexcept {
    const unsigned current = generation_;
    const unsigned stored = entry.generation();
    return ((current - stored) & GENERATION_MASK) / GENERATION_DELTA;
}

int TranspositionTable::retentionQuality(const TTEntry& entry) const noexcept {
    if (!entry.occupied())
        return std::numeric_limits<int>::min();

    constexpr int AGE_PENALTY = 16;
    constexpr int EXACT_BONUS = 8;
    constexpr int PV_BONUS = 4;
    return entry.depth()
         + (entry.bound() == TTBound::Exact ? EXACT_BONUS : 0)
         + (entry.pv() ? PV_BONUS : 0)
         - static_cast<int>(age(entry)) * AGE_PENALTY;
}

TTEntry* TranspositionTable::replacementEntry(TTCluster& cluster) noexcept {
    TTEntry* replacement = &cluster.entry[0];
    int lowestQuality = retentionQuality(*replacement);

    for (std::size_t index = 1; index < TTCluster::ENTRY_COUNT; ++index) {
        TTEntry& candidate = cluster.entry[index];
        const int quality = retentionQuality(candidate);
        if (quality < lowestQuality) {
            replacement = &candidate;
            lowestQuality = quality;
        }
    }
    return replacement;
}

void TranspositionTable::write(TTEntry& entry, std::uint16_t tag,
                               Value value, bool pv, TTBound bound,
                               Depth depth, Move move, Value eval,
                               int ply, bool sameKey) noexcept {
    // A failed/shallow re-search often has no move. Preserve the useful move
    // already attached to the same position instead of replacing it by none.
    if (!(sameKey && move.isNone()))
        entry.move16 = move.raw();

    entry.key16 = tag;
    entry.value16 = packValue(valueToTT(value, ply));
    entry.eval16 = packValue(eval);
    entry.depth8 = packDepth(depth);
    entry.generationBound8 = static_cast<std::uint8_t>(
        generation_ | (pv ? PV_MASK : 0U) |
        (static_cast<std::uint8_t>(bound) & BOUND_MASK));
}

} // namespace Eunshin
