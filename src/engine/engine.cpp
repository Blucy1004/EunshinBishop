#include "engine/engine.h"

#include "core/attacks.h"
#include "core/random.h"
#include "core/zobrist.h"
#include "position/fen.h"
#include "position/movegen.h"

#include <exception>
#include <filesystem>
#include <new>
#include <type_traits>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace Eunshin {
namespace {

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

} // namespace

static_assert(std::is_nothrow_move_assignable<EngineOptions>::value,
              "EngineOptions must commit transactionally without throwing");

Engine::Engine() : worker_(table_) {
    worker_.bindEvaluator(evaluator_);
}

bool Engine::initialize(std::string* error) {
    const std::lock_guard<std::mutex> lock(controlMutex_);
    if (error) error->clear();
    if (initialized_) return true;

    if (!Attacks::initialized()) {
        Random::reset();
        Attacks::initialize();
    }
    if (!Zobrist::initialized()) Zobrist::initialize();

    try {
        gameStates_.reserve(MAX_GAME_PLIES + 1);
        if (gameStates_.empty()) gameStates_.emplace_back();
        table_.resize(options_.hashMegabytes);
    } catch (const std::exception& exception) {
        return fail(error, "engine allocation failed: " +
                           std::string(exception.what()));
    }
    initialized_ = true;
    if (!newGameUnlocked(error)) {
        initialized_ = false;
        return false;
    }
    if (options_.useNNUE)
        reloadNetworkUnlocked();
    else
        nnueStatus_ = "NNUE disabled; using classical evaluator";
    if (error) error->clear();
    return true;
}

bool Engine::newGame(std::string* error) {
    const std::lock_guard<std::mutex> lock(controlMutex_);
    return newGameUnlocked(error);
}

bool Engine::newGameUnlocked(std::string* error) {
    if (error) error->clear();
    if (!initialized_) return fail(error, "Engine is not initialized");
    if (worker_.isSearching())
        return fail(error, "cannot start a new game while search is active");
    if (!setPositionUnlocked(kStartFen, error)) return false;
    table_.clear();
    worker_.resetForNewGame();
    return true;
}

bool Engine::setPosition(std::string_view fen, std::string* error) {
    const std::lock_guard<std::mutex> lock(controlMutex_);
    return setPositionUnlocked(fen, error);
}

bool Engine::setPositionUnlocked(std::string_view fen, std::string* error) {
    if (error) error->clear();
    if (!initialized_) return fail(error, "Engine is not initialized");
    if (worker_.isSearching())
        return fail(error, "cannot change position while search is active");
    if (gameStates_.empty()) gameStates_.emplace_back();

    if (!setFromFen(position_, fen, gameStates_.front(), error)) return false;
    gameStates_.resize(1);
    return true;
}

bool Engine::applyMove(std::string_view uci, std::string* error) {
    const std::lock_guard<std::mutex> lock(controlMutex_);
    return applyMoveUnlocked(uci, error);
}

bool Engine::applyMoveUnlocked(std::string_view uci, std::string* error) {
    if (error) error->clear();
    if (!initialized_) return fail(error, "Engine is not initialized");
    if (worker_.isSearching())
        return fail(error, "cannot apply a move while search is active");
    if (gameStates_.size() >= MAX_GAME_PLIES + 1)
        return fail(error, "game StateInfo capacity exceeded");

    const Move move = moveFromUci(position_, uci);
    if (move.isNone())
        return fail(error, "illegal UCI move: " + std::string(uci));

    gameStates_.emplace_back();
    if (!position_.doMove(move, gameStates_.back())) {
        gameStates_.pop_back();
        return fail(error, "move became illegal during application: " +
                           std::string(uci));
    }
    return true;
}

bool Engine::applyMoves(const std::vector<std::string_view>& moves,
                        std::string* error) {
    const std::lock_guard<std::mutex> lock(controlMutex_);
    if (error) error->clear();
    if (!initialized_) return fail(error, "Engine is not initialized");
    if (worker_.isSearching())
        return fail(error, "cannot apply moves while search is active");
    const std::size_t originalCount = gameStates_.size();
    for (const std::string_view move : moves) {
        if (!applyMoveUnlocked(move, error)) {
            rollbackTo(originalCount);
            return false;
        }
    }
    return true;
}

