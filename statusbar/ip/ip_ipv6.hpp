#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/status/status.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>

namespace statusbar::ip {

using ieee::doublet_t;
using ieee::octet_t;
using ieee::quadlet_t;

/// IPv6 address (128 bits)
struct IPv6Address
{
    static constexpr size_t LENGTH = 16;
    std::array<uint8_t, LENGTH> value;

    /// Default constructor - initializes to ::
    constexpr IPv6Address() noexcept
        : value{}
    {}

    /// Construct from 16 individual bytes
    /// @param b0 Address byte 0 (most significant) in network order
    /// @param b1 Address byte 1 in network order
    /// @param b2 Address byte 2 in network order
    /// @param b3 Address byte 3 in network order
    /// @param b4 Address byte 4 in network order
    /// @param b5 Address byte 5 in network order
    /// @param b6 Address byte 6 in network order
    /// @param b7 Address byte 7 in network order
    /// @param b8 Address byte 8 in network order
    /// @param b9 Address byte 9 in network order
    /// @param b10 Address byte 10 in network order
    /// @param b11 Address byte 11 in network order
    /// @param b12 Address byte 12 in network order
    /// @param b13 Address byte 13 in network order
    /// @param b14 Address byte 14 in network order
    /// @param b15 Address byte 15 (least significant) in network order
    constexpr IPv6Address(
        uint8_t b0,
        uint8_t b1,
        uint8_t b2,
        uint8_t b3,
        uint8_t b4,
        uint8_t b5,
        uint8_t b6,
        uint8_t b7,
        uint8_t b8,
        uint8_t b9,
        uint8_t b10,
        uint8_t b11,
        uint8_t b12,
        uint8_t b13,
        uint8_t b14,
        uint8_t b15) noexcept
        : value{b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15}
    {}

    /// Construct from 8 16-bit words in host byte order
    /// @param w0 Address word 0 (most significant) in host byte order
    /// @param w1 Address word 1 in host byte order
    /// @param w2 Address word 2 in host byte order
    /// @param w3 Address word 3 in host byte order
    /// @param w4 Address word 4 in host byte order
    /// @param w5 Address word 5 in host byte order
    /// @param w6 Address word 6 in host byte order
    /// @param w7 Address word 7 (least significant) in host byte order
    constexpr IPv6Address(
        uint16_t w0, uint16_t w1, uint16_t w2, uint16_t w3, uint16_t w4, uint16_t w5, uint16_t w6, uint16_t w7) noexcept
        : value{
              static_cast<uint8_t>(w0 >> 8),
              static_cast<uint8_t>(w0 & 0xFF),
              static_cast<uint8_t>(w1 >> 8),
              static_cast<uint8_t>(w1 & 0xFF),
              static_cast<uint8_t>(w2 >> 8),
              static_cast<uint8_t>(w2 & 0xFF),
              static_cast<uint8_t>(w3 >> 8),
              static_cast<uint8_t>(w3 & 0xFF),
              static_cast<uint8_t>(w4 >> 8),
              static_cast<uint8_t>(w4 & 0xFF),
              static_cast<uint8_t>(w5 >> 8),
              static_cast<uint8_t>(w5 & 0xFF),
              static_cast<uint8_t>(w6 >> 8),
              static_cast<uint8_t>(w6 & 0xFF),
              static_cast<uint8_t>(w7 >> 8),
              static_cast<uint8_t>(w7 & 0xFF)}
    {}

    /// Get the size of the IPv6 address in bytes
    [[nodiscard]] static constexpr auto size() noexcept -> size_t { return LENGTH; }

    /// Get a span view of the address bytes
    [[nodiscard]] constexpr auto span() const noexcept -> std::span<uint8_t const> { return value; }
    [[nodiscard]] constexpr auto span() noexcept -> std::span<uint8_t> { return value; }

