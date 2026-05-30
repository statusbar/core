// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/tsn/tsn_clock_identity.hpp"

#include "statusbar/tsn/tsn_clock_identity_format.hpp"

#include <array>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace statusbar::tsn {

auto to_string(ClockIdentity const& id) -> std::string
{
    std::string result;
    result.reserve(24);  // "xx:xx:xx:xx:xx:xx:xx:xx"
    format_to(std::back_inserter(result), id);
    return result;
}

auto from_string(std::string_view str, ClockIdentity* /*tag*/) -> std::optional<ClockIdentity>
{
    // Skip leading/trailing whitespace
    str = ieee::trim_whitespace(str);

    // Expected format with separators: "aa:bb:cc:dd:ee:ff:gg:hh" (23 chars)
    // Or without separators: "aabbccddeeffgghh" (16 chars)
    if (str.size() != 23 && str.size() != 16) {
        return std::nullopt;
    }

    std::array<std::uint8_t, 8> bytes{};

    if (str.size() == 16) {
        // No separators
        for (std::size_t i = 0; i < 8; ++i) {
            auto const hi = ieee::parse_hex_digit(str[(i * 2)]);
            auto const lo = ieee::parse_hex_digit(str[(i * 2) + 1]);
            if (hi == 255 || lo == 255) {
                return std::nullopt;
            }
            bytes[i] = static_cast<std::uint8_t>((hi << 4) | lo);
        }
    } else {
        // With separators (: or -)
        char const sep = str[2];
        if (sep != ':' && sep != '-') {
            return std::nullopt;
        }

        for (std::size_t i = 0; i < 8; ++i) {
            std::size_t const pos = i * 3;
            if (i > 0 && str[pos - 1] != sep) {
                return std::nullopt;
            }

            auto const hi = ieee::parse_hex_digit(str[pos]);
            auto const lo = ieee::parse_hex_digit(str[pos + 1]);
            if (hi == 255 || lo == 255) {
                return std::nullopt;
            }
            bytes[i] = static_cast<std::uint8_t>((hi << 4) | lo);
        }
    }

    return ClockIdentity{bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]};
}

auto clock_identity_from_string(std::string_view str) -> std::optional<ClockIdentity>
{
    return from_string(str, static_cast<ClockIdentity*>(nullptr));
}

}  // namespace statusbar::tsn
