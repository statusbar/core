#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Network socket utilities
/// Provides FileDescriptor RAII wrapper and socket configuration functions

#include "statusbar/buffer/file_descriptor.hpp"
#include "statusbar/net/net_error.hpp"
#include "statusbar/status/status.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <expected>
#include <string_view>
#include <system_error>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>

namespace statusbar::net {

// Re-export socket type constants so users don't need to #include <sys/socket.h>
inline constexpr int SocketStream = SOCK_STREAM;
inline constexpr int SocketDatagram = SOCK_DGRAM;

/// Alias to the canonical POSIX-fd RAII wrapper. The class itself lives in
/// `statusbar/buffer/file_descriptor.hpp` so the `bpf` module can share it
/// without depending on `net`. Existing `statusbar::net::FileDescriptor`
/// call sites continue to compile unchanged.
using FileDescriptor = ::statusbar::FileDescriptor;

/// Set a socket to non-blocking mode
/// @param fd Socket file descriptor
[[nodiscard]] inline auto set_nonblocking(int fd) noexcept -> Status
{
    int const flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return failure(NetError::socket_option_failed);
    }
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return failure(NetError::socket_option_failed);
    }
    return success();
}

/// Set a socket to blocking mode
/// @param fd Socket file descriptor
[[nodiscard]] inline auto set_blocking(int fd) noexcept -> Status
{
    int const flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return failure(NetError::socket_option_failed);
    }
    if (::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        return failure(NetError::socket_option_failed);
    }
    return success();
}

/// Set SO_REUSEADDR on a socket
/// @param fd Socket file descriptor
[[nodiscard]] inline auto set_reuse_addr(int fd) noexcept -> Status
{
    int const on = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        return failure(NetError::socket_option_failed);
    }
    return success();
}

/// Set SO_REUSEPORT on a socket (if available)
/// @param fd Socket file descriptor
[[nodiscard]] inline auto set_reuse_port(int fd) noexcept -> Status
{
#if defined(SO_REUSEPORT)
    int const on = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0) {
        return failure(NetError::socket_option_failed);
    }
#else
    (void)fd;
#endif
    return success();
}

/// Check if an error indicates the operation would block
[[nodiscard]] inline auto would_block() noexcept -> bool
{
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS;
}

/// Check if an error indicates the operation was interrupted
[[nodiscard]] inline auto was_interrupted() noexcept -> bool
{
    return errno == EINTR;
}

/// Bind a socket to a specific network interface ("nic pinning"). Empty
/// `iface` is a no-op (returns success). Linux uses SO_BINDTODEVICE;
/// macOS / BSD use IP_BOUND_IF / IPV6_BOUND_IF (chosen by `family`).
/// Returns false and sets errno on failure; the caller decides whether
/// to log or propagate.
[[nodiscard]] inline auto bind_to_interface(int fd, int family, std::string_view iface) noexcept -> bool
{
    if (iface.empty()) {
        return true;
    }
#if defined(__linux__) && defined(SO_BINDTODEVICE)
    (void)family;
    return ::setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, iface.data(), static_cast<socklen_t>(iface.size())) == 0;
#else
    char name[IF_NAMESIZE]{};
    if (iface.size() >= sizeof(name)) {
        errno = EINVAL;
        return false;
    }
    for (size_t i = 0; i < iface.size(); ++i) {
        name[i] = iface[i];
    }
    auto const idx = ::if_nametoindex(name);
    if (idx == 0) {
        return false;
    }
    int const idx_int = static_cast<int>(idx);
    if (family == AF_INET6) {
#    if defined(IPV6_BOUND_IF)
        return ::setsockopt(fd, IPPROTO_IPV6, IPV6_BOUND_IF, &idx_int, sizeof(idx_int)) == 0;
#    else
        errno = ENOSYS;
        return false;
#    endif
    }
#    if defined(IP_BOUND_IF)
    return ::setsockopt(fd, IPPROTO_IP, IP_BOUND_IF, &idx_int, sizeof(idx_int)) == 0;
#    else
    errno = ENOSYS;
    return false;
#    endif
#endif
}

/// Set the DSCP (Differentiated Services Code Point) value on a socket
/// Works for both IPv4 and IPv6 sockets on macOS and Linux
/// @param fd The socket file descriptor
/// @param family The address family (AF_INET or AF_INET6)
/// @param dscp The DSCP value (0-63, 6 bits). Will be shifted into TOS/Traffic Class field.
/// @return Status indicating success or failure
[[nodiscard]] inline auto set_dscp(int fd, int family, uint8_t dscp) noexcept -> Status
{
    // DSCP is the upper 6 bits of the TOS/Traffic Class byte
    // The lower 2 bits are ECN (Explicit Congestion Notification)
    int const tos = (dscp & 0x3F) << 2;

    if (family == AF_INET6) {
        // IPv6: use IPV6_TCLASS
        if (::setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &tos, sizeof(tos)) < 0) {
            return failure(NetError::socket_creation_failed);
        }
    } else {
        // IPv4: use IP_TOS
        if (::setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
            return failure(NetError::socket_creation_failed);
        }
    }

    return success();
}

/// Set the DSCP value using the raw TOS/Traffic Class byte
/// This preserves any ECN bits if needed
/// @param fd The socket file descriptor
/// @param family The address family (AF_INET or AF_INET6)
/// @param tos The full TOS/Traffic Class byte (DSCP << 2 | ECN)
/// @return Status indicating success or failure
[[nodiscard]] inline auto set_tos(int fd, int family, uint8_t tos) noexcept -> Status
{
    int const tos_val = tos;

    if (family == AF_INET6) {
        if (::setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &tos_val, sizeof(tos_val)) < 0) {
            return failure(NetError::socket_creation_failed);
        }
    } else {
        if (::setsockopt(fd, IPPROTO_IP, IP_TOS, &tos_val, sizeof(tos_val)) < 0) {
            return failure(NetError::socket_creation_failed);
        }
    }

    return success();
}

}  // namespace statusbar::net
