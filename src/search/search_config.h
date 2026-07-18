#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Eunshin {

enum class NNUEOutputMode : std::uint8_t {
    Residual,
    Absolute
};

// A value snapshot copied into a SearchWorker once, before a search starts.
// The Worker exposes it only as const state, so later frontend option changes
// cannot race with or alter an active search.
struct SearchConfig final {
    std::size_t hashMegabytes = 256;
    int moveOverheadMs = 30;

    bool useNNUE = true;
    NNUEOutputMode nnueOutputMode = NNUEOutputMode::Residual;
    int residualScale = 100;
    bool residualGuard = true;
    int absoluteBlend = 35;

    bool useIIR = false;
    bool useSEEPruning = false;
    bool useAEGIS = false;
    bool useLIMBO = false;

    std::uint64_t revision = 0;
};

[[nodiscard]] constexpr std::string_view toString(NNUEOutputMode mode) noexcept {
    return mode == NNUEOutputMode::Residual ? "Residual" : "Absolute";
}

} // namespace Eunshin
