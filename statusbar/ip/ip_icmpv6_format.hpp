#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/ieee/ieee.hpp"
#include "statusbar/ip/ip_icmpv6.hpp"
#include "statusbar/ip/ip_ipv6_format.hpp"

#include <cstdint>
#include <format>
#include <span>

namespace statusbar::ip::detail {

/// Format a hex dump to an output iterator
template <typename OutputIt>
auto format_hex_dump(OutputIt out, std::span<uint8_t const> const data) -> OutputIt
{
    for (auto octet_value : data) {
        out = std::format_to(out, "{:02x} ", octet_value);
    }
    out = std::format_to(out, "\n");
    return out;
}

}  // namespace statusbar::ip::detail

namespace statusbar::ip {

/// Format an Icmpv6Header to an output iterator
/// @param out Output iterator to write formatted text to
/// @param icmp ICMPv6 header to format
template <typename OutputIt>
auto format_to(OutputIt out, Icmpv6Header const& icmp) -> OutputIt
{
    return std::format_to(out, "ICMPv6: {} code={}", icmpv6_type_name(icmp.type), icmp.code.get());
}

/// Format an Icmpv6EchoHeader to an output iterator
/// @param out Output iterator to write formatted text to
/// @param echo ICMPv6 echo header to format
template <typename OutputIt>
auto format_to(OutputIt out, Icmpv6EchoHeader const& echo) -> OutputIt
{
    return std::format_to(out, "ICMPv6: {} id={} seq={}", icmpv6_type_name(echo.type), echo.identifier.get(), echo.sequence.get());
}

/// Format an Icmpv6NeighborSolicitation to an output iterator
/// @param out Output iterator to write formatted text to
/// @param ns Neighbor Solicitation message to format
template <typename OutputIt>
auto format_to(OutputIt out, Icmpv6NeighborSolicitation const& ns) -> OutputIt
{
    out = std::format_to(out, "ICMPv6: Neighbor Solicitation target=");
    return format_to(out, ns.target);
}

/// Format an Icmpv6NeighborAdvertisement to an output iterator
/// @param out Output iterator to write formatted text to
/// @param na Neighbor Advertisement message to format
template <typename OutputIt>
auto format_to(OutputIt out, Icmpv6NeighborAdvertisement const& na) -> OutputIt
{
    out = std::format_to(
        out,
        "ICMPv6: Neighbor Advertisement R={} S={} O={} target=",
        na.is_router() ? 1 : 0,
        na.is_solicited() ? 1 : 0,
        na.is_override() ? 1 : 0);
    return format_to(out, na.target);
}

/// Format an Icmpv6RouterSolicitation to an output iterator
/// @param out Output iterator to write formatted text to
template <typename OutputIt>
auto format_to(OutputIt out, Icmpv6RouterSolicitation const& /* rs */) -> OutputIt
{
    return std::format_to(out, "ICMPv6: Router Solicitation");
}

/// Format an ICMPv6 payload to an output iterator
template <typename OutputIt>
auto format_icmpv6(OutputIt out, std::span<uint8_t const> const ip_payload) -> OutputIt
{
    if (ip_payload.size() < Icmpv6Header::LENGTH) {
        out = std::format_to(out, "  ICMPv6 header truncated\n");
        return detail::format_hex_dump(out, ip_payload);
    }

    // Peek at the type to determine which struct to use
    uint8_t const type = ip_payload[0];

    switch (type) {
        case ICMPV6_TYPE_ECHO_REQUEST:
        case ICMPV6_TYPE_ECHO_REPLY:
            if (ip_payload.size() >= Icmpv6EchoHeader::LENGTH) {
                Icmpv6EchoHeader echo_hdr;
                (void)load_unchecked(ip_payload, &echo_hdr);
                out = std::format_to(out, "  ");
                out = format_to(out, echo_hdr);
                out = std::format_to(out, "\n");
                auto echo_payload = ip_payload.subspan(Icmpv6EchoHeader::LENGTH);
                if (!echo_payload.empty()) {
                    out = std::format_to(out, "  ICMPv6 echo payload ({} bytes): ", echo_payload.size());
                    out = detail::format_hex_dump(out, echo_payload);
                }
            } else {
                out = std::format_to(out, "  ICMPv6 Echo header truncated\n");
                out = detail::format_hex_dump(out, ip_payload);
            }
            break;

        case ICMPV6_TYPE_NEIGHBOR_SOLICITATION:
            if (ip_payload.size() >= Icmpv6NeighborSolicitation::LENGTH) {
                Icmpv6NeighborSolicitation ns;
                (void)load_unchecked(ip_payload, &ns);
                out = std::format_to(out, "  ");
                out = format_to(out, ns);
                out = std::format_to(out, "\n");
                auto options = ip_payload.subspan(Icmpv6NeighborSolicitation::LENGTH);
                if (!options.empty()) {
                    out = std::format_to(out, "  NDP options ({} bytes): ", options.size());
                    out = detail::format_hex_dump(out, options);
                }
            } else {
                out = std::format_to(out, "  ICMPv6 Neighbor Solicitation truncated\n");
                out = detail::format_hex_dump(out, ip_payload);
            }
            break;

        case ICMPV6_TYPE_NEIGHBOR_ADVERTISEMENT:
            if (ip_payload.size() >= Icmpv6NeighborAdvertisement::LENGTH) {
                Icmpv6NeighborAdvertisement na;
                (void)load_unchecked(ip_payload, &na);
                out = std::format_to(out, "  ");
                out = format_to(out, na);
                out = std::format_to(out, "\n");
                auto options = ip_payload.subspan(Icmpv6NeighborAdvertisement::LENGTH);
                if (!options.empty()) {
                    out = std::format_to(out, "  NDP options ({} bytes): ", options.size());
                    out = detail::format_hex_dump(out, options);
                }
            } else {
                out = std::format_to(out, "  ICMPv6 Neighbor Advertisement truncated\n");
                out = detail::format_hex_dump(out, ip_payload);
            }
            break;

        case ICMPV6_TYPE_ROUTER_SOLICITATION:
            if (ip_payload.size() >= Icmpv6RouterSolicitation::LENGTH) {
                Icmpv6RouterSolicitation rs;
                (void)load_unchecked(ip_payload, &rs);
                out = std::format_to(out, "  ");
                out = format_to(out, rs);
                out = std::format_to(out, "\n");
                auto options = ip_payload.subspan(Icmpv6RouterSolicitation::LENGTH);
                if (!options.empty()) {
                    out = std::format_to(out, "  NDP options ({} bytes): ", options.size());
                    out = detail::format_hex_dump(out, options);
                }
            } else {
                out = std::format_to(out, "  ICMPv6 Router Solicitation truncated\n");
                out = detail::format_hex_dump(out, ip_payload);
            }
            break;

        default: {
            // Generic ICMPv6 header for other types
            Icmpv6Header icmp_hdr;
            (void)load_unchecked(ip_payload, &icmp_hdr);
            out = std::format_to(out, "  ");
            out = format_to(out, icmp_hdr);
            out = std::format_to(out, "\n");
            auto icmp_payload = ip_payload.subspan(Icmpv6Header::LENGTH);
            if (!icmp_payload.empty()) {
                out = std::format_to(out, "  ICMPv6 payload ({} bytes): ", icmp_payload.size());
                out = detail::format_hex_dump(out, icmp_payload);
            }
            break;
        }
    }

    return out;
}

}  // namespace statusbar::ip
