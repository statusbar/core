#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Network module error types
/// Provides NetError enum and std::error_category integration

#include <string>
#include <system_error>

namespace statusbar::net {

/// Network error codes
enum class NetError
{
    socket_creation_failed = 1,
    bind_failed,
    listen_failed,
    connect_failed,
    accept_failed,
    send_failed,
    receive_failed,
    getaddrinfo_failed,
    poll_failed,
    handler_limit_reached,
    buffer_full,
    connection_closed,
    would_block,
    invalid_address,
    not_connected,
    already_connected,
    socket_option_failed,
    invalid_argument,
    invalid_handle,
    not_supported,
};

/// Error category for NetError
class NetErrorCategory : public std::error_category
{
  public:
    [[nodiscard]] auto name() const noexcept -> char const* override { return "statusbar.net"; }

    /// @param ev Error code value to convert to a message string
    [[nodiscard]] auto message(int ev) const -> std::string override;
};

/// Get the singleton NetError category instance
[[nodiscard]] auto net_error_category() noexcept -> std::error_category const&;

/// Create an error_code from a NetError
/// @param e The NetError value to convert
[[nodiscard]] auto make_error_code(NetError e) noexcept -> std::error_code;

}  // namespace statusbar::net

/// Register NetError as an error code enum
template <>
struct std::is_error_code_enum<statusbar::net::NetError> : std::true_type
{};