    /// Get 16-bit word at index (0-7) in host byte order
    /// @param index Word index (0-7, where 0 is the most significant)
    [[nodiscard]] constexpr auto word(size_t const index) const noexcept -> uint16_t
    {
        size_t const i = index * 2;
        return static_cast<uint16_t>((static_cast<uint16_t>(value[i]) << 8) | value[i + 1]);
    }

    /// Check if this is the unspecified address (::)
    [[nodiscard]] constexpr auto is_unspecified() const noexcept -> bool
    {
        for (auto b : value) {
            if (b != 0) {
                return false;
            }
        }
        return true;
    }

    /// Check if this is the loopback address (::1)
    [[nodiscard]] constexpr auto is_loopback() const noexcept -> bool
    {
        for (size_t i = 0; i < 15; ++i) {
            if (value[i] != 0) {
                return false;
            }
        }
        return value[15] == 1;
    }

    /// Check if this is a multicast address (ff00::/8)
    [[nodiscard]] constexpr auto is_multicast() const noexcept -> bool { return value[0] == 0xFF; }

    /// Check if this is a link-local address (fe80::/10)
    [[nodiscard]] constexpr auto is_link_local() const noexcept -> bool { return value[0] == 0xFE && (value[1] & 0xC0) == 0x80; }

    /// Check if this is a unique local address (fc00::/7)
    [[nodiscard]] constexpr auto is_unique_local() const noexcept -> bool { return (value[0] & 0xFE) == 0xFC; }

    /// Check if this is an IPv4-mapped IPv6 address (::ffff:0:0/96)
    [[nodiscard]] constexpr auto is_ipv4_mapped() const noexcept -> bool
    {
        for (size_t i = 0; i < 10; ++i) {
            if (value[i] != 0) {
                return false;
            }
        }
        return value[10] == 0xFF && value[11] == 0xFF;
    }

    auto operator<=>(IPv6Address const& rhs) const noexcept -> std::strong_ordering = default;
};

/// IPv6 header (40 bytes, fixed size)
/// Network byte order is handled by doublet_t/quadlet_t types
struct IPv6Header
{
    // Next header values (same as IPv4 protocol numbers for common protocols)
    static constexpr uint8_t NEXT_HEADER_HOP_BY_HOP = 0;
    static constexpr uint8_t NEXT_HEADER_ICMPV6 = 58;
    static constexpr uint8_t NEXT_HEADER_TCP = 6;
    static constexpr uint8_t NEXT_HEADER_UDP = 17;
    static constexpr uint8_t NEXT_HEADER_ROUTING = 43;
    static constexpr uint8_t NEXT_HEADER_FRAGMENT = 44;
    static constexpr uint8_t NEXT_HEADER_NO_NEXT = 59;

    // Header length is always 40 bytes
    static constexpr size_t LENGTH = 40;

    // Version (4 bits) + Traffic Class (8 bits) + Flow Label (20 bits)
    quadlet_t version_class_flow;
    doublet_t payload_length;  // Payload length (not including this header)
    octet_t next_header;       // Next header type
    octet_t hop_limit;         // Hop limit (like TTL)
    IPv6Address src_addr;      // Source address
    IPv6Address dst_addr;      // Destination address

    /// Default constructor
    constexpr IPv6Header() noexcept = default;

    /// Get IP version (should be 6)
    [[nodiscard]] constexpr auto version() const noexcept -> uint8_t
    {
        return version_class_flow.get_bits<uint8_t>(0xF0000000, 28);
    }

    /// Set IP version
    /// @param v IP version number (should be 6)
    constexpr void set_version(uint8_t v) noexcept { version_class_flow.set_bits(0xF0000000, 28, v); }

    /// Get traffic class (8 bits)
    [[nodiscard]] constexpr auto traffic_class() const noexcept -> uint8_t
    {
        return version_class_flow.get_bits<uint8_t>(0x0FF00000, 20);
    }

    /// Set traffic class
    /// @param tc Traffic class value (8 bits)
    constexpr void set_traffic_class(uint8_t tc) noexcept { version_class_flow.set_bits(0x0FF00000, 20, tc); }

