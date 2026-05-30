// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/ip/ip_ipv4.hpp"

#include <array>
#include <cstdint>

namespace statusbar::ip {

auto calculate_ipv4_checksum(IPv4Header const& header) noexcept -> uint16_t
{
    // Copy header to byte array to avoid strict aliasing violation
    std::array<uint8_t, sizeof(IPv4Header)> bytes{};
    span_store(bytes, header);

    uint32_t sum = 0;

    // Sum all 16-bit words (10 words for 20-byte header)
    // Read bytes in network byte order (big-endian)
    for (size_t i = 0; i < sizeof(IPv4Header); i += 2) {
        uint16_t const word = static_cast<uint16_t>((static_cast<uint16_t>(bytes[i]) << 8) | bytes[i + 1]);
        sum += word;
    }

    // Fold 32-bit sum to 16 bits
    while ((sum >> 16) != 0) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

void update_ipv4_checksum(IPv4Header& header) noexcept
{
    header.header_checksum = 0;
    header.header_checksum = calculate_ipv4_checksum(header);
}

auto verify_ipv4_checksum(IPv4Header const& header) noexcept -> bool
{
    return calculate_ipv4_checksum(header) == 0;
}

}  // namespace statusbar::ip
