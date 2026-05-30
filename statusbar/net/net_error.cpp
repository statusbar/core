// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/net/net_error.hpp"

#include <string>
#include <system_error>

namespace statusbar::net {

auto NetErrorCategory::message(int ev) const -> std::string
{
    switch (static_cast<NetError>(ev)) {
        case NetError::socket_creation_failed:
            return "Socket creation failed";
        case NetError::bind_failed:
            return "Bind failed";
        case NetError::listen_failed:
            return "Listen failed";
        case NetError::connect_failed:
            return "Connect failed";
        case NetError::accept_failed:
            return "Accept failed";
        case NetError::send_failed:
            return "Send failed";
        case NetError::receive_failed:
            return "Receive failed";
        case NetError::getaddrinfo_failed:
            return "getaddrinfo failed";
        case NetError::poll_failed:
            return "Poll failed";
        case NetError::handler_limit_reached:
            return "Handler limit reached";
        case NetError::buffer_full:
            return "Buffer full";
        case NetError::connection_closed:
            return "Connection closed";
        case NetError::would_block:
            return "Operation would block";
        case NetError::invalid_address:
            return "Invalid address";
        case NetError::not_connected:
            return "Not connected";
        case NetError::already_connected:
            return "Already connected";
        case NetError::socket_option_failed:
            return "Socket option failed";
        case NetError::invalid_argument:
            return "Invalid argument";
        case NetError::invalid_handle:
            return "Invalid slot handle";
        case NetError::not_supported:
            return "Operation not supported on this platform";
        default:
            return "Unknown network error";
    }
}

auto net_error_category() noexcept -> std::error_category const&
{
    static NetErrorCategory const instance;
    return instance;
}

auto make_error_code(NetError e) noexcept -> std::error_code
{
    return {static_cast<int>(e), net_error_category()};
}

}  // namespace statusbar::net
