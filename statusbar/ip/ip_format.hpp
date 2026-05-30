#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// IP module header including all format_to() overloads.
///
/// Include this header instead of ip.hpp when the translation unit
/// needs to call format_to on IP types. Headers that only need the
/// data structures should include ip.hpp (or the specific sub-header)
/// to avoid the compile-time cost of <format>.

#include "statusbar/ip/ip.hpp"
#include "statusbar/ip/ip_arp_format.hpp"
#include "statusbar/ip/ip_icmp_format.hpp"
#include "statusbar/ip/ip_icmpv6_format.hpp"
#include "statusbar/ip/ip_igmp_format.hpp"
#include "statusbar/ip/ip_ipv4_address_format.hpp"
#include "statusbar/ip/ip_ipv4_format.hpp"
#include "statusbar/ip/ip_ipv6_format.hpp"
#include "statusbar/ip/ip_udp_format.hpp"
