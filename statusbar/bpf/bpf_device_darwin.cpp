// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#ifndef __linux__

#    include <fcntl.h>
#    include <unistd.h>

#    include <cerrno>
#    include <cinttypes>
#    include <cstdio>
#    include <cstring>

#    include <net/bpf.h>
#    include <net/if.h>
#    include <sys/ioctl.h>
#    include <sys/time.h>
#    include <sys/types.h>

#endif

#include "statusbar/bpf/bpf.hpp"

#ifndef __linux__

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
#    include <system_error>

namespace statusbar::bpf {

namespace {

/// Try to open an available BPF device
/// @return File descriptor or -1 if none available
auto open_bpf() -> int
{
    char dev[16];
    for (int i = 0; i < bpf_max_device_num; i++) {
        std::snprintf(dev, sizeof(dev), "/dev/bpf%d", i);
        int const fd = ::open(dev, O_RDWR);
        if (fd != -1) {
            return fd;
        }
        if (errno != EBUSY) {
            break;  // Fatal error, not just busy
        }
    }
    return -1;
}

/// Configure BPF device with interface and filter
/// @param file_descriptor Open BPF device file descriptor
/// @param eth_port Network interface name
/// @param filter Filter configuration
/// @return Status indicating success or failure
auto set_bpf(int file_descriptor, char const* eth_port, FilterParams const& filter) -> Status
{
    // Set the buffer length (must be done before BIOCSETIF)
    unsigned int buflen = filter.buffer_size.value_or(bpf_buffer_size);
    if (ioctl(file_descriptor, BIOCSBLEN, &buflen) < 0) {
        return failure(BpfError::set_buffer_failed);
    }

    // Bind to network interface
    ifreq ifr{};
    std::snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", eth_port);

    if (ioctl(file_descriptor, BIOCSETIF, &ifr) < 0) {
        return failure(BpfError::set_interface_failed);
    }

    // Enable immediate mode (return immediately when packet received)
    unsigned int enable = 1;
    if (ioctl(file_descriptor, BIOCIMMEDIATE, &enable) < 0) {
        return failure(BpfError::set_immediate_failed);
    }

    // Set promiscuous mode if requested
    if (filter.promiscuous) {
        if (ioctl(file_descriptor, BIOCPROMISC, nullptr) < 0) {
            return failure(BpfError::set_promiscuous_failed);
        }
    }

    // Configure BPF filter for specified EtherType
    // Generated from: tcpdump -ddd -i en0 ether proto <ethertype>
    // BPF bytecode:
    //   ldh [12]                 ; Load half-word (16-bit) at offset 12 (EtherType field)
    //   jeq #<ethertype>, L1, L2 ; Jump if equal to ethertype
    //   L1: ret #524288          ; Accept packet (return max capture length)
    //   L2: ret #0               ; Reject packet
    bpf_insn filter_code[] = {
        {.code = 0x28, .jt = 0, .jf = 0, .k = 12},                     // ldh [12]
        {.code = 0x15, .jt = 0, .jf = 1, .k = 0},                      // jeq #<ethertype>, skip 0, skip 1
        {.code = 0x06, .jt = 0, .jf = 0, .k = bpf_filter_max_packet},  // ret #524288
        {.code = 0x06, .jt = 0, .jf = 0, .k = 0},                      // ret #0
    };
    filter_code[1].k = filter.ethertype;

    bpf_program prog = {.bf_len = sizeof(filter_code) / sizeof(bpf_insn), .bf_insns = filter_code};

    if (ioctl(file_descriptor, BIOCSETF, &prog) < 0) {
        return failure(BpfError::set_filter_failed);
    }

    return success();
}

}  // anonymous namespace

auto BpfDeviceDarwin::process_packet(
    timeval const packet_time_realtime_clock,
    timespec const receive_time_monotonic_clock,
    std::span<uint8_t const> const ethernet_frame) -> Status
{
    // Convert BPF timestamp to nanoseconds
    int64_t const bpf_time_ns = (static_cast<int64_t>(packet_time_realtime_clock.tv_sec) * 1'000'000'000) +
        (static_cast<int64_t>(packet_time_realtime_clock.tv_usec) * 1'000);

    // Convert monotonic timestamp to nanoseconds
    int64_t const monotonic_time_ns = (static_cast<int64_t>(receive_time_monotonic_clock.tv_sec) * 1'000'000'000) +
        (static_cast<int64_t>(receive_time_monotonic_clock.tv_nsec));

    // Invoke callback with exception safety
#    if __cpp_exceptions
    try {
#    endif
        if (callback()) {
            callback()(
                ethernet_frame,
                AcquisitionTimeAssociation{.bpf_time_ns = bpf_time_ns, .monotonic_clock_time_ns = monotonic_time_ns});
        }
        return success();
#    if __cpp_exceptions
    } catch (std::exception const& e) {
        std::println(stderr, "[BPF] Callback exception: {}", e.what());
        mutable_statistics().callback_errors++;
        return failure(BpfError::callback_exception);
    } catch (...) {
        std::println(stderr, "[BPF] Unknown callback exception");
        mutable_statistics().callback_errors++;
        return failure(BpfError::callback_exception);
    }
#    endif
}

auto BpfDeviceDarwin::capture_packets() -> StatusValue<int>
{
    int packet_count = 0;

    // Use buffer size from filter params, or default
    size_t const buffer_size = filter_params().buffer_size.value_or(bpf_buffer_size);
    auto buffer = std::make_unique<char[]>(buffer_size);

    ssize_t const bytes_read = ::read(file_descriptor(), buffer.get(), buffer_size);
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return success(0);
        }
        mutable_statistics().read_errors++;
        return failure(BpfError::read_failed);
    }

    if (bytes_read == 0) {
        return success(0);
    }

    // Capture a single monotonic timestamp for this entire batch. Each packet
    // also carries its own BPF kernel timestamp (bh_tstamp), which is more
    // precise. The monotonic timestamp here provides a common reference for
    // correlating the batch with other system clocks. For timing-sensitive
    // protocols (gPTP, CRF), use the per-packet BPF timestamp, not this one.
    timespec monotonic_time{};
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &monotonic_time) != 0) {
        mutable_statistics().read_errors++;
        return failure(BpfError::clock_failed);
    }

