// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/net/net_tap_bridge.hpp"

#if defined(__linux__)

namespace statusbar::net {

void TapBridge::close() noexcept
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    ifindex_ = 0;
    drain_budget_ = 8;
}

auto TapBridge::open(TapBridgeConfig const& config) -> Status
{
    close();

    // Open the TUN/TAP device
    int const tun_fd = ::open("/dev/net/tun", O_RDWR);
    if (tun_fd < 0) {
        return failure(std::error_code(errno, std::system_category()));
    }

    // Configure as TAP device (Ethernet frames) with no packet information header
    struct ifreq ifr{};
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

    // Copy device name safely
    auto const name_len = config.device_name.size() < IFNAMSIZ ? config.device_name.size() : IFNAMSIZ - 1;
    for (size_t i = 0; i < name_len; ++i) {
        ifr.ifr_name[i] = config.device_name[i];
    }
    ifr.ifr_name[name_len] = '\0';

    if (::ioctl(tun_fd, TUNSETIFF, &ifr) < 0) {
        ::close(tun_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    // Set non-blocking mode
    int const flags = ::fcntl(tun_fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(tun_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(tun_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    // Get interface index
    unsigned int const idx = ::if_nametoindex(ifr.ifr_name);
    if (idx == 0) {
        ::close(tun_fd);
        return failure(std::error_code(errno, std::system_category()));
    }

    fd_ = tun_fd;
    ifindex_ = idx;
    drain_budget_ = config.drain_budget > 0 ? config.drain_budget : 8;

    return success();
}

auto TapBridge::forward_to_tap(std::span<uint8_t const> frame) -> Status
{
    auto const n = ::write(fd_, frame.data(), frame.size());

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            // Kernel TAP buffer temporarily full — not fatal
            return success();
        }
        return failure(std::error_code(errno, std::system_category()));
    }

    return success();
}

void TapBridge::swap(TapBridge& other) noexcept
{
    int const tmp_fd = fd_;
    fd_ = other.fd_;
    other.fd_ = tmp_fd;

    unsigned int const tmp_idx = ifindex_;
    ifindex_ = other.ifindex_;
    other.ifindex_ = tmp_idx;

    size_t const tmp_budget = drain_budget_;
    drain_budget_ = other.drain_budget_;
    other.drain_budget_ = tmp_budget;
}

}  // namespace statusbar::net

#endif  // __linux__
