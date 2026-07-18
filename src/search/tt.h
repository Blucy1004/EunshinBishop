#pragma once

#include "../core/move.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace Eunshin {

enum class TTBound : std::uint8_t {
    None = 0,
    Upper = 1,
    Lower = 2,
    Exact = 3
};

struct TTData final {
    Move move = Move::none();
    Value value = 0;
    Value eval = VALUE_NONE;
    Depth depth = 0;
    TTBound bound = TTBound::None;
    bool pv = false;
};

struct TTProbe final {
    bool hit = false;
    TTData data{};
};

// The explicit padding keeps one four-way cluster on one 64-byte cache line.
// generationBound8 layout: generation[7:3], pv[2], bound[1:0].
struct TTEntry final {
    std::uint16_t key16 = 0;
    std::uint16_t move16 = Move::none().raw();
    std::int16_t value16 = 0;
    std::int16_t eval16 = 0;
    std::int8_t depth8 = 0;
    std::uint8_t generationBound8 = 0;
    std::uint8_t reserved[6]{};

    [[nodiscard]] bool occupied() const noexcept;
    [[nodiscard]] Move move() const noexcept;
    [[nodiscard]] Value storedValue() const noexcept;
    [[nodiscard]] Value staticEval() const noexcept;
    [[nodiscard]] Depth depth() const noexcept;
    [[nodiscard]] TTBound bound() const noexcept;
    [[nodiscard]] bool pv() const noexcept;
    [[nodiscard]] std::uint8_t generation() const noexcept;
};

struct alignas(64) TTCluster final {
    static constexpr std::size_t ENTRY_COUNT = 4;
    TTEntry entry[ENTRY_COUNT]{};
};

static_assert(sizeof(TTEntry) == 16, "TTEntry must remain compact");
static_assert(sizeof(TTEntry) <= 16, "TTEntry must remain near 16 bytes");
static_assert(sizeof(TTCluster) == TTCluster::ENTRY_COUNT * sizeof(TTEntry),
              "TTCluster must contain exactly four entries without hidden padding");
static_assert(alignof(TTCluster) == 64, "TT clusters must remain cache-line aligned");
static_assert(std::is_trivially_copyable<TTEntry>::value,
              "TTEntry must remain trivially copyable");

class TranspositionTable final {
public:
    TranspositionTable() = default;
    explicit TranspositionTable(std::size_t megabytes);

    // A zero-sized table is a supported disabled state.
    void resize(std::size_t megabytes);
    void clear() noexcept;
    void newSearch() noexcept;

    // Compact/raw probe compatible with search hot paths. On a miss, returns
    // the deterministic replacement candidate. Returns nullptr when disabled.
    [[nodiscard]] TTEntry* probe(Key key, bool& found) noexcept;

    // Decoded probe. Mate scores are converted back to root-relative values
    // for the supplied ply.
    [[nodiscard]] TTProbe probe(Key key, int ply) noexcept;

    void store(Key key, const TTData& data, int ply) noexcept;
    void store(Key key, Value value, bool pv, TTBound bound, Depth depth,
               Move move, Value eval, int ply) noexcept;

    // UCI hashfull in permill, sampled deterministically from the beginning
    // of the table and counting only the current generation.
    [[nodiscard]] int hashfull() const noexcept;

    [[nodiscard]] bool enabled() const noexcept { return !clusters_.empty(); }
    [[nodiscard]] std::size_t clusterCount() const noexcept {
        return clusters_.size();
    }
    [[nodiscard]] std::size_t sizeBytes() const noexcept {
        return clusters_.size() * sizeof(TTCluster);
    }
    [[nodiscard]] std::size_t sizeMegabytes() const noexcept;
    [[nodiscard]] std::uint8_t currentGeneration() const noexcept {
        return generation_;
    }

    [[nodiscard]] static Value valueToTT(Value value, int ply) noexcept;
    [[nodiscard]] static Value valueFromTT(Value value, int ply) noexcept;

private:
    static constexpr std::uint8_t BOUND_MASK = 0x03U;
    static constexpr std::uint8_t PV_MASK = 0x04U;
    static constexpr std::uint8_t GENERATION_MASK = 0xF8U;
    static constexpr std::uint8_t GENERATION_DELTA = 0x08U;

    [[nodiscard]] static std::uint16_t keyTag(Key key) noexcept;
    [[nodiscard]] static std::int16_t packValue(Value value) noexcept;
    [[nodiscard]] static std::int8_t packDepth(Depth depth) noexcept;
    [[nodiscard]] std::size_t clusterIndex(Key key) const noexcept;
    [[nodiscard]] unsigned age(const TTEntry& entry) const noexcept;
    [[nodiscard]] int retentionQuality(const TTEntry& entry) const noexcept;
    [[nodiscard]] TTEntry* replacementEntry(TTCluster& cluster) noexcept;
    void write(TTEntry& entry, std::uint16_t tag, Value value, bool pv,
               TTBound bound, Depth depth, Move move, Value eval,
               int ply, bool sameKey) noexcept;

    std::vector<TTCluster> clusters_{};
    std::size_t clusterMask_ = 0;
    std::uint8_t generation_ = 0;
};

} // namespace Eunshin