    /// Get DSCP (upper 6 bits of traffic class)
    [[nodiscard]] constexpr auto dscp() const noexcept -> uint8_t { return version_class_flow.get_bits<uint8_t>(0x0FC00000, 22); }

    /// Get ECN (lower 2 bits of traffic class)
    [[nodiscard]] constexpr auto ecn() const noexcept -> uint8_t { return version_class_flow.get_bits<uint8_t>(0x00300000, 20); }

    /// Get flow label (20 bits)
    [[nodiscard]] constexpr auto flow_label() const noexcept -> uint32_t
    {
        return version_class_flow.get_bits<uint32_t>(0x000FFFFF, 0);
    }

    /// Set flow label
    /// @param fl Flow label value (20 bits)
    constexpr void set_flow_label(uint32_t fl) noexcept { version_class_flow.set_bits(0x000FFFFF, 0, fl); }

    /// Initialize for a typical packet
    /// @param next_hdr Next header type (e.g., NEXT_HEADER_TCP, NEXT_HEADER_UDP)
    /// @param payload_len Payload length in bytes (excluding this header)
    /// @param hops Hop limit (default 64)
    constexpr void init(uint8_t next_hdr, uint16_t payload_len, uint8_t hops = 64) noexcept
    {
        version_class_flow = 0x60000000;  // Version 6, TC 0, Flow Label 0
        payload_length = payload_len;
        next_header = next_hdr;
        hop_limit = hops;
    }

    /// Check if the header has valid field values
    /// Validates version is 6
    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
    {
        // Version must be 6
        return version() == 6;
    }

    auto operator<=>(IPv6Header const& rhs) const noexcept -> std::strong_ordering = default;
};

// Compile-time layout verification
static_assert(sizeof(IPv6Header) == 40, "IPv6Header must be exactly 40 bytes");
static_assert(alignof(IPv6Header) <= 4, "IPv6Header alignment must not exceed 4 bytes");
static_assert(offsetof(IPv6Header, version_class_flow) == 0, "version_class_flow must be at offset 0");
static_assert(offsetof(IPv6Header, payload_length) == 4, "payload_length must be at offset 4");
static_assert(offsetof(IPv6Header, next_header) == 6, "next_header must be at offset 6");
static_assert(offsetof(IPv6Header, hop_limit) == 7, "hop_limit must be at offset 7");
static_assert(offsetof(IPv6Header, src_addr) == 8, "src_addr must be at offset 8");
static_assert(offsetof(IPv6Header, dst_addr) == 24, "dst_addr must be at offset 24");

}  // namespace statusbar::ip

// Serialization traits
// IPv6Address wraps an array and needs custom serialization
template <>
struct statusbar::traits::is_serializable_fixed_struct<statusbar::ip::IPv6Address> : std::true_type
{};

// IPv6Header is a packed wire format struct
template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<statusbar::ip::IPv6Header> : std::true_type
{};

// Export free functions for ADL serialization
// Note: wire_size, can_load, can_store, load, store are provided by
// statusbar::protocol for SerializableFixedStruct types
namespace statusbar::ip {

/// Load an IPv6 address from a buffer without bounds checking
/// @param buf Source buffer containing the address bytes
/// @param item Pointer to IPv6Address to populate
[[nodiscard]] inline auto load_unchecked(std::span<uint8_t const> const buf, IPv6Address* const item) noexcept -> size_t
{
    return protocol::load_unchecked(buf, &item->value);
}

/// Store an IPv6 address to a buffer without bounds checking
/// @param buf Destination buffer to write the address bytes to
/// @param item IPv6Address to store
[[nodiscard]] inline auto store_unchecked(std::span<uint8_t> const buf, IPv6Address const& item) noexcept -> size_t
{
    return protocol::store_unchecked(buf, item.value);
}

// Import template functions for wire fixed struct types (IPv6Header)
using protocol::load_unchecked;
using protocol::store_unchecked;

}  // namespace statusbar::ip
