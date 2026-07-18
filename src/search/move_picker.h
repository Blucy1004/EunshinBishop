#pragma once

#include "core/move.h"
#include "core/types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Eunshin {

class HistoryTables;
class Position;

enum class PickerMode : std::uint8_t {
    MainSearch,
    Quiescence,
    Evasion
};

enum class MoveStage : std::uint8_t {
    TT,
    GoodCapture,
    Killer1,
    Killer2,
    CounterMove,
    Quiet,
    BadCapture,
    Done
};

// A fixed-capacity staged picker. Captures are generated only when their stage
// is reached, and the larger quiet list is not generated unless search asks
// past the tactical/special-move stages. No heap allocation or full-list sort
// occurs in the search hot path.
class MovePicker final {
public:
    MovePicker(Position& position,
               const HistoryTables& histories,
               Move ttMove = Move::none(),
               std::array<Move, 2> killers =
                   {{Move::none(), Move::none()}},
               Move counterMove = Move::none(),
               PickerMode mode = PickerMode::MainSearch,
               int quiescencePly = 0) noexcept;

    [[nodiscard]] Move next() noexcept;
    [[nodiscard]] MoveStage lastStage() const noexcept { return lastStage_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t returned() const noexcept { return returned_; }

private:
    struct ScoredMove final {
        Move move = Move::none();
        int score = 0;
        MoveStage stage = MoveStage::Done;
        bool consumed = false;
    };

    [[nodiscard]] ScoredMove* bestInStage(MoveStage stage) noexcept;
    [[nodiscard]] static MoveStage nextStage(MoveStage stage) noexcept;
    void ensureGenerated(MoveStage stage) noexcept;
    void generateCaptures() noexcept;
    void generateQuiets() noexcept;
    void generateSpecial(Move candidate, MoveStage stage) noexcept;
    void add(Move move, int score, MoveStage stage) noexcept;
    [[nodiscard]] bool eligible(Move move) noexcept;
    [[nodiscard]] bool contains(Move move) const noexcept;

    Position& position_;
    const HistoryTables& histories_;
    Move ttMove_ = Move::none();
    std::array<Move, 2> killers_{{Move::none(), Move::none()}};
    Move counterMove_ = Move::none();
    PickerMode mode_ = PickerMode::MainSearch;
    int quiescencePly_ = 0;
    bool quiescenceEvasion_ = false;

    std::array<ScoredMove, MAX_MOVES> moves_{};
    std::array<bool, 8> generated_{};
    std::size_t size_ = 0;
    std::size_t returned_ = 0;
    MoveStage stage_ = MoveStage::TT;
    MoveStage lastStage_ = MoveStage::Done;
};

} // namespace Eunshin
