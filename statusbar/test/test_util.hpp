// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>

/// Prevent the compiler from optimizing away a value.
/// Use in tests to ensure computed results are not dead-code eliminated.
template <typename T>
inline void do_not_optimize(T const& value)
{
    asm volatile("" : : "g"(value) : "memory");
}

/// Convert a hex string to a fixed-size byte array.
/// The hex string must contain exactly 2*N hex characters (no separators).
template <std::size_t N>
auto hex_to_bytes(char const* hex) -> std::array<uint8_t, N>
{
    std::array<uint8_t, N> out{};
    for (std::size_t i = 0; i < N; ++i) {
        auto nibble = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') {
                return static_cast<uint8_t>(c - '0');
            }
            if (c >= 'a' && c <= 'f') {
                return static_cast<uint8_t>(c - 'a' + 10);
            }
            if (c >= 'A' && c <= 'F') {
                return static_cast<uint8_t>(c - 'A' + 10);
            }
            return 0;
        };
        out[i] = static_cast<uint8_t>((nibble(hex[2 * i]) << 4) | nibble(hex[2 * i + 1]));
    }
    return out;
}
