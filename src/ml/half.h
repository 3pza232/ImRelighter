#pragma once

#include <cstdint>
#include <cstring>

namespace ml {

// IEEE 754 binary16 conversion with round-to-nearest-even. The previous
// conversion shifted a 32-bit mantissa by up to 37 bits (undefined behavior)
// and truncated instead of rounding.
inline std::uint16_t floatToHalf(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));

    const std::uint32_t sign = (bits >> 16) & 0x8000u;
    const std::uint32_t magnitude = bits & 0x7fffffffu;

    if (magnitude > 0x7f800000u) {
        return static_cast<std::uint16_t>(
            sign | 0x7c00u | ((magnitude & 0x007fffffu) ? 0x0200u : 0u));
    }
    if (magnitude >= 0x47800000u) {
        return static_cast<std::uint16_t>(sign | 0x7c00u);
    }

    if (magnitude < 0x38800000u) {
        const int exponent =
            static_cast<int>((magnitude >> 23) & 0xffu) - 127;
        const int shift = -exponent - 1;
        if (shift > 24) return static_cast<std::uint16_t>(sign);

        std::uint32_t mantissa = (magnitude & 0x007fffffu) | 0x00800000u;
        std::uint32_t result = mantissa >> shift;
        const std::uint32_t remainder =
            mantissa & ((1u << shift) - 1u);
        const std::uint32_t halfway = 1u << (shift - 1);
        if (remainder > halfway ||
            (remainder == halfway && (result & 1u) != 0u)) {
            ++result;
        }
        return static_cast<std::uint16_t>(sign | result);
    }

    std::uint32_t result = ((magnitude >> 23) - 112u) << 10;
    result |= (magnitude & 0x007fffffu) >> 13;
    const std::uint32_t remainder = magnitude & 0x1fffu;
    if (remainder > 0x1000u ||
        (remainder == 0x1000u && (result & 1u) != 0u)) {
        ++result;
    }
    return static_cast<std::uint16_t>(sign | result);
}

inline float halfToFloat(std::uint16_t value) {
    const std::uint32_t sign = static_cast<std::uint32_t>(value & 0x8000u) << 16;
    const std::uint32_t exponent = (value >> 10) & 0x1fu;
    std::uint32_t mantissa = value & 0x3ffu;
    std::uint32_t bits = 0;

    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {
            int shift = 0;
            while ((mantissa & 0x400u) == 0) {
                mantissa <<= 1;
                ++shift;
            }
            mantissa &= 0x3ffu;
            bits = sign | (static_cast<std::uint32_t>(127 - 14 - shift) << 23) |
                   (mantissa << 13);
        }
    } else if (exponent == 31) {
        bits = sign | 0x7f800000u | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
    }

    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

} // namespace ml
