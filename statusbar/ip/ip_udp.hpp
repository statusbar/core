#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/ip/ip_port_numbers.hpp"
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

/// UDP header (8 bytes, fixed size)
/// Network byte order is handled by doublet_t type
///
/// Well-known port numbers are available in the statusbar::ip::port namespace.
/// See ip_port_numbers.cppm for the complete list.
struct UdpHeader
{
    // Header length is always 8 bytes
    static constexpr size_t LENGTH = 8;

    doublet_t src_port;  // Source port
    doublet_t dst_port;  // Destination port
    doublet_t length;    // Length (header + payload)
    doublet_t checksum;  // Checksum (optional for IPv4, required for IPv6)

    /// Default constructor
    constexpr UdpHeader() noexcept = default;

    /// Construct with port numbers and length
    /// @param src Source port number
    /// @param dst Destination port number
    /// @param len Total length including header and payload
    constexpr UdpHeader(uint16_t src, uint16_t dst, uint16_t len) noexcept
        : src_port(src)
        , dst_port(dst)
        , length(len)
        , checksum(0)
    {}

    /// Get payload length (length - header size)
    [[nodiscard]] constexpr auto payload_length() const noexcept -> uint16_t
    {
        uint16_t const total = length;
        return (total >= LENGTH) ? static_cast<uint16_t>(total - LENGTH) : 0;
    }

    /// Initialize for a typical packet
    /// @param src Source port number
    /// @param dst Destination port number
    /// @param payload_len Payload length in bytes (excluding header)
    constexpr void init(uint16_t src, uint16_t dst, uint16_t payload_len) noexcept
    {
        src_port = src;
        dst_port = dst;
        length = static_cast<uint16_t>(LENGTH + payload_len);
        checksum = 0;
    }

    /// Check if the header has valid field values
    /// Validates length is at least 8 bytes (minimum UDP header size)
    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
    {
        // Length must be at least 8 bytes (header size)
        return length.get() >= LENGTH;
    }

    auto operator<=>(UdpHeader const& rhs) const noexcept -> std::strong_ordering = default;
};

// Compile-time layout verification
static_assert(sizeof(UdpHeader) == 8, "UdpHeader must be exactly 8 bytes");
static_assert(alignof(UdpHeader) <= 2, "UdpHeader alignment must not exceed 2 bytes");
static_assert(offsetof(UdpHeader, src_port) == 0, "src_port must be at offset 0");
static_assert(offsetof(UdpHeader, dst_port) == 2, "dst_port must be at offset 2");
static_assert(offsetof(UdpHeader, length) == 4, "length must be at offset 4");
static_assert(offsetof(UdpHeader, checksum) == 6, "checksum must be at offset 6");

}  // namespace statusbar::ip

// Serialization traits - UdpHeader is a packed wire format struct
template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<statusbar::ip::UdpHeader> : std::true_type
{};

// Export using declarations for ADL to find the template functions
namespace statusbar::ip {
using protocol::load_unchecked;
using protocol::store_unchecked;
}  // namespace statusbar::ip
