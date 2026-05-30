#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// format_to() overloads for IP IgmpHeader. Split from ip_igmp.hpp so
/// consumers that only need the data structures do not pay the
/// compile-time cost of <format>.

#include "statusbar/ip/ip_igmp.hpp"
#include "statusbar/ip/ip_ipv4_address_format.hpp"

#include <format>

namespace statusbar::ip {

/// Format an IgmpHeader to an output iterator
/// @param out Output iterator to write formatted text to
/// @param igmp IGMP header to format
template <typename OutputIt>
auto format_to(OutputIt out, IgmpHeader const& igmp) -> OutputIt
{
    out = std::format_to(out, "IGMP: {} group=", igmp_type_name(igmp.type));
    out = format_to(out, igmp.group_addr);
    if (igmp.is_membership_query()) {
        out = std::format_to(out, " max_resp_time={}", igmp.max_resp_time.get());
    }
    return out;
}

}  // namespace statusbar::ip
