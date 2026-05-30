#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// format_to() overloads for IP UdpHeader. Split from ip_udp.hpp so
/// consumers that only need the data structures do not pay the
/// compile-time cost of <format>.

#include "statusbar/ip/ip_udp.hpp"

#include <format>

namespace statusbar::ip {

/// Format a UdpHeader to an output iterator
/// @param out Output iterator to write formatted text to
/// @param udp UDP header to format
template <typename OutputIt>
auto format_to(OutputIt out, UdpHeader const& udp) -> OutputIt
{
    return std::format_to(out, "UDP: port {} -> {} len={}", udp.src_port.get(), udp.dst_port.get(), udp.length.get());
}

}  // namespace statusbar::ip
