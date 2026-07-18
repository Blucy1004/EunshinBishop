#pragma once

#include "search/search_config.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Eunshin {

struct EngineOptions final {
    static constexpr std::size_t MIN_HASH_MB = 1;
    static constexpr std::size_t MAX_HASH_MB = 4096;
    static constexpr int MAX_MOVE_OVERHEAD_MS = 5000;
    static constexpr int MAX_RESIDUAL_SCALE = 200;

    std::size_t hashMegabytes = 256;
    int moveOverheadMs = 30;

    bool useNNUE = true;
    std::string evalFile = "firstnet_v5_10b.snnue";
    NNUEOutputMode nnueOutputMode = NNUEOutputMode::Residual;
    int residualScale = 100;
    bool residualGuard = true;
    int absoluteBlend = 35;

    bool useIIR = false;
    bool useSEEPruning = false;
    bool useAEGIS = false;
    bool useLIMBO = false;

    std::uint64_t revision = 0;

    [[nodiscard]] SearchConfig snapshot() const noexcept;

    // Names are ASCII case-insensitive. Values are validated rather than
    // silently accepted; false leaves the entire option object unchanged.
    bool set(std::string_view name,
             std::string_view value,
             std::string* error = nullptr);
};

} // namespace Eunshin
