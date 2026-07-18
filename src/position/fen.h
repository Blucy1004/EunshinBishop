#pragma once

#include <string>
#include <string_view>

namespace Eunshin {

class Position;
struct StateInfo;

inline constexpr std::string_view kStartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

bool setFromFen(Position& position,
                std::string_view fen,
                StateInfo& rootState,
                std::string* error = nullptr);

std::string toFen(const Position& position);

} // namespace Eunshin
