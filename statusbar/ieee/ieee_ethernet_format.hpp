#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// format_to() overloads for IEEE Ethernet types. Split from
/// ieee_ethernet.hpp so consumers that only need the structs do not pay
/// the compile-time cost of <format>.

#include "statusbar/ieee/ieee_ethernet.hpp"

#include <format>
#include <tuple>

namespace statusbar::ieee {

/// Format an Eui48 to an output iterator
template <typename OutputIt>
inline auto format_to(OutputIt out, Eui48 const& mac) -> OutputIt
{
    return std::format_to(
        out,
        "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
        std::get<0>(mac.value),
        std::get<1>(mac.value),
        std::get<2>(mac.value),
        std::get<3>(mac.value),
        std::get<4>(mac.value),
        std::get<5>(mac.value));
}

/// Format an Eui64 to an output iterator
template <typename OutputIt>
inline auto format_to(OutputIt out, Eui64 const& mac) -> OutputIt
{
    auto const s = mac.span();
    return std::format_to(
        out, "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]);
}

/// Format an EthernetFrame to an output iterator
template <typename OutputIt>
inline auto format_to(OutputIt out, EthernetFrame const& eth) -> OutputIt
{
    out = std::format_to(out, "Ethernet: ");
    out = format_to(out, eth.src_mac);
    out = std::format_to(out, " -> ");
    out = format_to(out, eth.dest_mac);
    if (eth.vlan_tag.is_set()) {
        out = std::format_to(out, " vlan={} pcp={}", eth.vlan_tag.get_vid(), eth.vlan_tag.get_pcp());
    }
    return std::format_to(out, " ethertype={:#06x}", eth.ethertype.get());
}

}  // namespace statusbar::ieee
