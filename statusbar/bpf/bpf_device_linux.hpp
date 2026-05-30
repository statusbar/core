#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/bpf/bpf_base.hpp"
#include "statusbar/bpf/bpf_device_base.hpp"
#include "statusbar/status/status.hpp"

#include <span>
#include <string>

namespace statusbar::bpf {

/// Linux BPF device implementation using AF_PACKET sockets
///
/// This implementation uses Linux AF_PACKET sockets for raw packet capture.
/// AF_PACKET provides direct access to network interfaces at the data link layer.
///
/// Features:
/// - Uses AF_PACKET sockets with SOCK_RAW
/// - Protocol-level filtering via socket creation
/// - Per-packet timestamping
/// - Efficient zero-copy packet reception
///
/// Note: Requires root privileges or CAP_NET_RAW capability
class BpfDeviceLinux final : public BpfDeviceBase
{
  public:
    /// Construct Linux BPF device
    /// @param network_device Network interface name (e.g., "eth0")
    /// @param filter Filter configuration parameters
    BpfDeviceLinux(std::string const& network_device, FilterParams filter);

    ~BpfDeviceLinux() noexcept override;

    // Non-copyable (owns file descriptor)
    BpfDeviceLinux(BpfDeviceLinux const&) = delete;
    auto operator=(BpfDeviceLinux const&) -> BpfDeviceLinux& = delete;

    // Non-movable (prevent accidental moves during callback lifetime)
    BpfDeviceLinux(BpfDeviceLinux&&) = delete;
    auto operator=(BpfDeviceLinux&&) -> BpfDeviceLinux& = delete;

    /// Receive and process packets
    /// @return Status indicating success or failure
    [[nodiscard]] auto receive_pdus() -> Status override;

  private:
    /// Process a single received packet
    /// @param ethernet_frame Complete Ethernet frame
    /// @param timestamp_ns Packet timestamp in nanoseconds
    /// @return Status indicating success or failure (e.g., callback exception)
    [[nodiscard]] auto process_packet(std::span<uint8_t const> ethernet_frame, int64_t timestamp_ns) -> Status;

    /// Holds the two values that need to be produced before the base class
    /// is constructed (so the base can take ownership of `fd`) AND need to
    /// flow into a derived-class member (`interface_index_`). Returned by
    /// the static open helper and consumed by the delegated constructor.
    struct OpenedDevice
    {
        int fd{-1};
        int interface_index{-1};
    };

    /// Open and configure AF_PACKET socket. Static so it cannot accidentally
    /// touch derived members before the derived ctor begins.
    /// @param network_device Network interface name
    /// @param filter Filter configuration
    /// @return Open result; fd == -1 on error.
    [[nodiscard]] static auto open_bpf_device(std::string const& network_device, FilterParams const& filter) -> OpenedDevice;

    /// Delegated ctor that publishes the static open result into the base
    /// (fd) and the derived (interface_index_) in one well-defined sequence.
    BpfDeviceLinux(std::string const& network_device, FilterParams filter, OpenedDevice opened);

    /// Close socket file descriptor
    static void close_bpf_device(int file_descriptor) noexcept;

    int interface_index_{-1};
};

}  // namespace statusbar::bpf
