// Temporary diagnostic tool for Checkpoint 8's perft profiling. Not part of
// the shipped project; lives only in this throwaway worktree.
#include "core/attacks.h"
#include "core/random.h"
#include "core/zobrist.h"
#include "position/fen.h"
#include "position/movegen.h"
#include "position/position.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Eunshin;
using Clock = std::chrono::steady_clock;

namespace Eunshin { namespace Profile {
std::uint64_t g_pseudoLegalNs = 0;
std::uint64_t g_legalityCheckNs = 0;
std::uint64_t g_checkersNs = 0;
std::uint64_t g_domoveCalls = 0;
} }

namespace {

std::uint64_t g_genMovesNs = 0;
std::uint64_t g_genMovesCalls = 0;
std::uint64_t g_doMoveNs = 0;
std::uint64_t g_doMoveCalls = 0;
std::uint64_t g_undoMoveNs = 0;
std::uint64_t g_undoMoveCalls = 0;

std::uint64_t nowNs() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
}

std::uint64_t perftProfiled(Position& position, Depth depth) noexcept {
    if (depth <= 0) return 1;
    MoveList pseudo;

    std::uint64_t t0 = nowNs();
    generateMoves(position, pseudo, GenerationType::All);
    g_genMovesNs += nowNs() - t0;
    ++g_genMovesCalls;

    std::uint64_t nodes = 0;
    for (std::size_t i = 0; i < pseudo.size; ++i) {
        const Move move = pseudo[i];
        StateInfo next;

        t0 = nowNs();
        const bool ok = position.doMove(move, next);
        g_doMoveNs += nowNs() - t0;
        ++g_doMoveCalls;
        if (!ok) continue;

        nodes += perftProfiled(position, depth - 1);

        t0 = nowNs();
        position.undoMove(move);
        g_undoMoveNs += nowNs() - t0;
        ++g_undoMoveCalls;
    }
    return nodes;
}

} // namespace

int main(int argc, char** argv) {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    Random::reset();
    Attacks::initialize();
    Zobrist::initialize();

    const int depth = argc > 1 ? std::atoi(argv[1]) : 5;

    Position position;
    StateInfo root;
    std::string error;
    setFromFen(position,
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        root, &error);

    const std::uint64_t wall0 = nowNs();
    const std::uint64_t nodes = perftProfiled(position, depth);
    const std::uint64_t wallNs = nowNs() - wall0;

    const std::uint64_t accounted = g_genMovesNs + g_doMoveNs + g_undoMoveNs;
    const std::uint64_t other = wallNs > accounted ? wallNs - accounted : 0;

    std::printf("perft(%d) = %llu\n", depth,
                static_cast<unsigned long long>(nodes));
    std::printf("total:               %.1f ms\n", wallNs / 1e6);
    std::printf("generateMoves:       %.1f ms, %llu calls, %.1f ns avg, %.1f%% of total\n",
                g_genMovesNs / 1e6,
                static_cast<unsigned long long>(g_genMovesCalls),
                g_genMovesCalls ? double(g_genMovesNs) / double(g_genMovesCalls) : 0.0,
                100.0 * double(g_genMovesNs) / double(wallNs));
    std::printf("Position::doMove:    %.1f ms, %llu calls, %.1f ns avg, %.1f%% of total\n",
                g_doMoveNs / 1e6,
                static_cast<unsigned long long>(g_doMoveCalls),
                g_doMoveCalls ? double(g_doMoveNs) / double(g_doMoveCalls) : 0.0,
                100.0 * double(g_doMoveNs) / double(wallNs));
    std::printf("Position::undoMove:  %.1f ms, %llu calls, %.1f ns avg, %.1f%% of total\n",
                g_undoMoveNs / 1e6,
                static_cast<unsigned long long>(g_undoMoveCalls),
                g_undoMoveCalls ? double(g_undoMoveNs) / double(g_undoMoveCalls) : 0.0,
                100.0 * double(g_undoMoveNs) / double(wallNs));
    std::printf("unaccounted (loop/recursion/timer overhead): %.1f ms, %.1f%% of total\n",
                other / 1e6, 100.0 * double(other) / double(wallNs));

#ifdef EUNSHIN_PROFILE_DOMOVE
    std::printf("\n-- doMove breakdown (%llu calls) --\n",
                static_cast<unsigned long long>(Profile::g_domoveCalls));
    std::printf("  isPseudoLegal:          %.1f ms, %.1f%% of total, %.1f%% of doMove\n",
                Profile::g_pseudoLegalNs / 1e6,
                100.0 * double(Profile::g_pseudoLegalNs) / double(wallNs),
                100.0 * double(Profile::g_pseudoLegalNs) / double(g_doMoveNs));
    std::printf("  legality check:         %.1f ms, %.1f%% of total, %.1f%% of doMove\n",
                Profile::g_legalityCheckNs / 1e6,
                100.0 * double(Profile::g_legalityCheckNs) / double(wallNs),
                100.0 * double(Profile::g_legalityCheckNs) / double(g_doMoveNs));
    std::printf("  checkers (attackersTo): %.1f ms, %.1f%% of total, %.1f%% of doMove\n",
                Profile::g_checkersNs / 1e6,
                100.0 * double(Profile::g_checkersNs) / double(wallNs),
                100.0 * double(Profile::g_checkersNs) / double(g_doMoveNs));
    const std::uint64_t domoveAccounted = Profile::g_pseudoLegalNs +
        Profile::g_legalityCheckNs + Profile::g_checkersNs;
    const std::uint64_t domoveOther = g_doMoveNs > domoveAccounted
        ? g_doMoveNs - domoveAccounted : 0;
    std::printf("  rest of doMove (hashing, board mutation, bookkeeping): %.1f ms, %.1f%% of total, %.1f%% of doMove\n",
                domoveOther / 1e6,
                100.0 * double(domoveOther) / double(wallNs),
                100.0 * double(domoveOther) / double(g_doMoveNs));
#endif
    return 0;
}
