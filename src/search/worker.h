#pragma once

#include "position/state.h"
#include "search/history.h"
#include "search/search_config.h"
#include "search/search_stack.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace Eunshin {

class Evaluator;
class TranspositionTable;

struct SearchStatistics final {
    std::uint64_t nodes = 0;
    std::uint64_t qnodes = 0;
    std::uint64_t qCheckNodes = 0;
    std::uint64_t qCheckmates = 0;
    std::uint64_t ttHits = 0;
    std::uint64_t betaCutoffs = 0;

    std::uint64_t aegisTriggers = 0;
    std::uint64_t aegisIIRBlocks = 0;
    std::uint64_t aegisLMRReliefs = 0;
    std::array<std::uint64_t, 6> aegisSignals{};

    std::uint64_t limboChecks = 0;
    std::uint64_t limboTriggers = 0;
    std::uint64_t limboExtensions = 0;
    std::uint64_t limboBoundedResearches = 0;
    std::uint64_t limboSuppressedByCooldown = 0;

    void clear() noexcept { *this = SearchStatistics{}; }
};

class SearchWorker final {
public:
    static constexpr int STACK_PADDING = 4;
    static constexpr int STACK_SIZE = MAX_PLY + 8;

    explicit SearchWorker(TranspositionTable& table) noexcept;

    SearchWorker(const SearchWorker&) = delete;
    SearchWorker& operator=(const SearchWorker&) = delete;

    void bindEvaluator(Evaluator& evaluator) noexcept { evaluator_ = &evaluator; }
    [[nodiscard]] bool hasEvaluator() const noexcept { return evaluator_ != nullptr; }

    // Returns false if a previous search session has not been completed.
    [[nodiscard]] bool beginSearch(const SearchConfig& config) noexcept;
    void finishSearch() noexcept {
        searching_.store(false, std::memory_order_release);
    }
    void resetForNewGame() noexcept;

    void requestStop() noexcept { stopRequested_.store(true, std::memory_order_relaxed); }
    void clearStop() noexcept { stopRequested_.store(false, std::memory_order_relaxed); }
    [[nodiscard]] bool stopRequested() const noexcept {
        return stopRequested_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] const std::atomic_bool& stopFlag() const noexcept {
        return stopRequested_;
    }
    [[nodiscard]] bool isSearching() const noexcept {
        return searching_.load(std::memory_order_acquire);
    }

    [[nodiscard]] const SearchConfig& config() const noexcept { return config_; }
    [[nodiscard]] HistoryTables& histories() noexcept { return histories_; }
    [[nodiscard]] const HistoryTables& histories() const noexcept { return histories_; }
    [[nodiscard]] SearchStatistics& statistics() noexcept { return statistics_; }
    [[nodiscard]] const SearchStatistics& statistics() const noexcept {
        return statistics_;
    }

    [[nodiscard]] StateInfo& stateAt(int ply) noexcept;
    [[nodiscard]] SearchStack& stackAt(int ply) noexcept;
    [[nodiscard]] const SearchStack& stackAt(int ply) const noexcept;

    [[nodiscard]] TranspositionTable& table() noexcept { return table_; }
    [[nodiscard]] Evaluator* evaluator() noexcept { return evaluator_; }

private:
    TranspositionTable& table_;
    Evaluator* evaluator_ = nullptr;

    SearchConfig config_{};
    HistoryTables histories_{};
    SearchStatistics statistics_{};
    std::array<StateInfo, STACK_SIZE> states_{};
    std::array<SearchStack, STACK_SIZE + STACK_PADDING> stack_{};
    std::atomic_bool stopRequested_{false};
    std::atomic_bool searching_{false};
};

} // namespace Eunshin
