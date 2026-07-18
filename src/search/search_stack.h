#pragma once

#include "core/move.h"
#include "core/types.h"

#include <array>
#include <cstdint>
#include <type_traits>

namespace Eunshin {

enum class NodeType : std::uint8_t {
    Root,
    PV,
    NonPV
};

enum InstabilitySignal : std::uint16_t {
    StableNode = 0,
    TTEvalDisagreement = 1U << 0,
    EvaluationSwing = 1U << 1,
    KingPressure = 1U << 2,
    PasserRace = 1U << 3,
    RootIterationSwing = 1U << 4,
    ScoreGap = 1U << 5
};

struct SearchStack final {
    Move currentMove = Move::none();
    Move excludedMove = Move::none();
    std::array<Move, 2> killers{{Move::none(), Move::none()}};

    Value staticEval = VALUE_NONE;
    Value ttValue = VALUE_NONE;
    int ply = 0;
    int moveCount = 0;

    std::uint16_t instability = StableNode;
    std::uint8_t limboCooldown = 0;
    std::uint8_t limboChain = 0;

    bool inCheck = false;
    bool improving = false;
    bool ttPv = false;
};

static_assert(std::is_trivially_copyable<SearchStack>::value,
              "SearchStack must remain a small node-local value type");

} // namespace Eunshin
