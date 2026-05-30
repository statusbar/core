#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/bpf/bpf_base.hpp"
#include "statusbar/bpf/bpf_device_base.hpp"
#include "statusbar/status/status.hpp"

#include <span>
#include <string>

#include <sys/time.h>

namespace statusbar::bpf {

/// Darwin/macOS BPF device implementation using /dev/bpf*
///
/// This implementation uses Berkeley Packet Filter (BPF) devices available
/// on Darwin/macOS through /dev/bpf* character devices.
///
/// Features:
/// - Automatic BPF device selection from /dev/bpf0 to /dev/bpf254
/// - Hardware timestamping from kernel BPF
/// - Efficient batch packet processing
/// - EtherType filtering in kernel space
///
/// Limitations:
/// - Monotonic timestamp is captured per read batch, not per packet
/// - All packets in a batch share the same monotonic timestamp
class BpfDeviceDarwin final : public BpfDeviceBase
{
  public:
    /// Construct Darwin BPF device
    /// @param network_device Network interface name (e.g., "en0")
    /// @param filter Filter configuration parameters
    BpfDeviceDarwin(std::string const& network_device, FilterParams filter);

    ~BpfDeviceDarwin() noexcept override;

    // Non-copyable and non-movable (owns file descriptor)
    BpfDeviceDarwin(BpfDeviceDarwin const&) = delete;
    auto operator=(BpfDeviceDarwin const&) -> BpfDeviceDarwin& = delete;
    BpfDeviceDarwin(BpfDeviceDarwin&&) = delete;
    auto operator=(BpfDeviceDarwin&&) -> BpfDeviceDarwin& = delete;

    /// Receive and process packets
    /// @return Status indicating success or failure
    [[nodiscard]] auto receive_pdus() -> Status override;

  private:
    /// Process a single packet from BPF buffer
    /// @param packet_time_realtime_clock Kernel timestamp from BPF
    /// @param receive_time_monotonic_clock Monotonic timestamp when read() was called
    /// @param ethernet_frame Complete Ethernet frame
    /// @return Status indicating success or failure (e.g., callback exception)
    [[nodiscard]] auto process_packet(
        timeval packet_time_realtime_clock, timespec receive_time_monotonic_clock, std::span<uint8_t const> ethernet_frame)
        -> Status;

    /// Capture packets from BPF device
    /// @return Number of packets captured, or error status
    [[nodiscard]] auto capture_packets() -> StatusValue<int>;

    /// Open and configure BPF device
    /// @param network_device Network interface name
    /// @param filter Filter configuration
    /// @return File descriptor or -1 on error
    static auto open_bpf_device(std::string const& network_device, FilterParams const& filter) -> int;

    /// Close BPF device file descriptor
    static void close_bpf_device(int file_descriptor) noexcept;
};

}  // namespace statusbar::bpf
