#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// TapBridge — RAII wrapper for a Linux TAP device with bidirectional frame forwarding.
/// Supports forwarding from any EthernetPort backend to a TAP device and vice versa.
/// On non-Linux platforms, all operations return not_supported.

#if defined(__linux__)

#    include <fcntl.h>
#    include <unistd.h>

#    include <cerrno>

#    include <linux/if_tun.h>
#    include <net/if.h>
#    include <sys/ioctl.h>

#endif  // __linux__

#include "statusbar/net/net_error.hpp"
#include "statusbar/net/net_ethernet_port.hpp"
#include "statusbar/status/status.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <system_error>

namespace statusbar::net {

/// Configuration for TapBridge
struct TapBridgeConfig
{
    std::string_view device_name;  ///< TAP device name (e.g., "tap0")
    size_t drain_budget{8};        ///< Max frames per drain_to_wire call
};

#if defined(__linux__)

/// RAII wrapper for a Linux TAP device with bidirectional frame forwarding.
///
/// Factored out of XdpContext to be reusable with any EthernetPort backend.
/// The drain_to_wire() method is templated on EthernetPort so it works with
/// MmapContext, XdpContext, BpfPortContext, or any compliant backend.
///
/// Move-only. Not thread-safe.
class TapBridge
{
  public:
    TapBridge() = default;

    ~TapBridge() { close(); }

    // Move-only semantics
    TapBridge(TapBridge&& other) noexcept { swap(other); }

    auto operator=(TapBridge&& other) noexcept -> TapBridge&
    {
        if (this != &other) {
            close();
            swap(other);
        }
        return *this;
    }

    TapBridge(TapBridge const&) = delete;
    auto operator=(TapBridge const&) -> TapBridge& = delete;

    /// Open the TAP device with the given configuration.
    /// Opens /dev/net/tun, configures IFF_TAP | IFF_NO_PI, sets O_NONBLOCK,
    /// and retrieves the interface index via if_nametoindex().
    /// @param config Configuration specifying device name and drain budget
    [[nodiscard]] auto open(TapBridgeConfig const& config) -> Status;

    /// Close the TAP device and release all resources.
    void close() noexcept;

    /// Returns true if the TAP device is open.
    [[nodiscard]] auto valid() const noexcept -> bool { return fd_ >= 0; }

    /// Get the underlying file descriptor (for poll/epoll integration).
    [[nodiscard]] auto fd() const noexcept -> int { return fd_; }

    /// Get the interface index of the TAP device.
    [[nodiscard]] auto ifindex() const noexcept -> unsigned int { return ifindex_; }

    /// TAP -> wire: read frames from TAP device and transmit via the given port.
    ///
    /// Loops up to drain_budget_ times:
    ///   1. tx_guard() to acquire a TX guard; if full, stop
    ///   2. read() from the TAP fd into the guard buffer
    ///   3. On success: commit the guard and increment count
    ///   4. On EAGAIN or error: guard auto-cancels on scope exit
    ///
    /// Returns the number of frames drained and transmitted.
    /// @param port EthernetPort backend to transmit frames through
    template <EthernetPort Port>
    [[nodiscard]] auto drain_to_wire(Port& port) -> StatusValue<size_t>
    {
        size_t count = 0;

        for (size_t i = 0; i < drain_budget_; ++i) {
            // Acquire a TX guard (auto-cancels on destruction if not committed)
            auto guard_result = tx_guard(port, 0);
            if (!guard_result.has_value()) {
                // TX ring full — stop draining
                break;
            }

            auto& guard = *guard_result;

            // Read one frame from the TAP device
            auto const n = ::read(fd_, guard.buffer().data(), guard.buffer().size());

            if (n > 0) {
                // Successfully read a frame — commit it for transmission
                auto commit_status = guard.commit(static_cast<size_t>(n));
                if (!commit_status.has_value()) {
                    return failure(commit_status.error());
                }
                ++count;
            } else {
                // EAGAIN means no more frames available — not an error
                // Any other error or n == 0 also means stop
                // Guard auto-cancels on break
                break;
            }
        }

        return success(count);
    }

    /// Wire -> TAP: write a received Ethernet frame to the TAP device.
    ///
    /// EAGAIN is treated as non-fatal (TAP kernel buffer temporarily full).
    /// Returns failure on other write errors.
    /// @param frame Complete Ethernet frame to forward to the TAP device
    [[nodiscard]] auto forward_to_tap(std::span<uint8_t const> frame) -> Status;

  private:
    void swap(TapBridge& other) noexcept;

    int fd_{-1};
    unsigned int ifindex_{0};
    size_t drain_budget_{8};
};

#else  // !__linux__

/// Non-Linux stub for TapBridge.
/// All operations return not_supported.
class TapBridge
{
  public:
    TapBridge() = default;
    ~TapBridge() = default;

    TapBridge(TapBridge&&) noexcept = default;
    auto operator=(TapBridge&&) noexcept -> TapBridge& = default;
    TapBridge(TapBridge const&) = delete;
    auto operator=(TapBridge const&) -> TapBridge& = delete;

    [[nodiscard]] auto open(TapBridgeConfig const&) -> Status { return failure(NetError::not_supported); }

    void close() noexcept {}

    [[nodiscard]] auto valid() const noexcept -> bool { return false; }

    [[nodiscard]] auto fd() const noexcept -> int { return -1; }

    [[nodiscard]] auto ifindex() const noexcept -> unsigned int { return 0; }

    template <EthernetPort Port>
    [[nodiscard]] auto drain_to_wire(Port&) -> StatusValue<size_t>
    {
        return failure(NetError::not_supported);
    }

    [[nodiscard]] auto forward_to_tap(std::span<uint8_t const>) -> Status { return failure(NetError::not_supported); }
};

#endif  // __linux__

}  // namespace statusbar::net
