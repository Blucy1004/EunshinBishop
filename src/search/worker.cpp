#include "search/worker.h"

#include "search/tt.h"

#include <algorithm>
#include <cassert>

namespace Eunshin {

SearchWorker::SearchWorker(TranspositionTable& table) noexcept : table_(table) {
    resetForNewGame();
}

bool SearchWorker::beginSearch(const SearchConfig& config) noexcept {
    bool expected = false;
    if (!searching_.compare_exchange_strong(expected, true,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
        return false;
    config_ = config;
    statistics_.clear();
    clearStop();
    for (SearchStack& entry : stack_) entry = SearchStack{};
    for (int ply = 0; ply < STACK_SIZE; ++ply)
        stackAt(ply).ply = ply;
    return true;
}

void SearchWorker::resetForNewGame() noexcept {
    searching_.store(false, std::memory_order_release);
    histories_.clear();
    statistics_.clear();
    clearStop();
    for (StateInfo& state : states_) state = StateInfo{};
    for (SearchStack& entry : stack_) entry = SearchStack{};
    for (int ply = 0; ply < STACK_SIZE; ++ply)
        stackAt(ply).ply = ply;
}

StateInfo& SearchWorker::stateAt(int ply) noexcept {
    assert(ply >= 0 && ply < STACK_SIZE);
    const int bounded = std::max(0, std::min(ply, STACK_SIZE - 1));
    return states_[static_cast<std::size_t>(bounded)];
}

SearchStack& SearchWorker::stackAt(int ply) noexcept {
    assert(ply >= -STACK_PADDING && ply < STACK_SIZE);
    const int bounded = std::max(-STACK_PADDING,
                                 std::min(ply, STACK_SIZE - 1));
    return stack_[static_cast<std::size_t>(bounded + STACK_PADDING)];
}

const SearchStack& SearchWorker::stackAt(int ply) const noexcept {
    assert(ply >= -STACK_PADDING && ply < STACK_SIZE);
    const int bounded = std::max(-STACK_PADDING,
                                 std::min(ply, STACK_SIZE - 1));
    return stack_[static_cast<std::size_t>(bounded + STACK_PADDING)];
}

} // namespace Eunshin
