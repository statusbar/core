// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/bpf/bpf_base.hpp"

#include "statusbar/bpf/bpf.hpp"

#include <unistd.h>

namespace statusbar::bpf {

auto BpfErrorCategory::message(int ev) const -> std::string
{
    switch (static_cast<BpfError>(ev)) {
        case BpfError::device_not_found:
            return "BPF device not found";
        case BpfError::device_busy:
            return "All BPF devices are busy";
        case BpfError::set_buffer_failed:
            return "Failed to set BPF buffer size";
        case BpfError::set_interface_failed:
            return "Failed to set network interface";
        case BpfError::set_immediate_failed:
            return "Failed to set immediate mode";
        case BpfError::set_filter_failed:
            return "Failed to set BPF filter";
        case BpfError::set_promiscuous_failed:
            return "Failed to set promiscuous mode";
        case BpfError::set_blocking_failed:
            return "Failed to set blocking mode";
        case BpfError::read_failed:
            return "Failed to read from BPF device";
        case BpfError::clock_failed:
            return "Failed to get system time";
        case BpfError::invalid_packet:
            return "Invalid packet received";
        case BpfError::buffer_overflow:
            return "Buffer overflow detected";
        case BpfError::interface_not_found:
            return "Network interface not found";
        case BpfError::socket_creation_failed:
            return "Failed to create socket";
        case BpfError::bind_failed:
            return "Failed to bind socket";
        case BpfError::invalid_file_descriptor:
            return "Invalid file descriptor";
        case BpfError::callback_exception:
            return "Exception thrown in packet callback";
        case BpfError::get_mtu_failed:
            return "Failed to get interface MTU";
        case BpfError::get_mac_address_failed:
            return "Failed to get interface MAC address";
        case BpfError::operation_not_supported:
            return "Operation not supported on this platform";
        default:
            return "Unknown BPF error";
    }
}

auto bpf_error_category() noexcept -> std::error_category const&
{
    static BpfErrorCategory const instance;
    return instance;
}

auto make_error_code(BpfError e) noexcept -> std::error_code
{
    return {static_cast<int>(e), bpf_error_category()};
}

// FileDescriptor is now aliased to ::statusbar::FileDescriptor (defined
// inline in statusbar/buffer/file_descriptor.hpp). No out-of-line
// definitions live here anymore.

}  // namespace statusbar::bpf