    // Process all packets in the buffer
    ssize_t offset = 0;
    while (offset < bytes_read) {
        // Bounds check: Ensure we have room for bpf_hdr
        if (offset + static_cast<ssize_t>(sizeof(bpf_hdr)) > bytes_read) {
            mutable_statistics().buffer_overflows++;
            break;  // Truncated/corrupted buffer
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto* hdr = reinterpret_cast<bpf_hdr*>(buffer.get() + offset);

        // Bounds check: Ensure packet doesn't extend beyond buffer
        ssize_t const packet_end = offset + hdr->bh_hdrlen + hdr->bh_caplen;
        if (packet_end > bytes_read) {
            mutable_statistics().buffer_overflows++;
            break;  // Packet extends beyond buffer
        }

        // Validate packet has minimum Ethernet header
        if (hdr->bh_caplen >= ETHERNET_HEADER_MIN_SIZE) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            auto const* packet = reinterpret_cast<uint8_t const*>(buffer.get() + offset + hdr->bh_hdrlen);

            packet_count++;
            mutable_statistics().packets_received++;

            (void)process_packet(
                {.tv_sec = hdr->bh_tstamp.tv_sec, .tv_usec = hdr->bh_tstamp.tv_usec},
                monotonic_time,
                std::span<uint8_t const>(packet, static_cast<size_t>(hdr->bh_caplen)));
        }

        // Advance to next packet (BPF_WORDALIGN ensures proper alignment).
        // Guard against the corrupt-header case where bh_hdrlen + bh_caplen
        // is zero — without this, the loop would not make progress and would
        // spin forever on a hostile or buggy buffer.
        ssize_t const advance = static_cast<ssize_t>(BPF_WORDALIGN(hdr->bh_hdrlen + hdr->bh_caplen));
        if (advance <= 0) {
            mutable_statistics().buffer_overflows++;
            break;
        }
        offset += advance;
    }

    return success(packet_count);
}

BpfDeviceDarwin::BpfDeviceDarwin(std::string const& network_device, FilterParams filter)
    : BpfDeviceBase{network_device, filter, open_bpf_device(network_device, filter)}
{}

BpfDeviceDarwin::~BpfDeviceDarwin() noexcept
{
    if (is_valid()) {
        close_bpf_device(file_descriptor());
    }
}

auto BpfDeviceDarwin::receive_pdus() -> Status
{
    if (!is_valid()) {
        return failure(BpfError::invalid_file_descriptor);
    }

    auto result = capture_packets();
    if (!result) {
        return failure(result.error());
    }

    return success();
}

auto BpfDeviceDarwin::open_bpf_device(std::string const& network_device, FilterParams const& filter) -> int
{
    int const fd = open_bpf();
    if (fd == -1) {
        return -1;
    }

    // Configure the BPF device
    auto status = set_bpf(fd, network_device.c_str(), filter);
    if (!status) {
        ::close(fd);
        return -1;
    }

    // Set non-blocking mode if timeout is specified
    if (filter.read_timeout.has_value()) {
        int const flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    return fd;
}

void BpfDeviceDarwin::close_bpf_device(int file_descriptor) noexcept
{
    if (file_descriptor != -1) {
        ::close(file_descriptor);
    }
}

}  // namespace statusbar::bpf

#endif