#include "engine/options.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <utility>

namespace Eunshin {
namespace {

bool equalAsciiIgnoreCase(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        const unsigned char left = static_cast<unsigned char>(lhs[index]);
        const unsigned char right = static_cast<unsigned char>(rhs[index]);
        if (std::tolower(left) != std::tolower(right)) return false;
    }
    return true;
}

bool parseBoolean(std::string_view text, bool& value) noexcept {
    if (equalAsciiIgnoreCase(text, "true") || text == "1" ||
        equalAsciiIgnoreCase(text, "on")) {
        value = true;
        return true;
    }
    if (equalAsciiIgnoreCase(text, "false") || text == "0" ||
        equalAsciiIgnoreCase(text, "off")) {
        value = false;
        return true;
    }
    return false;
}

bool parseInteger(std::string_view text, int& value) noexcept {
    if (text.empty()) return false;
    int parsed = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr != last) return false;
    value = parsed;
    return true;
}

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

template <typename T>
bool assignIfChanged(T& destination, const T& source) {
    if (destination == source) return false;
    destination = source;
    return true;
}

} // namespace

SearchConfig EngineOptions::snapshot() const noexcept {
    SearchConfig result;
    result.hashMegabytes = hashMegabytes;
    result.moveOverheadMs = moveOverheadMs;
    result.useNNUE = useNNUE;
    result.nnueOutputMode = nnueOutputMode;
    result.residualScale = residualScale;
    result.residualGuard = residualGuard;
    result.absoluteBlend = absoluteBlend;
    result.useIIR = useIIR;
    result.useSEEPruning = useSEEPruning;
    result.useAEGIS = useAEGIS;
    result.useLIMBO = useLIMBO;
    result.revision = revision;
    return result;
}

bool EngineOptions::set(std::string_view name,
                        std::string_view value,
                        std::string* error) {
    if (error) error->clear();
    bool changed = false;

    if (equalAsciiIgnoreCase(name, "Hash")) {
        int parsed = 0;
        if (!parseInteger(value, parsed) || parsed < 0 ||
            static_cast<std::size_t>(parsed) < MIN_HASH_MB ||
            static_cast<std::size_t>(parsed) > MAX_HASH_MB)
            return fail(error, "Hash must be an integer from 1 to 4096");
        changed = assignIfChanged(hashMegabytes,
                                  static_cast<std::size_t>(parsed));
    } else if (equalAsciiIgnoreCase(name, "MoveOverhead")) {
        int parsed = 0;
        if (!parseInteger(value, parsed) || parsed < 0 ||
            parsed > MAX_MOVE_OVERHEAD_MS)
            return fail(error, "MoveOverhead must be an integer from 0 to 5000");
        changed = assignIfChanged(moveOverheadMs, parsed);
    } else if (equalAsciiIgnoreCase(name, "UseNNUE")) {
        bool parsed = false;
        if (!parseBoolean(value, parsed))
            return fail(error, "UseNNUE requires true or false");
        changed = assignIfChanged(useNNUE, parsed);
    } else if (equalAsciiIgnoreCase(name, "EvalFile")) {
        if (value.empty()) return fail(error, "EvalFile must not be empty");
        changed = assignIfChanged(evalFile, std::string(value));
    } else if (equalAsciiIgnoreCase(name, "NNUEOutputMode")) {
        NNUEOutputMode parsed;
        if (equalAsciiIgnoreCase(value, "Residual"))
            parsed = NNUEOutputMode::Residual;
        else if (equalAsciiIgnoreCase(value, "Absolute"))
            parsed = NNUEOutputMode::Absolute;
        else
            return fail(error, "NNUEOutputMode must be Residual or Absolute");
        changed = assignIfChanged(nnueOutputMode, parsed);
    } else if (equalAsciiIgnoreCase(name, "ResidualScale")) {
        int parsed = 0;
        if (!parseInteger(value, parsed) || parsed < 0 ||
            parsed > MAX_RESIDUAL_SCALE)
            return fail(error, "ResidualScale must be an integer from 0 to 200");
        changed = assignIfChanged(residualScale, parsed);
    } else if (equalAsciiIgnoreCase(name, "ResidualGuard") ||
               equalAsciiIgnoreCase(name, "HybridGuard")) {
        bool parsed = false;
        if (!parseBoolean(value, parsed))
            return fail(error, "ResidualGuard requires true or false");
        changed = assignIfChanged(residualGuard, parsed);
    } else if (equalAsciiIgnoreCase(name, "AbsoluteBlend") ||
               equalAsciiIgnoreCase(name, "NNUEBlend")) {
        int parsed = 0;
        if (!parseInteger(value, parsed) || parsed < 0 || parsed > 100)
            return fail(error, "AbsoluteBlend must be an integer from 0 to 100");
        changed = assignIfChanged(absoluteBlend, parsed);
    } else if (equalAsciiIgnoreCase(name, "IIR")) {
        bool parsed = false;
        if (!parseBoolean(value, parsed))
            return fail(error, "IIR requires true or false");
        changed = assignIfChanged(useIIR, parsed);
    } else if (equalAsciiIgnoreCase(name, "SEEPruning")) {
        bool parsed = false;
        if (!parseBoolean(value, parsed))
            return fail(error, "SEEPruning requires true or false");
        changed = assignIfChanged(useSEEPruning, parsed);
    } else if (equalAsciiIgnoreCase(name, "AEGIS")) {
        bool parsed = false;
        if (!parseBoolean(value, parsed))
            return fail(error, "AEGIS requires true or false");
        changed = assignIfChanged(useAEGIS, parsed);
    } else if (equalAsciiIgnoreCase(name, "LIMBO")) {
        bool parsed = false;
        if (!parseBoolean(value, parsed))
            return fail(error, "LIMBO requires true or false");
        changed = assignIfChanged(useLIMBO, parsed);
    } else {
        return fail(error, "unknown option: " + std::string(name));
    }

    if (changed && revision != std::numeric_limits<std::uint64_t>::max())
        ++revision;
    return true;
}

} // namespace Eunshin
