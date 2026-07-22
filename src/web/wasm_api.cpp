#include "core/move.h"
#include "core/version.h"
#include "engine/engine.h"
#include "search/search.h"
#include "search/time_manager.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define EUNSHIN_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EUNSHIN_EXPORT
#endif

namespace {
Eunshin::Engine engine;
std::string response;
bool initialized = false;

std::string escapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 16);
    for (const unsigned char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[(c >> 4) & 0x0f];
                    out += hex[c & 0x0f];
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

const char* errorJson(const std::string& message) {
    response = "{\"ok\":false,\"error\":\"" + escapeJson(message) + "\"}";
    return response.c_str();
}

std::string pvJson(const Eunshin::PVLine& pv) {
    std::ostringstream out;
    out << '[';
    const int length = std::max(0, std::min(pv.length, Eunshin::MAX_PLY));
    for (int i = 0; i < length; ++i) {
        const Eunshin::Move move = pv.moves[static_cast<std::size_t>(i)];
        if (move.isNone()) break;
        if (i > 0) out << ',';
        out << '"' << Eunshin::moveToUci(move) << '"';
    }
    out << ']';
    return out.str();
}

bool ensureInitialized(std::string& error) {
    if (initialized) return true;
    if (!engine.initialize(&error)) return false;
    // Browser memory is finite. 32 MB is enough for casual play and keeps the
    // total WASM footprint reasonable beside the ~12 MB NNUE network.
    if (!engine.setOption("Hash", "32", &error)) return false;
    // The file is preloaded by Emscripten at this exact virtual path.
    if (!engine.setOption("EvalFile", "/firstnet_v5_10b.snnue", &error)) return false;
    engine.setOption("UseNNUE", "false", nullptr);
    initialized = true;
    return true;
}
}

extern "C" {

EUNSHIN_EXPORT const char* eunshin_init() {
    std::string error;
    if (!ensureInitialized(error)) return errorJson(error);
    std::ostringstream out;
    out << "{\"ok\":true,\"name\":\"" << escapeJson(Eunshin::Version::idString())
        << "\",\"nnueLoaded\":" << (engine.nnueLoaded() ? "true" : "false")
        << ",\"nnueStatus\":\"" << escapeJson(engine.nnueStatusMessage()) << "\"}";
    response = out.str();
    return response.c_str();
}

EUNSHIN_EXPORT const char* eunshin_search(const char* fen, int depth, int moveTimeMs) {
    std::string error;
    if (!ensureInitialized(error)) return errorJson(error);
    if (!fen || !*fen) return errorJson("FEN is empty");
    if (!engine.setPosition(fen, &error)) return errorJson(error);

    Eunshin::SearchLimits limits;
    limits.depth = std::clamp(depth, 1, 64);
    if (moveTimeMs >= 0) limits.moveTimeMs = moveTimeMs;

    const Eunshin::SearchResult result = engine.go(limits, nullptr, nullptr, &error);
    if (!error.empty()) return errorJson(error);

    std::ostringstream out;
    out << "{\"ok\":true"
        << ",\"bestmove\":\"" << Eunshin::moveToUci(result.bestMove) << "\""
        << ",\"ponder\":\"" << Eunshin::moveToUci(result.ponder) << "\""
        << ",\"score\":" << result.score
        << ",\"depth\":" << result.completedDepth
        << ",\"seldepth\":" << result.selDepth
        << ",\"nodes\":" << result.nodes
        << ",\"timeMs\":" << result.timeMs
        << ",\"pv\":" << pvJson(result.pv)
        << '}';
    response = out.str();
    return response.c_str();
}

EUNSHIN_EXPORT const char* eunshin_evaluate(const char* fen) {
    std::string error;
    if (!ensureInitialized(error)) return errorJson(error);
    if (!fen || !*fen) return errorJson("FEN is empty");
    if (!engine.setPosition(fen, &error)) return errorJson(error);
    const Eunshin::EvalResult result = engine.debugEvaluate(&error);
    if (!error.empty()) return errorJson(error);

    std::ostringstream out;
    out << "{\"ok\":true,\"score\":" << result.finalScore
        << ",\"classical\":" << result.classical
        << ",\"nnueRaw\":" << result.networkRaw
        << ",\"nnueUsed\":" << (result.nnueUsed ? "true" : "false") << '}';
    response = out.str();
    return response.c_str();
}

EUNSHIN_EXPORT void eunshin_stop() {
    engine.stop();
}

EUNSHIN_EXPORT const char* eunshin_version() {
    response = Eunshin::Version::idString();
    return response.c_str();
}

}