bool Engine::setOption(std::string_view name,
                       std::string_view value,
                       std::string* error) {
    const std::lock_guard<std::mutex> lock(controlMutex_);
    if (worker_.isSearching())
        return fail(error, "cannot change options while search is active");

    EngineOptions candidate = options_;
    if (!candidate.set(name, value, error)) return false;
    if (candidate.revision == options_.revision) return true;

    // These switches belong to later numbered checkpoints. Reject enabling
    // them instead of accepting a silent no-op in a public UCI engine.
    if (candidate.useSEEPruning)
        return fail(error, "SEEPruning is not implemented in checkpoint 3");
    if (candidate.useLIMBO)
        return fail(error, "LIMBO is not implemented in checkpoint 3");

    const bool hashChanged = candidate.hashMegabytes != options_.hashMegabytes;
    const bool evalFileChanged = candidate.evalFile != options_.evalFile;
    const bool nnueEnabled = candidate.useNNUE && !options_.useNNUE;
    const bool nnueDisabled = !candidate.useNNUE && options_.useNNUE;
    if (initialized_ && hashChanged) {
        try {
            table_.resize(candidate.hashMegabytes);
        } catch (const std::exception& exception) {
            return fail(error, "Hash allocation failed: " +
                               std::string(exception.what()));
        }
    }

    options_ = std::move(candidate);
    if (initialized_) {
        if (!hashChanged) table_.clear();

        if (evalFileChanged) {
            // Engine-level policy intentionally deactivates mismatched old
            // weights. A failed replacement therefore has one unambiguous
            // state: the requested path plus Classical fallback.
            evaluator_.unloadNetwork();
            if (options_.useNNUE)
                reloadNetworkUnlocked();
            else
                nnueStatus_ = "EvalFile changed while NNUE is disabled; "
                              "using classical evaluator";
        } else if (nnueEnabled && !evaluator_.network().ready()) {
            reloadNetworkUnlocked();
        } else if (nnueDisabled) {
            nnueStatus_ = "NNUE disabled; using classical evaluator";
        }
    }
    if (error) error->clear();
    return true;
}

SearchResult Engine::go(const SearchLimits& limits,
                        SearchInfoCallback callback,
                        void* userData,
                        std::string* error) {
    SearchResult rejected;
    rejected.stopReason = StopReason::Rejected;
    Position searchPosition;

    {
        const std::lock_guard<std::mutex> lock(controlMutex_);
        if (error) error->clear();
        if (!initialized_) {
            fail(error, "Engine is not initialized");
            return rejected;
        }
        if (!worker_.beginSearch(options_.snapshot())) {
            fail(error, "search is already active");
            return rejected;
        }
        goOwnsSession_ = true;
        table_.newSearch();
        searchPosition = position_.snapshotForSearch(worker_.stateAt(0));
    }

    SearchResult result = runSearch(searchPosition, worker_, limits,
                                    callback, userData);
    {
        const std::lock_guard<std::mutex> lock(controlMutex_);
        worker_.finishSearch();
        goOwnsSession_ = false;
    }
    return result;
}

SearchWorker* Engine::prepareSearch(std::string* error) {
    const std::lock_guard<std::mutex> lock(controlMutex_);
    if (!initialized_) {
        fail(error, "Engine is not initialized");
        return nullptr;
    }
    if (!worker_.beginSearch(options_.snapshot())) {
        fail(error, "search is already active");
        return nullptr;
    }
    goOwnsSession_ = false;
    table_.newSearch();
    if (error) error->clear();
    return &worker_;
}

void Engine::finishSearch() {
    const std::lock_guard<std::mutex> lock(controlMutex_);
    if (goOwnsSession_) return;
    worker_.finishSearch();
}

bool Engine::reloadNetworkUnlocked() {
    const std::string resolved = resolveNetworkPathUnlocked();
    const NNUE::LoadResult load = evaluator_.loadNetwork(resolved);
    if (load.success) {
        nnueStatus_ = load.message;
        return true;
    }

    // Network::load is transactional, but Engine unloads before replacement
    // attempts so a failed configured file cannot secretly use old weights.
    std::string detail = evaluator_.network().lastError();
    if (detail.empty()) detail = load.message;
    evaluator_.unloadNetwork();
    nnueStatus_ = "NNUE load failed for '" + options_.evalFile + "': "
                + detail + "; using classical fallback";
    return false;
}

std::string Engine::resolveNetworkPathUnlocked() const {
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path requested = fs::u8path(options_.evalFile);
    if (requested.is_absolute() || fs::exists(requested, error))
        return requested.u8string();

    error.clear();
    const fs::path sourceTreeCandidate = fs::current_path(error)
        / "networks" / requested;
    if (!error && fs::exists(sourceTreeCandidate, error))
        return sourceTreeCandidate.u8string();

#if defined(_WIN32)
    std::wstring modulePath(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (length > 0 && static_cast<std::size_t>(length) < modulePath.size()) {
        modulePath.resize(length);
        error.clear();
        const fs::path besideExecutable =
            fs::path(modulePath).parent_path() / requested;
        if (fs::exists(besideExecutable, error))
            return besideExecutable.u8string();
    }
#endif

    return requested.u8string();
}

void Engine::rollbackTo(std::size_t stateCount) noexcept {
    while (gameStates_.size() > stateCount && position_.state() &&
           position_.state()->previous) {
        const Move move = position_.state()->move;
        position_.undoMove(move);
        gameStates_.pop_back();
    }
}

} // namespace Eunshin
