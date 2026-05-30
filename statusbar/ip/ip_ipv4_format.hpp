#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// format_to() overloads for IP IPv4Header. Split from ip_ipv4.hpp so
/// consumers that only need the data structures do not pay the
/// compile-time cost of <format>.

#include "statusbar/ip/ip_ipv4.hpp"
#include "statusbar/ip/ip_ipv4_address_format.hpp"

#include <format>

namespace statusbar::ip {

/// Format an IPv4Header to an output iterator
/// @param out Output iterator to write formatted text to
/// @param ip IPv4 header to format
template <typename OutputIt>
auto format_to(OutputIt out, IPv4Header const& ip) -> OutputIt
{
    out = std::format_to(out, "IPv4: ");
    out = format_to(out, ip.src_addr);
    out = std::format_to(out, " -> ");
    out = format_to(out, ip.dst_addr);
    return std::format_to(out, " proto={} ttl={} len={}", ip.protocol.get(), ip.ttl.get(), ip.total_length.get());
}

}  // namespace statusbar::ip
