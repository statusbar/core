// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/tsn/tsn_stream_id.hpp"

#include "statusbar/tsn/tsn_stream_id_format.hpp"

#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace statusbar::tsn {

auto to_string(StreamId const& stream_id) -> std::string
{
    std::string result;
    result.reserve(22);  // "xx:xx:xx:xx:xx:xx:xxxx"
    format_to(std::back_inserter(result), stream_id);
    return result;
}

auto from_string(std::string_view str, StreamId* /*tag*/) noexcept -> std::optional<StreamId>
{
    str = ieee::trim_whitespace(str);

    // Find the separator between EUI-48 and unique_id
    // Could be : or - or . or /
    // EUI-48 is 17 chars (with separators) or 12 chars (without)

    std::string_view eui48_part;
    std::string_view unique_id_part;

    // Try to find various separators after the EUI-48 portion
    size_t const sep_pos = 17;  // After "aa:bb:cc:dd:ee:ff"
    (void)sep_pos;

    if ((str.size() > 17 && (str[17] == ':' || str[17] == '-' || str[17] == '.' || str[17] == '/')) || str.size() >= 22) {
        // Format with separator after "aa:bb:cc:dd:ee:ff" or full 22+ char format
        eui48_part = str.substr(0, 17);
        unique_id_part = str.substr(18);
    } else if (str.size() > 12 && str.size() <= 17) {
        // Might be "aabbccddeeff:1234" format
        size_t const pos = str.find_first_of(":-./ ");
        if (pos == 12) {
            eui48_part = str.substr(0, 12);
            unique_id_part = str.substr(13);
        } else {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }

    // Parse EUI-48
    auto eui48_opt = ieee::eui48_from_string(eui48_part);
    if (!eui48_opt) {
        return std::nullopt;
    }

    // Parse unique_id (hex, 1-4 digits)
    if (unique_id_part.empty() || unique_id_part.size() > 4) {
        return std::nullopt;
    }

    uint16_t unique_id = 0;
    for (char const c : unique_id_part) {
        auto const digit = ieee::parse_hex_digit(c);
        if (digit == 255) {
            return std::nullopt;
        }
        unique_id = static_cast<uint16_t>((unique_id << 4) | digit);
    }

    return StreamId{*eui48_opt, unique_id};
}

auto stream_id_from_string(std::string_view str) noexcept -> std::optional<StreamId>
{
    return from_string(str, static_cast<StreamId*>(nullptr));
}

}  // namespace statusbar::tsn
