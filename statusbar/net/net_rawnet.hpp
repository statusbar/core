#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Raw Ethernet (Layer 2) socket utilities
/// Provides RawnetContext for sending/receiving raw Ethernet frames
/// Supports Linux (AF_PACKET) and macOS (BPF)

#include <unistd.h>

#include <cerrno>
#include <cstring>

#include <net/if.h>
#include <sys/ioctl.h>

#if defined(__linux__)
#    include <arpa/inet.h>
#    include <linux/if_ether.h>
#    include <linux/if_packet.h>
#    include <sys/socket.h>
#elif defined(__APPLE__)
#    include <fcntl.h>
#    include <ifaddrs.h>

#    include <net/bpf.h>
#    include <net/if_dl.h>
#    include <sys/socket.h>
#    include <sys/types.h>
#endif

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/net/net_error.hpp"
#include "statusbar/net/net_socket.hpp"
#include "statusbar/status/status.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace statusbar::net {

using ieee::Eui48;

/// Maximum Ethernet frame size (standard MTU + headers, no VLAN)
inline constexpr size_t MAX_ETHERNET_FRAME_SIZE = 1514;

/// Maximum Ethernet frame size with VLAN tag (MTU + headers + VLAN tag)
inline constexpr size_t MAX_ETHERNET_FRAME_SIZE_VLAN = 1518;

/// Maximum Ethernet frame size with double VLAN (QinQ)
inline constexpr size_t MAX_ETHERNET_FRAME_SIZE_QINQ = 1522;

/// Minimum Ethernet frame size (excluding FCS)
inline constexpr size_t MIN_ETHERNET_FRAME_SIZE = 60;

/// Ethernet header size (dest MAC + src MAC + ethertype) — from ieee::protocols
using ieee::protocols::ETHERNET_HEADER_SIZE;

/// VLAN tag size (TPID + TCI)
inline constexpr size_t VLAN_TAG_SIZE = 4;

/// Maximum payload size (standard MTU)
inline constexpr size_t MAX_ETHERNET_PAYLOAD = 1500;

/// Maximum payload size including VLAN tag bytes that appear in payload
/// When receiving VLAN-tagged frames, the kernel may include the VLAN tag
/// in the payload depending on configuration
inline constexpr size_t MAX_ETHERNET_PAYLOAD_WITH_VLAN = 1504;

/// Context for raw Ethernet socket operations
/// Provides platform-independent interface for Layer 2 networking
class RawnetContext
{
  public:
    RawnetContext() noexcept = default;
    ~RawnetContext() noexcept { close(); }

    // No copy
    RawnetContext(RawnetContext const&) = delete;
    auto operator=(RawnetContext const&) -> RawnetContext& = delete;

    // Move allowed
    RawnetContext(RawnetContext&& other) noexcept
        : fd_{other.fd_}
        , ethertype_{other.ethertype_}
        , interface_index_{other.interface_index_}
        , my_mac_{other.my_mac_}
        , default_dest_mac_{other.default_dest_mac_}
#if defined(__APPLE__)
        , bpf_buffer_size_{other.bpf_buffer_size_}
#endif
    {
        other.fd_ = -1;
        other.interface_index_ = -1;
    }

    auto operator=(RawnetContext&& other) noexcept -> RawnetContext&;

    /// Open a raw Ethernet socket
    /// @param interface_name Network interface name (e.g., "en0", "eth0")
    /// @param ethertype EtherType to filter/use (e.g., 0x88f7 for gPTP)
    /// @param multicast_mac Optional multicast MAC address to join (nullptr = none)
    /// @return Status indicating success or failure
    [[nodiscard]] auto open(std::string_view interface_name, uint16_t ethertype, Eui48 const* multicast_mac = nullptr) noexcept
        -> Status;

    /// Close the socket
    void close() noexcept;

    /// Send a raw Ethernet frame including ethernet header.
    /// @param full_frame Full frame data (with Ethernet header)
    /// @return Number of bytes sent, or error
    [[nodiscard]] auto send(std::span<uint8_t const> full_frame) const noexcept -> StatusValue<ssize_t>;

    /// Send an Ethernet frame
    /// @param dest_mac Destination MAC address (nullptr = use default)
    /// @param payload Payload data (without Ethernet header)
    /// @return Number of bytes sent, or error
    [[nodiscard]] auto send(Eui48 const* dest_mac, std::span<uint8_t const> payload) noexcept -> StatusValue<ssize_t>;

    /// Receive an Ethernet frame
    /// @param src_mac Output: source MAC address
    /// @param dest_mac Output: destination MAC address
    /// @param payload_buf Buffer to receive payload (without Ethernet header)
    /// @return Number of payload bytes received, or error
    [[nodiscard]] auto recv(Eui48* src_mac, Eui48* dest_mac, std::span<uint8_t> payload_buf) noexcept -> StatusValue<ssize_t>;

    /// Join a multicast group
    /// @param multicast_mac Multicast MAC address to join
    /// @return Status indicating success or failure
    [[nodiscard]] auto join_multicast(Eui48 const& multicast_mac) const noexcept -> Status;

    /// Check if the socket is valid
    [[nodiscard]] auto valid() const noexcept -> bool { return fd_ >= 0; }

    /// Get the file descriptor
    [[nodiscard]] auto fd() const noexcept -> int { return fd_; }

    /// Get the EtherType
    [[nodiscard]] auto ethertype() const noexcept -> uint16_t { return ethertype_; }

    /// Get the interface index
    [[nodiscard]] auto interface_index() const noexcept -> int { return interface_index_; }

    /// Get our MAC address
    [[nodiscard]] auto my_mac() const noexcept -> Eui48 const& { return my_mac_; }

    /// Get the default destination MAC address
    [[nodiscard]] auto default_dest_mac() const noexcept -> Eui48 const& { return default_dest_mac_; }

    /// Set the default destination MAC address
    /// @param mac MAC address to use as the default destination
    void set_default_dest_mac(Eui48 const& mac) noexcept { default_dest_mac_ = mac; }

#if defined(__APPLE__)
    /// Get the BPF buffer size (macOS only)
    [[nodiscard]] auto bpf_buffer_size() const noexcept -> size_t { return bpf_buffer_size_; }
#endif

  private:
    int fd_{-1};
    uint16_t ethertype_{0};
    int interface_index_{-1};
    Eui48 my_mac_{};
    Eui48 default_dest_mac_{};

#if defined(__APPLE__)
    size_t bpf_buffer_size_{0};
    std::array<uint8_t, 32768> bpf_read_buffer_{};
    size_t bpf_read_offset_{0};
    size_t bpf_read_length_{0};
#endif
};

//
// Platform-specific system includes are in net_rawnet_impl.cpp
//

}  // namespace statusbar::net
