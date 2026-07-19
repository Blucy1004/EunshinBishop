#include "uci/uci.h"

#include "core/move.h"
#include "core/version.h"
#include "engine/engine.h"
#include "eval/score.h"
#include "position/fen.h"
#include "position/movegen.h"
#include "search/search.h"
#include "search/see.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <istream>
#include <limits>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace Eunshin::UCI {
namespace {

constexpr int MAX_SUPPORTED_SEARCH_DEPTH = MAX_PLY - 8;

[[nodiscard]] std::string lowerAscii(std::string_view text) {
    std::string result(text);
    for (char& character : result) {
        const unsigned char value = static_cast<unsigned char>(character);
        character = static_cast<char>(std::tolower(value));
    }
    return result;
}

[[nodiscard]] std::vector<std::string> splitWords(std::string_view line) {
    std::istringstream stream{std::string(line)};
    std::vector<std::string> words;
    std::string word;
    while (stream >> word) words.push_back(std::move(word));
    return words;
}

[[nodiscard]] std::string joinWords(const std::vector<std::string>& words,
                                    std::size_t first,
                                    std::size_t last) {
    std::string result;
    for (std::size_t index = first; index < last; ++index) {
        if (!result.empty()) result.push_back(' ');
        result += words[index];
    }
    return result;
}

[[nodiscard]] std::string stripOptionalQuotes(std::string text) {
    if (text.size() >= 2 &&
        ((text.front() == '"' && text.back() == '"') ||
         (text.front() == '\'' && text.back() == '\''))) {
        text.erase(text.begin());
        text.pop_back();
    }
    return text;
}

template <typename Integer>
[[nodiscard]] bool parseInteger(std::string_view text, Integer& result) noexcept {
    static_assert(std::numeric_limits<Integer>::is_integer,
                  "UCI numeric parsing requires an integer type");
    if (text.empty()) return false;
    Integer parsed{};
    const char* const first = text.data();
    const char* const last = first + text.size();
    const auto conversion = std::from_chars(first, last, parsed);
    if (conversion.ec != std::errc{} || conversion.ptr != last) return false;
    result = parsed;
    return true;
}

[[nodiscard]] std::string protocolSafe(std::string text) {
    for (char& character : text)
        if (character == '\r' || character == '\n') character = ' ';
    return text;
}

[[nodiscard]] std::uint64_t nodesPerSecond(std::uint64_t nodes,
                                           std::int64_t timeMs) noexcept {
    if (timeMs <= 0) return 0;
    const std::uint64_t elapsed = static_cast<std::uint64_t>(timeMs);
    const std::uint64_t whole = nodes / elapsed;
    const std::uint64_t remainder = nodes % elapsed;
    if (whole > std::numeric_limits<std::uint64_t>::max() / 1000)
        return std::numeric_limits<std::uint64_t>::max();
    const std::uint64_t base = whole * 1000;
    const std::uint64_t fraction = remainder * 1000 / elapsed;
    if (base > std::numeric_limits<std::uint64_t>::max() - fraction)
        return std::numeric_limits<std::uint64_t>::max();
    return base + fraction;
}

[[nodiscard]] std::string toHex(Key key) {
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << key;
    return stream.str();
}

[[nodiscard]] std::string formatCentipawns(Value value) {
    return (value >= 0 ? "+" : "") + std::to_string(value) + " cp";
}

class Session final {
public:
    Session(std::istream& input, std::ostream& output) noexcept
        : input_(input), output_(output) {}

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    ~Session() {
        stopAndJoin();
    }

