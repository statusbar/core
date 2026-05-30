#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Cooperative stop signal for "please stop" coordination across
/// threads, including from POSIX signal handlers. Bundles a
/// signal-safe atomic flag with a condition variable so sleeping
/// waiters can wake immediately on stop instead of polling.
///
/// Replaces the bare `std::atomic_flag` / `std::atomic<bool>*`
/// patterns previously scattered across the codebase.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace statusbar::itc {

class StopToken
{
  public:
    StopToken() noexcept;

    StopToken(StopToken const&) = delete;
    StopToken(StopToken&&) = delete;
    StopToken& operator=(StopToken const&) = delete;
    StopToken& operator=(StopToken&&) = delete;

    /// Request a stop. Sets the underlying atomic flag AND notifies
    /// any cv-waiters so they wake immediately. Idempotent. NOT
    /// signal-safe — call signal_set() from signal handlers instead.
    void request_stop() noexcept;

    /// Signal-handler-safe variant: sets the underlying atomic flag
    /// only, without notifying cv-waiters. Waiters wake on their
    /// next wait_for_stop timeout, or earlier if any regular thread
    /// subsequently calls request_stop().
    void signal_set() noexcept;

    /// Non-blocking check. Race-free for any thread.
    [[nodiscard]] auto stop_requested() const noexcept -> bool;

    /// Wait until stop is requested or `timeout` elapses. Returns
    /// true if stop was requested, false on timeout. Multiple
    /// waiters may call concurrently; all are woken on
    /// request_stop().
    template <typename Rep, typename Period>
    [[nodiscard]] auto wait_for_stop(std::chrono::duration<Rep, Period> timeout) const noexcept -> bool
    {
        return wait_for_stop_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(timeout));
    }

  private:
    [[nodiscard]] auto wait_for_stop_ns(std::chrono::nanoseconds timeout) const noexcept -> bool;

    std::atomic_flag flag_;
    mutable std::mutex mu_;
    mutable std::condition_variable cv_;
};

/// Install SIGINT/SIGTERM handlers that call signal_set() on a
/// process-static StopToken; SIGPIPE is ignored. Returns a reference
/// to the static token. Idempotent across multiple calls.
[[nodiscard]] auto install_stop_signal() noexcept -> StopToken&;

}  // namespace statusbar::itc
