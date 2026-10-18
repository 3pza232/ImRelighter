#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace core {

// Parses '#rrggbb' into linear component values in [0, 1]. Returns nullopt for
// malformed input so callers can choose their own fallback color.
inline std::optional<std::array<float, 3>> parseHexColor(std::string_view hex) {
    if (hex.size() != 7 || hex[0] != '#') return std::nullopt;

    const auto nibble = [](char digit) -> int {
        if (digit >= '0' && digit <= '9') return digit - '0';
        if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
        if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
        return -1;
    };

    std::array<float, 3> color{};
    for (std::size_t i = 0; i < color.size(); ++i) {
        const int high = nibble(hex[1 + i * 2]);
        const int low = nibble(hex[2 + i * 2]);
        if (high < 0 || low < 0) return std::nullopt;
        color[i] = static_cast<float>(high * 16 + low) / 255.0f;
    }
    return color;
}

} // namespace core
