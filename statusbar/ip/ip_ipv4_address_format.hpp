#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// format_to() overloads for IP IPv4Address. Split from ip_ipv4_address.hpp
/// so consumers that only need the data structures do not pay the
/// compile-time cost of <format>.

#include "statusbar/ip/ip_ipv4_address.hpp"

#include <format>

namespace statusbar::ip {

/// Format an IPv4Address to an output iterator
/// @param out Output iterator to write formatted text to
/// @param addr IPv4 address to format
template <typename OutputIt>
auto format_to(OutputIt out, IPv4Address const& addr) -> OutputIt
{
    return std::format_to(out, "{}.{}.{}.{}", addr.octet(0), addr.octet(1), addr.octet(2), addr.octet(3));
}

}  // namespace statusbar::ip
