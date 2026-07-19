#pragma once

#include <string>
#include <string_view>

namespace Eunshin {
namespace Version {

inline constexpr int Major = 5;
inline constexpr int Minor = 0;
inline constexpr int Patch = 0;

inline constexpr std::string_view Suffix = "rc.1";
inline constexpr std::string_view Architecture = "Q";

inline constexpr std::string_view EngineName = "EunshinBishop";
inline constexpr std::string_view Author = "Blucy1004";

// "5.0.0" or "5.0.0-rc.1" if Suffix is non-empty.
[[nodiscard]] inline std::string number() {
    std::string result = std::to_string(Major) + "." + std::to_string(Minor) +
                         "." + std::to_string(Patch);
    if (!Suffix.empty()) {
        result += "-";
        result += Suffix;
    }
    return result;
}

// "EunshinBishop 5.0.0-rc.1 Q" -- the single UCI id name / --version /
// console banner string.  There must be exactly one place that formats
// this so those three surfaces can never drift apart.
[[nodiscard]] inline std::string idString() {
    std::string result{EngineName};
    result += " ";
    result += number();
    result += " ";
    result += Architecture;
    return result;
}

} // namespace Version
} // namespace Eunshin
