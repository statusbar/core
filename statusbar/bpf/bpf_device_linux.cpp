// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#ifdef __linux__

#    include <fcntl.h>
#    include <unistd.h>

#    include <cerrno>
#    include <cstring>
#    include <ctime>

#    include <arpa/inet.h>
#    include <linux/if_packet.h>
#    include <net/if.h>
#    include <sys/ioctl.h>
#    include <sys/socket.h>

#endif

#include "statusbar/bpf/bpf.hpp"
#include "statusbar/bpf/bpf_base.hpp"
#include "statusbar/bpf/bpf_device_base.hpp"
#include "statusbar/bpf/bpf_device_linux.hpp"

#include <cstdio>

#include <sys/types.h>

#ifdef __linux__

#    include "statusbar/net/net_posix_util.hpp"
#    include "statusbar/status/status.hpp"

#    include <cstdint>
#    include <exception>
#    include <expected>
#    include <functional>
#    include <memory>
#    include <optional>
#    include <print>
#    include <span>
#    include <string>

namespace statusbar::bpf {

namespace {

/// Get interface index from name
/// @param sock Socket file descriptor
/// @param interface_name Network interface name
/// @return Interface index or -1 on error
auto get_interface_index(int sock, char const* interface_name) -> int
{
    ifreq ifr{};
    std::snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", interface_name);

    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        return -1;
    }

    return ifr.ifr_ifindex;
}

/// Set promiscuous mode on interface
/// @param sock Socket file descriptor
/// @param interface_index Network interface index
/// @param enable true to enable, false to disable
/// @return Status indicating success or failure
auto set_promiscuous(int sock, int interface_index, bool enable) -> Status
{
    if (sock != -1 && interface_index != -1) {
        struct packet_mreq mreq{};
        mreq.mr_ifindex = interface_index;
        mreq.mr_type = PACKET_MR_PROMISC;

        if (setsockopt(sock, SOL_PACKET, enable ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            return failure(BpfError::set_promiscuous_failed);
        }
    }

    return success();
}

}  // anonymous namespace

auto BpfDeviceLinux::process_packet(std::span<uint8_t const> const ethernet_frame, int64_t const timestamp_ns) -> Status
{
    // Invoke callback with exception safety
#    if __cpp_exceptions
    try {
#    endif
        if (callback()) {
            callback()(
                ethernet_frame, AcquisitionTimeAssociation{.bpf_time_ns = timestamp_ns, .monotonic_clock_time_ns = timestamp_ns});
        }
        return success();
#    if __cpp_exceptions
    } catch (std::exception const& e) {
        std::print(stderr, "[BPF] Callback exception: {}\n", e.what());
        mutable_statistics().callback_errors++;
        return failure(BpfError::callback_exception);
    } catch (...) {
        std::print(stderr, "[BPF] Unknown callback exception\n");
        mutable_statistics().callback_errors++;
        return failure(BpfError::callback_exception);
    }
#    endif
}

auto BpfDeviceLinux::receive_pdus() -> Status
{
    if (!is_valid()) {
        return failure(BpfError::invalid_file_descriptor);
    }

    // Use buffer size from filter params, or default
    size_t const buffer_size = filter_params().buffer_size.value_or(bpf_buffer_size);
    auto buffer = std::make_unique<uint8_t[]>(buffer_size);

    // Receive packet with MSG_TRUNC to detect truncation
    ssize_t const bytes_read = ::recv(file_descriptor(), buffer.get(), buffer_size, 0);

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return success();
        }
        mutable_statistics().read_errors++;
        return failure(BpfError::read_failed);
    }

    if (bytes_read == 0) {
        return success();
    }

    // Get timestamp
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        mutable_statistics().read_errors++;
        return failure(BpfError::clock_failed);
    }

    int64_t const timestamp_ns = (static_cast<int64_t>(ts.tv_sec) * 1'000'000'000) + static_cast<int64_t>(ts.tv_nsec);

    // Validate minimum Ethernet frame size
    if (bytes_read >= static_cast<ssize_t>(ETHERNET_HEADER_MIN_SIZE)) {
        mutable_statistics().packets_received++;
        (void)process_packet(std::span<uint8_t const>(buffer.get(), static_cast<size_t>(bytes_read)), timestamp_ns);
    }

    return success();
}

BpfDeviceLinux::BpfDeviceLinux(std::string const& network_device, FilterParams filter)
    : BpfDeviceLinux{network_device, filter, open_bpf_device(network_device, filter)}
{}

BpfDeviceLinux::BpfDeviceLinux(std::string const& network_device, FilterParams filter, OpenedDevice opened)
    : BpfDeviceBase{network_device, filter, opened.fd}
    , interface_index_{opened.interface_index}
{}

BpfDeviceLinux::~BpfDeviceLinux() noexcept
{
    if (is_valid()) {
        (void)set_promiscuous(file_descriptor(), interface_index_, false);
        close_bpf_device(file_descriptor());
    }
}

auto BpfDeviceLinux::open_bpf_device(std::string const& network_device, FilterParams const& filter) -> OpenedDevice
{
    // Create AF_PACKET socket
    // Use SOCK_RAW to get full Ethernet frames
    // Specify protocol in network byte order to filter at socket level
    int const sock = ::socket(AF_PACKET, SOCK_RAW, htons(filter.ethertype));
    if (sock < 0) {
        return {};
    }

    // Get interface index
    int const ifindex = get_interface_index(sock, network_device.c_str());
    if (ifindex < 0) {
        ::close(sock);
        return {};
    }

    // Bind socket to interface
    sockaddr_ll sll{};
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(filter.ethertype);
    sll.sll_ifindex = ifindex;

    if (::bind(sock, ::statusbar::net::sockaddr_cast(sll), sizeof(sll)) < 0) {
        ::close(sock);
        return {};
    }

    if (filter.promiscuous) {
        auto status = set_promiscuous(sock, ifindex, true);
        if (!status) {
            ::close(sock);
            return {};
        }
    }

    // Set socket buffer size if specified
    if (filter.buffer_size.has_value()) {
        int bufsize = static_cast<int>(filter.buffer_size.value());
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0) {
            // Non-fatal, continue anyway
        }
    }

    // Set non-blocking mode if timeout is specified
    if (filter.read_timeout.has_value()) {
        int const flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }
    }

    return {.fd = sock, .interface_index = ifindex};
}

void BpfDeviceLinux::close_bpf_device(int file_descriptor) noexcept
{
    if (file_descriptor != -1) {
        ::close(file_descriptor);
    }
}

}  // namespace statusbar::bpf

#endif