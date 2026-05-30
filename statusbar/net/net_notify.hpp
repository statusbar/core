#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Notification pipe for inter-thread communication with reactor handlers
/// Provides a pipe-based mechanism for other threads to wake up a handler

#include "statusbar/net/net_error.hpp"
#include "statusbar/net/net_socket.hpp"
#include "statusbar/status/status.hpp"

#include <unistd.h>

#include <array>
#include <cstdint>

namespace statusbar::net {

/// RAII wrapper for a notification pipe pair
/// the reactor polls The read end, the "write" end can be used from any thread
class NotificationPipe
{
  public:
    /// Default constructor creates an invalid pipe (no system resources allocated)
    /// Use create() to get a valid pipe
    NotificationPipe() noexcept = default;

    /// Factory method to create a valid notification pipe
    /// @return A valid NotificationPipe, or an invalid one if creation failed
    [[nodiscard]] static auto create() noexcept -> NotificationPipe
    {
        NotificationPipe pipe;
        std::array<int, 2> fds{-1, -1};
        if (::pipe(fds.data()) == 0) {
            pipe.read_fd_ = std::get<0>(fds);
            pipe.write_fd_ = std::get<1>(fds);

            // Set both ends to non-blocking
            if (set_nonblocking(pipe.read_fd_) && set_nonblocking(pipe.write_fd_)) {
                // Success
            } else {
                // Failed to set non-blocking, close both
                pipe.close();
            }
        }
        return pipe;
    }

    ~NotificationPipe() noexcept { close(); }

    // No copy
    NotificationPipe(NotificationPipe const&) = delete;
    auto operator=(NotificationPipe const&) -> NotificationPipe& = delete;

    // Move allowed
    NotificationPipe(NotificationPipe&& other) noexcept
        : read_fd_{other.read_fd_}
        , write_fd_{other.write_fd_}
    {
        other.read_fd_ = -1;
        other.write_fd_ = -1;
    }

    auto operator=(NotificationPipe&& other) noexcept -> NotificationPipe&
    {
        if (this != &other) {
            close();
            read_fd_ = other.read_fd_;
            write_fd_ = other.write_fd_;
            other.read_fd_ = -1;
            other.write_fd_ = -1;
        }
        return *this;
    }

    /// Check if the pipe was created successfully
    [[nodiscard]] auto valid() const noexcept -> bool { return read_fd_ >= 0 && write_fd_ >= 0; }

    /// Get the read end fd (for polling by reactor)
    [[nodiscard]] auto read_fd() const noexcept -> int { return read_fd_; }

    /// Get the "write" end fd (for signaling from other threads)
    [[nodiscard]] auto write_fd() const noexcept -> int { return write_fd_; }

    /// Send a notification byte (thread-safe, can be called from any thread)
    /// @param code The notification code to send
    /// @return true if the byte was written, false if the pipe is full or closed
    [[nodiscard]] auto notify(uint8_t code) const noexcept -> bool
    {
        if (write_fd_ < 0) {
            return false;
        }
        ssize_t const n = ::write(write_fd_, &code, 1);
        return n == 1;
    }

    /// Read one notification byte (called by reactor on readable)
    /// @param code Output: the notification code read
    /// @return true if a byte was read, false if no data or error
    [[nodiscard]] auto read_one(uint8_t& code) const noexcept -> bool
    {
        if (read_fd_ < 0) {
            return false;
        }
        ssize_t const n = ::read(read_fd_, &code, 1);
        return n == 1;
    }

    /// Drain all pending notifications, calling a callback for each
    /// @param callback Function to call with each notification code
    template <typename Callback>
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward) - callback is called multiple times in the loop
    auto drain(Callback&& callback) noexcept
    {
        uint8_t code{};
        while (read_one(code)) {
            callback(code);
        }
    }

    /// Close both ends of the pipe
    void close() noexcept
    {
        if (read_fd_ >= 0) {
            ::close(read_fd_);
            read_fd_ = -1;
        }
        if (write_fd_ >= 0) {
            ::close(write_fd_);
            write_fd_ = -1;
        }
    }

  private:
    int read_fd_{-1};
    int write_fd_{-1};
};

}  // namespace statusbar::net
