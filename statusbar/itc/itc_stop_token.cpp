// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/itc/itc_stop_token.hpp"

#include <csignal>

namespace statusbar::itc {

StopToken::StopToken() noexcept = default;

void StopToken::request_stop() noexcept
{
    // Set the flag first (signal-safe op). The empty lock-then-unlock
    // dance on mu_ forces the requester to wait until any concurrent
    // waiter has progressed past cv_.wait_for's internal
    // "release lock → register sleeper" step. Without it,
    // notify_all() could fire in the window between the waiter
    // releasing mu_ to sleep and actually being in the cv wait
    // queue, and the wake would be lost until the next timeout.
    flag_.test_and_set(std::memory_order_release);
    {
        std::lock_guard<std::mutex> const lock{mu_};
    }
    cv_.notify_all();
}

void StopToken::signal_set() noexcept
{
    // Signal-handler-safe: only the atomic_flag set. The cv notify
    // is NOT called because std::condition_variable::notify_all is
    // not guaranteed async-signal-safe. Waiters wake on their next
    // wait_for_stop timeout, or earlier if a regular thread later
    // calls request_stop().
    flag_.test_and_set(std::memory_order_release);
}

auto StopToken::stop_requested() const noexcept -> bool
{
    return flag_.test(std::memory_order_acquire);
}

auto StopToken::wait_for_stop_ns(std::chrono::nanoseconds timeout) const noexcept -> bool
{
    // unique_lock construction and cv_.wait_for can both throw
    // std::system_error (e.g. EAGAIN / EINVAL from the underlying pthread
    // call). The function is declared noexcept so an escaping exception
    // would call std::terminate. Swallow and fall through to the atomic
    // check, which still produces a correct answer for callers.
#if __cpp_exceptions
    try {
#endif
        std::unique_lock<std::mutex> lock{mu_};
        cv_.wait_for(lock, timeout, [&]() noexcept { return flag_.test(std::memory_order_acquire); });
#if __cpp_exceptions
    } catch (...) {  // NOLINT(bugprone-empty-catch)
        // intentional: noexcept contract preserved; result derives from the
        // atomic flag, not the cv wakeup.
    }
#endif
    return flag_.test(std::memory_order_acquire);
}

namespace {

inline StopToken& static_token() noexcept
{
    static StopToken instance;
    return instance;
}

void signal_handler(int /*sig*/)
{
    static_token().signal_set();
}

}  // namespace

auto install_stop_signal() noexcept -> StopToken&
{
    std::signal(SIGINT, &signal_handler);
    std::signal(SIGTERM, &signal_handler);
    std::signal(SIGPIPE, SIG_IGN);
    return static_token();
}

}  // namespace statusbar::itc
