#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/ieee/ieee.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace statusbar::pcap {

/// Packet data type - a vector of bytes representing an Ethernet frame
using Packet = std::vector<uint8_t>;

/// PCAP file magic numbers
constexpr uint32_t PCAP_MAGIC_NATIVE = 0xa1b2c3d4;
constexpr uint32_t PCAP_MAGIC_SWAPPED = 0xd4c3b2a1;

/// PCAP file header (from Wireshark libpcap format)
/// See: http://wiki.wireshark.org/Development/LibpcapFileFormat
struct FileHeader
{
    uint32_t magic_number;   ///< Magic number (0xa1b2c3d4 or byte-swapped)
    uint16_t version_major;  ///< Major version number
    uint16_t version_minor;  ///< Minor version number
    int32_t thiszone;        ///< GMT to local correction
    uint32_t sigfigs;        ///< Accuracy of timestamps
    uint32_t snaplen;        ///< Max length of captured packets, in octets
    uint32_t network;        ///< Data link type (1 = Ethernet)
};

/// PCAP packet record header
struct RecordHeader
{
    uint32_t ts_sec;   ///< Timestamp seconds
    uint32_t ts_usec;  ///< Timestamp microseconds
    int32_t incl_len;  ///< Number of octets of packet saved in file
    int32_t orig_len;  ///< Actual length of packet
};

/// Swap bytes of a 32-bit value (for endian conversion)
[[nodiscard]] constexpr auto swap_bytes(uint32_t v) noexcept -> uint32_t
{
    uint32_t r = 0;
    r |= ((v >> 24) & 0x000000ff);
    r |= ((v >> 8) & 0x0000ff00);
    r |= ((v << 8) & 0x00ff0000);
    r |= ((v << 24) & 0xff000000);
    return r;
}

/// Get current time in microseconds since epoch
[[nodiscard]] inline auto get_current_time_in_microseconds() noexcept -> uint64_t
{
    auto const now = std::chrono::system_clock::now();
    auto const duration = now.time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
}

/// RAII wrapper for FILE* with custom deleter
using FilePtr = std::unique_ptr<FILE, void (*)(FILE*)>;

/// Create a FilePtr from a filename and mode
/// @param filename The file path to open
/// @param mode The fopen mode string (e.g., "rb", "wb")
/// @return A FilePtr managing the opened file, or nullptr on failure
[[nodiscard]] inline auto make_file(std::string const& filename, char const* mode) noexcept -> FilePtr
{
    FILE* f = std::fopen(filename.c_str(), mode);
    return {f, [](FILE* fp) -> void {
                if (fp) {
                    std::fclose(fp);
                }
            }};
}

}  // namespace statusbar::pcap
