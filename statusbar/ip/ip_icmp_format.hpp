#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// format_to() overloads for IP IcmpHeader. Split from ip_icmp.hpp so
/// consumers that only need the data structures do not pay the
/// compile-time cost of <format>.

#include "statusbar/ip/ip_icmp.hpp"

#include <cstdint>
#include <format>

namespace statusbar::ip {

/// Format an IcmpHeader to an output iterator
/// @param out Output iterator to write formatted text to
/// @param icmp ICMP header to format
template <typename OutputIt>
auto format_to(OutputIt out, IcmpHeader const& icmp) -> OutputIt
{
    out = std::format_to(out, "ICMP: {} code={}", icmp_type_name(icmp.type), icmp.code.get());
    if (icmp.is_echo_request() || icmp.is_echo_reply()) {
        out = std::format_to(out, " id={} seq={}", icmp.identifier.get(), static_cast<uint16_t>(icmp.sequence));
    }
    return out;
}

}  // namespace statusbar::ip
