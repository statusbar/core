// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/net/net_traffic_classifier.hpp"

#include <optional>

namespace statusbar::net {

namespace {

/// Layer-2 header fields extracted from a raw frame. `vlan_offset` is
/// the number of extra bytes the VLAN tag adds to every downstream
/// offset computation (0 when untagged, 4 when tagged).
struct FrameHeader
{
    uint16_t outer_ethertype;
    uint16_t ethertype;
    bool is_vlan_tagged;
    uint16_t vlan_vid;
    uint8_t vlan_pcp;
    size_t vlan_offset;
};

auto read_be16(std::span<uint8_t const> frame, size_t offset) noexcept -> uint16_t
{
    return static_cast<uint16_t>((static_cast<uint16_t>(frame[offset]) << 8) | static_cast<uint16_t>(frame[offset + 1]));
}

/// Returns nullopt if the frame is too short to carry an Ethernet II
/// header (and optionally an 802.1Q tag).
auto parse_frame_header(std::span<uint8_t const> frame) noexcept -> std::optional<FrameHeader>
{
    // Need at least 14 bytes to read the ethertype.
    if (frame.size() < 14) {
        return std::nullopt;
    }

    FrameHeader hdr{};
    hdr.outer_ethertype = read_be16(frame, 12);
    hdr.is_vlan_tagged = (hdr.outer_ethertype == 0x8100U);
    hdr.ethertype = hdr.outer_ethertype;

    if (!hdr.is_vlan_tagged) {
        return hdr;
    }

    // VLAN header occupies bytes 14-17:
    //   bytes 14-15: TCI (PCP[15:13] | DEI[12] | VID[11:0])
    //   bytes 16-17: real ethertype
    if (frame.size() < 18) {
        return std::nullopt;
    }
    uint16_t const tci = read_be16(frame, 14);
    hdr.vlan_vid = tci & 0x0FFFU;
    hdr.vlan_pcp = static_cast<uint8_t>((tci >> 13) & 0x07U);
    hdr.ethertype = read_be16(frame, 16);
    hdr.vlan_offset = 4;  // VLAN tag is 4 bytes (TCI + real ethertype)
    return hdr;
}

constexpr auto vlan_pcp_mismatches(ClassifierRule const& rule, bool tagged, uint8_t pcp) noexcept -> bool
{
    return rule.vlan_pcp != 0xFF && tagged && pcp != rule.vlan_pcp;
}

}  // namespace

auto TrafficClassifier::classify(std::span<uint8_t const> frame) const noexcept -> FrameAction
{
    auto const parsed = parse_frame_header(frame);
    if (!parsed) {
        return default_action_;
    }
    auto const& [outer_ethertype, ethertype, is_vlan_tagged, vlan_vid, vlan_pcp, vlan_offset] = *parsed;

    // Evaluate rules in order — first match wins
    for (auto const& rule : rules_) {
        // a. Ethertype check
        if (rule.ethertype != 0 && rule.ethertype != ethertype) {
            continue;
        }

        // b. VLAN matching
        if (rule.vlan_match == VlanMatch::untagged_only && is_vlan_tagged) {
            continue;
        }
        if (rule.vlan_match == VlanMatch::tagged_any && !is_vlan_tagged) {
            continue;
        }
        if (rule.vlan_match == VlanMatch::tagged_exact) {
            if (!is_vlan_tagged || vlan_vid != rule.vlan_id) {
                continue;
            }
        }

        // c. VLAN PCP check (0xFF = any)
        if (vlan_pcp_mismatches(rule, is_vlan_tagged, vlan_pcp)) {
            continue;
        }

        // d. IP protocol check
        if (rule.ip_protocol != 0) {
            bool const is_ipv4 = (ethertype == 0x0800U);
            bool const is_ipv6 = (ethertype == 0x86DDU);
            if (!is_ipv4 && !is_ipv6) {
                continue;
            }
            // IPv4: protocol field at IP header offset +9
            // IPv6: next header field at IP header offset +6
            size_t const ip_proto_offset = 14U + vlan_offset + (is_ipv6 ? 6U : 9U);
            if (frame.size() <= ip_proto_offset) {
                continue;
            }
            if (frame[ip_proto_offset] != rule.ip_protocol) {
                continue;
            }
        }

        // e. UDP destination port check
        if (rule.udp_port != 0) {
            if (rule.ip_protocol != 17U) {
                continue;
            }
            // IPv4: 20-60 bytes (read IHL from low nibble of byte 0).
            // IPv6 base header: 40 bytes (extension headers ignored — UDP
            // traffic in TSN/AVB contexts does not chain extension headers).
            bool const is_ipv6 = (ethertype == 0x86DDU);
            size_t const ip_start = 14U + vlan_offset;
            size_t ip_hdr_len = 0;
            if (is_ipv6) {
                ip_hdr_len = 40U;
            } else {
                if (frame.size() <= ip_start) {
                    continue;
                }
                // IHL is the low nibble of byte 0 and counts 32-bit words.
                ip_hdr_len = static_cast<size_t>(frame[ip_start] & 0x0FU) * 4U;
                // RFC 791: IHL minimum is 5 (20 bytes). Anything smaller
                // is malformed; skip this rule rather than reading into
                // the payload as if it were the UDP header.
                if (ip_hdr_len < 20U) {
                    continue;
                }
            }
            // UDP dest port is at offset 2 within the UDP header
            size_t const udp_port_offset = ip_start + ip_hdr_len + 2U;
            if (frame.size() < udp_port_offset + 2U) {
                continue;
            }
            uint16_t const dest_port = static_cast<uint16_t>(
                (static_cast<uint16_t>(frame[udp_port_offset]) << 8) | static_cast<uint16_t>(frame[udp_port_offset + 1U]));
            if (dest_port != rule.udp_port) {
                continue;
            }
        }

        // All checks passed — this rule matches
        return rule.action;
    }

    // No rule matched
    return default_action_;
}

}  // namespace statusbar::net
