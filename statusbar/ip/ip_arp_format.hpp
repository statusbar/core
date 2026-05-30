#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// format_to() overloads for IP ArpHeader. Split from ip_arp.hpp so
/// consumers that only need the data structures do not pay the
/// compile-time cost of <format>.

#include "statusbar/ieee/ieee_ethernet_format.hpp"
#include "statusbar/ip/ip_arp.hpp"
#include "statusbar/ip/ip_ipv4_address_format.hpp"

#include <format>

namespace statusbar::ip {

/// Format an ArpHeader to an output iterator
/// @param out Output iterator to write formatted text to
/// @param arp ARP header to format
template <typename OutputIt>
auto format_to(OutputIt out, ArpHeader const& arp) -> OutputIt
{
    out = std::format_to(out, "ARP: {} ", arp_op_name(arp.operation));
    out = format_to(out, arp.sender_hardware_addr);
    out = std::format_to(out, " (");
    out = format_to(out, arp.sender_protocol_addr);
    out = std::format_to(out, ") -> ");
    out = format_to(out, arp.target_hardware_addr);
    out = std::format_to(out, " (");
    out = format_to(out, arp.target_protocol_addr);
    out = std::format_to(out, ")");
    return out;
}

}  // namespace statusbar::ip
