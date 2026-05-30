#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/ieee/ieee_ethernet.hpp"

#include <cstddef>
#include <cstdint>

/// Shared protocol constants for IEEE 802 and Internet protocols.
/// This module provides EtherType values, IP protocol numbers, multicast addresses, and common protocol sizes.
namespace statusbar::ieee::protocols {

//
// EtherType Values (used in Ethernet frame header)
//
constexpr std::uint16_t ETHERTYPE_IPV4 = 0x0800;  ///< Internet Protocol version 4
constexpr std::uint16_t ETHERTYPE_ARP = 0x0806;   ///< Address Resolution Protocol
constexpr std::uint16_t ETHERTYPE_MSRP = 0x22EA;  ///< Multiple Stream Reservation Protocol (IEEE 802.1Q-2014)
constexpr std::uint16_t ETHERTYPE_AVTP = 0x22F0;  ///< Audio Video Bridging / TSN (IEEE 1722)
constexpr std::uint16_t ETHERTYPE_VLAN = 0x8100;  ///< VLAN-tagged frame (IEEE 802.1Q)
constexpr std::uint16_t ETHERTYPE_IPV6 = 0x86DD;  ///< Internet Protocol version 6
constexpr std::uint16_t ETHERTYPE_MVRP = 0x88F5;  ///< Multiple VLAN Registration Protocol (IEEE 802.1Q-2014)
constexpr std::uint16_t ETHERTYPE_GPTP = 0x88F7;  ///< gPTP (IEEE 802.1AS)

//
// Multicast MAC Addresses
//
/// MVRP multicast MAC address (01:80:C2:00:00:21) - IEEE 802.1Q-2014
inline constexpr Eui48 MVRP_MULTICAST_MAC(0x01, 0x80, 0xC2, 0x00, 0x00, 0x21);

/// MSRP multicast MAC address (01:80:C2:00:00:0E) - IEEE 802.1Q-2014
inline constexpr Eui48 MSRP_MULTICAST_MAC(0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E);

/// gPTP multicast MAC address (01:80:C2:00:00:0E) - IEEE 802.1AS
/// Used for Peer-to-Peer messages (Pdelay_Req, Pdelay_Resp, Announce, Sync, Follow_Up)
inline constexpr Eui48 GPTP_MULTICAST_MAC(0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E);

/// IEEE 1722.1 ATDECC multicast MAC address (91:E0:F0:01:00:00)
/// Used for ACMP, ADP, and AECP discovery messages
inline constexpr Eui48 ATDECC_MULTICAST_MAC(0x91, 0xE0, 0xF0, 0x01, 0x00, 0x00);

/// IEEE 1722.1 ATDECC identification multicast MAC address (91:E0:F0:01:00:01)
/// Used for entity identification
inline constexpr Eui48 ATDECC_IDENTIFY_MULTICAST_MAC(0x91, 0xE0, 0xF0, 0x01, 0x00, 0x01);

/// MAAP multicast MAC address (91:E0:F0:00:FF:00) - IEEE 1722 Table B.10
/// Destination address for MAAP_PROBE and MAAP_ANNOUNCE frames
inline constexpr Eui48 MAAP_MULTICAST_MAC(0x91, 0xE0, 0xF0, 0x00, 0xFF, 0x00);

//
// IP Protocol Numbers (used in IPv4 header protocol field)
//
constexpr std::uint8_t IP_PROTO_ICMP = 1;   ///< Internet Control Message Protocol
constexpr std::uint8_t IP_PROTO_IGMP = 2;   ///< Internet Group Management Protocol
constexpr std::uint8_t IP_PROTO_TCP = 6;    ///< Transmission Control Protocol
constexpr std::uint8_t IP_PROTO_UDP = 17;   ///< User Datagram Protocol
constexpr std::uint8_t IP_PROTO_IPV6 = 41;  ///< IPv6 encapsulation

//
// Protocol Header Sizes (in bytes)
//
// Layer 2 (Data Link)
constexpr std::size_t ETHERNET_HEADER_SIZE = 14;       ///< Ethernet frame header (no VLAN)
constexpr std::size_t ETHERNET_VLAN_HEADER_SIZE = 18;  ///< Ethernet frame header (with VLAN)
constexpr std::size_t ARP_HEADER_SIZE = 28;            ///< ARP header (IPv4 over Ethernet)

// Layer 3 (Network)
constexpr std::size_t IPV4_HEADER_MIN_SIZE = 20;  ///< IPv4 header minimum size (no options)
constexpr std::size_t IPV6_HEADER_SIZE = 40;      ///< IPv6 header fixed size

// Layer 4 (Transport)
constexpr std::size_t TCP_HEADER_MIN_SIZE = 20;  ///< TCP header minimum size (no options)
constexpr std::size_t UDP_HEADER_SIZE = 8;       ///< UDP header fixed size
constexpr std::size_t ICMP_HEADER_SIZE = 8;      ///< ICMP header minimum size
constexpr std::size_t IGMP_HEADER_SIZE = 8;      ///< IGMP header fixed size

// Layer 2 Application (IEEE 1722)
constexpr std::size_t AVTP_COMMON_HEADER_SIZE = 24;  ///< AVTP common header fixed size

}  // namespace statusbar::ieee::protocols