    int run() {
        std::string error;
        if (!engine_.initialize(&error)) {
            writeLine("info string engine initialization failed: " +
                      protocolSafe(error));
            return 1;
        }

        std::string line;
        while (std::getline(input_, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (handle(line)) break;
        }

        stopAndJoin();
        return 0;
    }

private:
    static void searchInfoCallback(const SearchInfo& info,
                                   void* context) noexcept {
        if (!context) return;
        try {
            static_cast<Session*>(context)->writeSearchInfo(info);
        } catch (...) {
            // A reporting allocation failure must never terminate search or
            // suppress its eventual bestmove.
        }
    }

    void writeLine(std::string_view line) noexcept {
        try {
            const std::lock_guard<std::mutex> lock(outputMutex_);
            output_ << line << '\n';
            output_.flush();
        } catch (...) {
            // Protocol output failures are external I/O failures.  Search must
            // still unwind and its thread must remain joinable.
        }
    }

    void writeSearchInfo(const SearchInfo& info) {
        std::ostringstream line;
        line << "info depth " << info.depth;
        if (info.selDepth > 0) line << " seldepth " << info.selDepth;

        if (info.score != VALUE_NONE) {
            if (info.score >= MATE - MAX_PLY ||
                info.score <= -MATE + MAX_PLY) {
                const int distance =
                    (MATE - std::abs(info.score) + 1) / 2;
                line << " score mate " << (info.score > 0 ? distance : -distance);
            } else {
                line << " score cp " << info.score;
            }
        }

        line << " nodes " << info.nodes
             << " nps " << nodesPerSecond(info.nodes, info.timeMs)
             << " hashfull " << info.hashfull
             << " time " << std::max<std::int64_t>(0, info.timeMs);

        const int pvLength = std::max(0, std::min(info.pv.length, MAX_PLY));
        if (pvLength > 0) {
            line << " pv";
            for (int index = 0; index < pvLength; ++index) {
                const Move move = info.pv.moves[static_cast<std::size_t>(index)];
                if (move.isNone()) break;
                line << ' ' << moveToUci(move);
            }
        }
        writeLine(line.str());
    }

    void writeBestMove(const SearchResult& result) noexcept {
        try {
            std::string line = "bestmove " + moveToUci(result.bestMove);
            const Move ponder = !result.ponder.isNone()
                ? result.ponder : result.pv.ponder();
            if (!ponder.isNone()) line += " ponder " + moveToUci(ponder);
            writeLine(line);
        } catch (...) {
            writeLine("bestmove 0000");
        }
    }

    void reportNNUEFallbackIfNeeded() {
        if (!engine_.options().useNNUE || engine_.nnueLoaded()) {
            reportedNNUEFailure_.clear();
            return;
        }

        std::string status = protocolSafe(engine_.nnueStatusMessage());
        if (status.empty()) status = "NNUE network unavailable";
        if (status == reportedNNUEFailure_) return;
        reportedNNUEFailure_ = status;
        if (lowerAscii(status).find("fallback") == std::string::npos)
            status += "; using classical fallback";
        writeLine("info string " + status);
    }

    void printIdentityAndOptions() {
        writeLine("id name " + Version::idString());
        writeLine(std::string("id author ") + std::string(Version::Author));
        writeLine("option name Hash type spin default 256 min 1 max 4096");
        writeLine("option name MoveOverhead type spin default 30 min 0 max 5000");
        writeLine("option name UseNNUE type check default true");
        writeLine("option name EvalFile type string default firstnet_v5_10b.snnue");
        writeLine("option name NNUEOutputMode type combo default Residual var Residual var Absolute");
        writeLine("option name ResidualScale type spin default 100 min 0 max 200");
        writeLine("option name ResidualGuard type check default true");
        writeLine("option name AbsoluteBlend type spin default 35 min 0 max 100");
        writeLine("option name IIR type check default false");
        writeLine("option name AEGIS type check default false");
        writeLine("option name SEEPruning type check default false");
        writeLine("option name LIMBO type check default false");
        reportNNUEFallbackIfNeeded();
        writeLine("uciok");
    }

    void stopAndJoin() noexcept {
        if (!searchThread_.joinable()) return;
        engine_.stop();
        // Session is the sole thread owner and stopAndJoin is called only by
        // the protocol thread, so neither invalid-thread nor self-join applies.
        searchThread_.join();
    }

    void waitForSearchActivation() {
        std::unique_lock<std::mutex> lock(searchStateMutex_);
        while (!searchFinished_ && !engine_.isSearching())
            searchStateChanged_.wait_for(lock, std::chrono::milliseconds(1));
    }

    void startSearch(const SearchLimits& limits) {
        stopAndJoin();
        {
            const std::lock_guard<std::mutex> lock(searchStateMutex_);
            searchFinished_ = false;
        }

        try {
            searchThread_ = std::thread([this, limits]() noexcept {
                SearchResult result{};
                try {
                    result = engine_.go(limits, &Session::searchInfoCallback, this);
                    writeBestMove(result);
                } catch (const std::exception& exception) {
                    writeLine("info string search failed: " +
                              protocolSafe(exception.what()));
                    writeLine("bestmove 0000");
                } catch (...) {
                    writeLine("info string search failed: unknown exception");
                    writeLine("bestmove 0000");
                }

                {
                    const std::lock_guard<std::mutex> lock(searchStateMutex_);
                    searchFinished_ = true;
                }
                searchStateChanged_.notify_all();
            });
        } catch (const std::system_error& exception) {
            writeLine("info string search thread unavailable: " +
                      protocolSafe(exception.what()) +
                      "; running one emergency iteration synchronously");
            SearchLimits emergencyLimits;
            emergencyLimits.depth = 1;
            const SearchResult result =
                engine_.go(emergencyLimits, &Session::searchInfoCallback, this);
            writeBestMove(result);
            const std::lock_guard<std::mutex> lock(searchStateMutex_);
            searchFinished_ = true;
            return;
        }
        waitForSearchActivation();
    }

    // Specification section 26 debug/diagnostic commands. None of these are
    // standard UCI; every line is still framed as "info string" so a strict
    // GUI parser can safely ignore them. None auto-stops an active search --
    // Engine's debug methods reject cleanly instead, since force-stopping a
    // search to answer an unrelated diagnostic query would be surprising.
    void handlePerft(const std::vector<std::string>& words) {
        int depth = 0;
        if (words.size() < 2 || !parseInteger(words[1], depth) ||
            depth < 1 || depth > 9) {
            writeLine("info string perft requires an integer depth from 1 to 9");
            return;
        }

        std::string error;
        const auto start = std::chrono::steady_clock::now();
        const std::uint64_t nodes = engine_.debugPerft(depth, &error);
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (!error.empty()) {
            writeLine("info string perft failed: " + protocolSafe(error));
            return;
        }
        writeLine("info string perft " + std::to_string(depth) + ": " +
                  std::to_string(nodes) + " nodes, " +
                  std::to_string(elapsedMs) + " ms, " +
                  std::to_string(nodesPerSecond(nodes, elapsedMs)) + " nps");
    }

    void handleEval(bool detailed) {
        std::string error;
        const EvalResult result = engine_.debugEvaluate(&error);
        if (!error.empty()) {
            writeLine("info string eval failed: " + protocolSafe(error));
            return;
        }

        if (!detailed) {
            std::string line = "info string eval: classical " +
                formatCentipawns(result.classical);
            if (result.nnueUsed) {
                line += ", nnue raw " + formatCentipawns(result.networkRaw) +
                        ", applied " + formatCentipawns(result.networkApplied);
            } else if (result.nnueRequested && !result.nnueAvailable) {
                line += " (NNUE requested but unavailable; classical fallback)";
            }
            line += ", final " + formatCentipawns(result.finalScore) +
                    " (side to move)";
            writeLine(line);
            return;
        }

        ClassicalBreakdown breakdown;
        engine_.debugClassicalBreakdown(breakdown, &error);
        if (!error.empty()) {
            writeLine("info string evaldetail failed: " + protocolSafe(error));
            return;
        }

        const bool blackToMove =
            engine_.position().sideToMove() == Color::Black;
        const auto stm = [&](Score score) noexcept {
            const Value tapered = taper(score, breakdown.phase);
            return blackToMove ? static_cast<Value>(-tapered) : tapered;
        };

        const Value materialCp = stm(breakdown.material);
        const Value psqtCp = stm(breakdown.psqt);
        const Value pawnsCp = stm(breakdown.pawns);
        const Value mobilityCp = stm(breakdown.mobility);
        const Value kingSafetyCp = stm(breakdown.kingSafety);
        const Value threatsCp = stm(breakdown.threats);
        const Value passedPawnsCp = stm(breakdown.passedPawns);
        const Value spaceCp = stm(breakdown.space);
        const Value miscellaneousCp = stm(breakdown.miscellaneous);
        // ClassicalEvaluator::evaluate() adds a small fixed side-to-move
        // bonus directly in its return statement, outside every
        // ClassicalBreakdown category. Reconciling here (rather than
        // silently dropping it) keeps this printout an honest accounting
        // that actually sums to "Classical STM", not just the categorized
        // part of it.
        const Value categorizedCp = static_cast<Value>(
            materialCp + psqtCp + pawnsCp + mobilityCp + kingSafetyCp +
            threatsCp + passedPawnsCp + spaceCp + miscellaneousCp);
        const Value uncategorizedCp = static_cast<Value>(
            result.classical - categorizedCp);

        writeLine("info string Classical STM: " +
                  formatCentipawns(result.classical));
        writeLine("info string Material: " + formatCentipawns(materialCp));
        writeLine("info string PSQT: " + formatCentipawns(psqtCp));
        writeLine("info string Pawns: " + formatCentipawns(pawnsCp));
        writeLine("info string Mobility: " + formatCentipawns(mobilityCp));
        writeLine("info string King safety: " + formatCentipawns(kingSafetyCp));
        writeLine("info string Threats: " + formatCentipawns(threatsCp));
        writeLine("info string Passed pawns: " + formatCentipawns(passedPawnsCp));
        writeLine("info string Space: " + formatCentipawns(spaceCp));
        writeLine("info string Miscellaneous: " +
                  formatCentipawns(miscellaneousCp));
        if (uncategorizedCp != 0)
            writeLine("info string Tempo/unmodeled (not in any category above): " +
                      formatCentipawns(uncategorizedCp));

        if (result.nnueUsed) {
            writeLine("info string NNUE mode: " +
                      std::string(toString(engine_.options().nnueOutputMode)));
            writeLine("info string NNUE raw STM: " +
                      formatCentipawns(result.networkRaw));
            if (engine_.options().nnueOutputMode == NNUEOutputMode::Residual) {
                writeLine("info string Residual scale: " +
                          std::to_string(engine_.options().residualScale) + "%");
                writeLine("info string Residual guard factor: " +
                          std::to_string(result.guardFactorPercent) + "%");
                writeLine("info string Residual applied: " +
                          formatCentipawns(result.networkApplied));
            } else {
                writeLine("info string Absolute blend: " +
                          std::to_string(engine_.options().absoluteBlend) + "%");
            }
        } else if (result.nnueRequested && !result.nnueAvailable) {
            writeLine("info string NNUE requested but unavailable; "
                      "classical fallback");
        } else {
            writeLine("info string NNUE mode: disabled (UseNNUE=false)");
        }

        writeLine("info string Final STM: " + formatCentipawns(result.finalScore));
    }

    void handleNnueCheck() {
        const bool ready = engine_.nnueLoaded();
        writeLine("info string nnuecheck: " +
                  std::string(ready ? "network loaded" : "no network loaded") +
                  "; " + protocolSafe(engine_.nnueStatusMessage()));
        if (!ready) return;
        writeLine("info string nnuecheck path: " +
                  protocolSafe(engine_.evaluator().network().loadedPath()));
        writeLine("info string nnuecheck sha256: " +
                  engine_.evaluator().network().payloadSha256());
        writeLine("info string nnuecheck generation: " +
                  std::to_string(engine_.evaluator().network().generation()));
    }

    void handleNnueVerify() {
        std::string error;
        const NNUE::AccumulatorCheck check =
            engine_.debugVerifyAccumulator(&error);
        if (!error.empty()) {
            writeLine("info string nnueverify failed: " + protocolSafe(error));
            return;
        }
        if (!check.available) {
            writeLine("info string nnueverify: no accumulator data available");
            return;
        }

        writeLine(std::string("info string nnueverify: ") +
                  (check.exact ? "incremental matches scratch exactly"
                               : "MISMATCH between incremental and scratch"));
        writeLine("info string nnueverify white: incremental " +
                  std::to_string(check.incrementalWhite) + " cp, scratch " +
                  std::to_string(check.scratchWhite) + " cp");
        writeLine("info string nnueverify side-to-move: incremental " +
                  std::to_string(check.incrementalSideToMove) + " cp, scratch " +
                  std::to_string(check.scratchSideToMove) + " cp");
        if (!check.exact)
            writeLine("info string nnueverify mismatch at perspective " +
                      std::to_string(check.mismatchPerspective) + ", neuron " +
                      std::to_string(check.mismatchNeuron));
    }

    void handleSee(const std::vector<std::string>& words) {
        if (words.size() < 2) {
            writeLine("info string see requires a UCI move argument");
            return;
        }
        const Position& position = engine_.position();
        const Move move = moveFromUci(position, words[1]);
        if (move.isNone()) {
            writeLine("info string see: '" + protocolSafe(words[1]) +
                      "' is not a legal move in the current position");
            return;
        }
        writeLine("info string see " + moveToUci(move) + ": " +
                  formatCentipawns(See::see(position, move)) +
                  " (seeGe 0 = " +
                  (See::seeGe(position, move, 0) ? "true" : "false") + ")");
    }

    void handleKey() {
        const Position& position = engine_.position();
        writeLine("info string key position=" + toHex(position.key()) +
                  " pawn=" + toHex(position.pawnKey()) +
                  " material=" + toHex(position.materialKey()));
    }

    void handleCheckBoard() {
        const Position& position = engine_.position();
        if (position.isConsistent()) {
            writeLine("info string checkboard: consistent");
        } else {
            writeLine("info string checkboard: INCONSISTENT: " +
                      protocolSafe(position.consistencyError()));
        }
    }

    void handleSetOption(const std::vector<std::string>& words) {
        stopAndJoin();
        if (words.size() < 4 || lowerAscii(words[1]) != "name") {
            writeLine("info string malformed setoption command");
            return;
        }

        std::size_t valueIndex = words.size();
        for (std::size_t index = 2; index < words.size(); ++index) {
            if (lowerAscii(words[index]) == "value") {
                valueIndex = index;
                break;
            }
        }
        if (valueIndex == words.size() || valueIndex == 2 ||
            valueIndex + 1 >= words.size()) {
            writeLine("info string malformed setoption name/value");
            return;
        }

        const std::string name = joinWords(words, 2, valueIndex);
        const std::string value = stripOptionalQuotes(
            joinWords(words, valueIndex + 1, words.size()));
        const bool oldResidualMode =
            engine_.options().nnueOutputMode == NNUEOutputMode::Residual;

        std::string error;
        const bool optionAccepted = engine_.setOption(name, value, &error);
        if (!optionAccepted)
            writeLine("info string setoption failed: " + protocolSafe(error));

        if (optionAccepted && lowerAscii(name) == "nnueblend" && oldResidualMode)
            writeLine("info string NNUEBlend controls AbsoluteBlend only; "
                      "ResidualScale is unchanged in residual mode");

        reportNNUEFallbackIfNeeded();
    }

    void handlePosition(const std::vector<std::string>& words) {
        stopAndJoin();
        if (words.size() < 2) {
            writeLine("info string malformed position command");
            return;
        }

        std::string fen;
        std::size_t index = 0;
        const std::string kind = lowerAscii(words[1]);
        if (kind == "startpos") {
            fen = std::string(kStartFen);
            index = 2;
        } else if (kind == "fen") {
            if (words.size() < 8) {
                writeLine("info string position fen requires six FEN fields");
                return;
            }
            fen = joinWords(words, 2, 8);
            index = 8;
        } else {
            writeLine("info string position requires startpos or fen");
            return;
        }

        if (index < words.size() && lowerAscii(words[index]) != "moves") {
            writeLine("info string unexpected token after position: " +
                      protocolSafe(words[index]));
            return;
        }

        std::vector<std::string_view> moves;
        if (index < words.size()) {
            ++index;
            moves.reserve(words.size() - index);
            for (; index < words.size(); ++index) moves.emplace_back(words[index]);
        }

        std::string error;
        if (!engine_.setPosition(fen, &error)) {
            writeLine("info string position rejected: " + protocolSafe(error));
            return;
        }
        if (!moves.empty() && !engine_.applyMoves(moves, &error))
            writeLine("info string position moves rejected: " +
                      protocolSafe(error));
    }

    [[nodiscard]] bool parseGo(const std::vector<std::string>& words,
                               SearchLimits& limits,
                               std::string& error) const {
        bool hasClock = false;
        bool hasFiniteLimit = false;

        for (std::size_t index = 1; index < words.size(); ++index) {
            const std::string token = lowerAscii(words[index]);
            if (token == "infinite") {
                limits.infinite = true;
                continue;
            }
            if (index + 1 >= words.size()) {
                error = "go token requires a value: " + words[index];
                return false;
            }

            const std::string_view value = words[++index];
            std::int64_t signedValue = 0;
            if (token == "nodes") {
                std::uint64_t nodeValue = 0;
                if (!parseInteger(value, nodeValue) || nodeValue == 0) {
                    error = "nodes must be a positive integer";
                    return false;
                }
                limits.nodes = nodeValue;
                hasFiniteLimit = true;
                continue;
            }
            if (!parseInteger(value, signedValue) || signedValue < 0) {
                error = token + " must be a non-negative integer";
                return false;
            }

            if (token == "wtime") {
                limits.whiteTimeMs = signedValue;
                hasClock = true;
            } else if (token == "btime") {
                limits.blackTimeMs = signedValue;
                hasClock = true;
            } else if (token == "winc") {
                limits.whiteIncrementMs = signedValue;
            } else if (token == "binc") {
                limits.blackIncrementMs = signedValue;
            } else if (token == "movestogo") {
                if (signedValue == 0 || signedValue >
                    std::numeric_limits<int>::max()) {
                    error = "movestogo must fit a positive int";
                    return false;
                }
                limits.movesToGo = static_cast<int>(signedValue);
            } else if (token == "movetime") {
                limits.moveTimeMs = signedValue;
                hasFiniteLimit = true;
            } else if (token == "depth") {
                if (signedValue == 0 ||
                    signedValue > MAX_SUPPORTED_SEARCH_DEPTH) {
                    error = "depth must be from 1 to " +
                            std::to_string(MAX_SUPPORTED_SEARCH_DEPTH);
                    return false;
                }
                limits.depth = static_cast<int>(signedValue);
                hasFiniteLimit = true;
            } else {
                error = "unsupported go token: " + token;
                return false;
            }
        }

        if (!limits.infinite && !hasClock && !hasFiniteLimit)
            limits.infinite = true;
        return true;
    }

    bool handle(std::string_view line) {
        const std::vector<std::string> words = splitWords(line);
        if (words.empty()) return false;
        const std::string command = lowerAscii(words.front());

        if (command == "uci") {
            printIdentityAndOptions();
        } else if (command == "isready") {
            reportNNUEFallbackIfNeeded();
            writeLine("readyok");
        } else if (command == "setoption") {
            handleSetOption(words);
        } else if (command == "ucinewgame") {
            stopAndJoin();
            std::string error;
            if (!engine_.newGame(&error))
                writeLine("info string ucinewgame failed: " +
                          protocolSafe(error));
        } else if (command == "position") {
            handlePosition(words);
        } else if (command == "go") {
            SearchLimits limits;
            std::string error;
            if (!parseGo(words, limits, error)) {
                writeLine("info string malformed go: " + protocolSafe(error));
            } else {
                startSearch(limits);
            }
        } else if (command == "stop") {
            stopAndJoin();
        } else if (command == "quit") {
            stopAndJoin();
            return true;
        } else if (command == "perft") {
            handlePerft(words);
        } else if (command == "eval") {
            handleEval(false);
        } else if (command == "evaldetail") {
            handleEval(true);
        } else if (command == "nnuecheck") {
            handleNnueCheck();
        } else if (command == "nnueverify") {
            handleNnueVerify();
        } else if (command == "see") {
            handleSee(words);
        } else if (command == "key") {
            handleKey();
        } else if (command == "checkboard") {
            handleCheckBoard();
        } else {
            writeLine("info string unknown command: " +
                      protocolSafe(words.front()));
        }
        return false;
    }

    std::istream& input_;
    std::ostream& output_;
    Engine engine_{};

    std::mutex outputMutex_{};
    std::thread searchThread_{};
    std::mutex searchStateMutex_{};
    std::condition_variable searchStateChanged_{};
    bool searchFinished_ = true;
    std::string reportedNNUEFailure_{};
};

} // namespace

int run(std::istream& input, std::ostream& output) {
    Session session(input, output);
    return session.run();
}

} // namespace Eunshin::UCI
