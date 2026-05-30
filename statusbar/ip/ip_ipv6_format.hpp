#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// format_to() overloads for IP IPv6Address and IPv6Header. Split from
/// ip_ipv6.hpp so consumers that only need the data structures do not
/// pay the compile-time cost of <format>.

#include "statusbar/ip/ip_ipv6.hpp"

#include <cstddef>
#include <format>

namespace statusbar::ip {

/// Format an IPv6Address to an output iterator
/// @param out Output iterator to write formatted text to
/// @param addr IPv6 address to format
template <typename OutputIt>
inline auto format_to(OutputIt out, IPv6Address const& addr) -> OutputIt
{
    // Print in standard colon-hex format
    for (size_t i = 0; i < 8; ++i) {
        if (i > 0) {
            out = std::format_to(out, ":");
        }
        out = std::format_to(out, "{:x}", addr.word(i));
    }
    return out;
}

/// Format an IPv6Header to an output iterator
/// @param out Output iterator to write formatted text to
/// @param ip IPv6 header to format
template <typename OutputIt>
inline auto format_to(OutputIt out, IPv6Header const& ip) -> OutputIt
{
    out = std::format_to(out, "IPv6: ");
    out = format_to(out, ip.src_addr);
    out = std::format_to(out, " -> ");
    out = format_to(out, ip.dst_addr);
    return std::format_to(
        out, " next_hdr={} hop_limit={} payload_len={}", ip.next_header.get(), ip.hop_limit.get(), ip.payload_length.get());
}

}  // namespace statusbar::ip
