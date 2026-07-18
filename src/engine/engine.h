#pragma once

#include "eval/evaluator.h"
#include "engine/options.h"
#include "position/position.h"
#include "search/search.h"
#include "search/time_manager.h"
#include "search/tt.h"
#include "search/worker.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Eunshin {

// Engine is the only frontend-facing owner of mutable game/search state.
class Engine final {
public:
    static constexpr std::size_t MAX_GAME_PLIES = 8192;

    Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool initialize(std::string* error = nullptr);
    bool newGame(std::string* error = nullptr);

    bool setPosition(std::string_view fen, std::string* error = nullptr);
    bool applyMove(std::string_view uci, std::string* error = nullptr);
    bool applyMoves(const std::vector<std::string_view>& moves,
                    std::string* error = nullptr);

    bool setOption(std::string_view name,
                   std::string_view value,
                   std::string* error = nullptr);

    // Synchronous search boundary. A protocol frontend may run this method on
    // one joinable thread and use stop() from its input thread.
    [[nodiscard]] SearchResult go(
        const SearchLimits& limits,
        SearchInfoCallback callback = nullptr,
        void* userData = nullptr,
        std::string* error = nullptr);

    // Freezes the current options into the Worker and advances TT generation.
    // The caller must pair a successful result with finishSearch().
    // This is the cold setup boundary immediately preceding the future go().
    [[nodiscard]] SearchWorker* prepareSearch(std::string* error = nullptr);
    void finishSearch();
    void stop() noexcept { worker_.requestStop(); }

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool isSearching() const noexcept { return worker_.isSearching(); }
    [[nodiscard]] bool nnueLoaded() const noexcept {
        return evaluator_.network().ready();
    }
    [[nodiscard]] std::string nnueStatusMessage() const { return nnueStatus_; }
    [[nodiscard]] const EngineOptions& options() const noexcept { return options_; }
    [[nodiscard]] const Position& position() const noexcept { return position_; }
    [[nodiscard]] std::size_t gamePly() const noexcept {
        return gameStates_.empty() ? 0 : gameStates_.size() - 1;
    }
    [[nodiscard]] const TranspositionTable& table() const noexcept { return table_; }
    [[nodiscard]] const SearchWorker& worker() const noexcept { return worker_; }
    [[nodiscard]] const Evaluator& evaluator() const noexcept { return evaluator_; }

private:
    bool newGameUnlocked(std::string* error);
    bool setPositionUnlocked(std::string_view fen, std::string* error);
    bool applyMoveUnlocked(std::string_view uci, std::string* error);
    void rollbackTo(std::size_t stateCount) noexcept;
    bool reloadNetworkUnlocked();
    [[nodiscard]] std::string resolveNetworkPathUnlocked() const;

    // Serializes frontend/control-plane mutations. It is never acquired by a
    // search node and therefore does not enter the hot path.
    std::mutex controlMutex_{};
    EngineOptions options_{};
    Position position_{};
    std::vector<StateInfo> gameStates_{};
    TranspositionTable table_{};
    Evaluator evaluator_{};
    SearchWorker worker_;
    std::string nnueStatus_ = "NNUE has not been initialized";
    bool initialized_ = false;
    bool goOwnsSession_ = false;
};

} // namespace Eunshin
