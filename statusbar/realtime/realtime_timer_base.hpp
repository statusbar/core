#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/itc/itc_stop_token.hpp"
#include "statusbar/realtime/realtime_timer_config.hpp"
#include "statusbar/stats/stats.hpp"

#include <atomic>
#include <cstdint>
#include <thread>

namespace statusbar::realtime {

/// Non-template base class for Timer.
///
/// Holds everything that does not depend on the clock adapter template: config,
/// statistics, running flag, worker thread, and the thread prepare / sleep
/// primitives. Derived Timer<ClockAdapterT> supplies the per-tick loop body
/// via the pure virtual run_loop() hook.
class TimerBase
{
  public:
    explicit TimerBase(TimerConfig config) noexcept;
    virtual ~TimerBase() noexcept;

    TimerBase(TimerBase const&) = delete;
    auto operator=(TimerBase const&) -> TimerBase& = delete;
    TimerBase(TimerBase&&) = delete;
    auto operator=(TimerBase&&) -> TimerBase& = delete;

    /// Start the timer thread
    /// @param shutdown_token Optional external stop token to check in addition to internal running state
    auto start(statusbar::itc::StopToken* shutdown_token = nullptr) -> void;

    /// Stop the timer thread (blocks until thread exits)
    auto stop() noexcept -> void;

    [[nodiscard]] auto is_running() const noexcept -> bool { return is_running_atomic(); }
    [[nodiscard]] auto stats() const noexcept -> stats::AtomicWakeStats::Snapshot { return stats_.snapshot(); }
    [[nodiscard]] auto config() const noexcept -> TimerConfig const& { return config_; }
    [[nodiscard]] auto period_ns() const noexcept -> int64_t { return config_.period_ns; }
    [[nodiscard]] auto compensation_ns() const noexcept -> int64_t { return config_.compensation_ns; }
    [[nodiscard]] auto cpu() const noexcept -> int { return config_.cpu; }
    [[nodiscard]] auto is_busy_wait() const noexcept -> bool { return config_.busy_wait; }

  protected:
    /// Derived class supplies the timing loop body.
    virtual auto run_loop() -> void = 0;

    /// Gating hook called after prepare_thread() and before run_loop().
    /// Derived classes override this to wait for prerequisites (e.g. clock
    /// adapter becoming healthy). Must honour should_run() so a shutdown
    /// requested during the wait can exit the thread without entering
    /// the timing loop.
    /// @return true to proceed into run_loop(); false to exit the thread.
    [[nodiscard]] virtual auto wait_until_ready() -> bool { return true; }

    [[nodiscard]] auto is_running_atomic() const noexcept -> bool;
    auto set_running(bool value) noexcept -> void;
    [[nodiscard]] auto try_start() noexcept -> bool;
    [[nodiscard]] auto try_stop() noexcept -> bool;

    /// Return true while the timer should continue running (checks internal
    /// atomic flag and the external shutdown flag, if any).
    [[nodiscard]] auto should_run() const noexcept -> bool;

    /// Prepare thread for realtime operation: stack prefault, CPU affinity,
    /// SCHED_FIFO priority, and memory locking. Virtual so tests can override
    /// without paying the 1MB stack prefault, which overflows the default
    /// std::thread stack on some platforms.
    /// @return true on success; false only if strict_affinity is set and CPU affinity failed.
    [[nodiscard]] virtual auto prepare_thread() -> bool;

    /// Sleep-based wait until an absolute monotonic deadline.
    /// @return wake error in ns (positive = late)
    static auto wait_until_absolute(int64_t deadline_ns) noexcept -> int64_t;

    /// Hybrid sleep/busy-wait until an absolute monotonic deadline for ultra-low latency.
    /// @return wake error in ns (positive = late)
    static auto wait_until_busy(int64_t deadline_ns, int64_t threshold_ns = 100'000) noexcept -> int64_t;

    /// Result of a deadline wait: wake error (positive = late) and the number
    /// of full periods we are past the canonical deadline (0 when on time).
    struct WaitResult
    {
        int64_t error_ns;
        int64_t skipped_counts;
    };

    /// Sleep until the canonical monotonic deadline, applying
    /// config_.compensation_ns as a sleep-offset and dispatching to the
    /// sleep-based or hybrid busy-wait primitive according to
    /// config_.busy_wait. Error and skipped-cycle counts are measured against
    /// the uncompensated @p mono_deadline_ns so statistics remain comparable
    /// across compensation settings.
    auto wait_until_deadline(int64_t mono_deadline_ns) noexcept -> WaitResult;

    TimerConfig config_;
    stats::AtomicWakeStats stats_;
    statusbar::itc::StopToken* shutdown_token_{nullptr};
    std::thread timer_thread_;

  private:
    std::atomic<bool> running_{false};
};

/// Return the cycle number that contains @p time_ns for a timer of period
/// @p period_ns (floor division). A "cycle number" here is the integer index
/// of the period-sized slot the time falls within, counted from the clock's
/// zero epoch.
///
/// Precondition: period_ns > 0.
[[nodiscard]] constexpr auto cycle_number_from_ns(int64_t time_ns, int64_t period_ns) noexcept -> int64_t
{
    return time_ns / period_ns;
}

/// Return the ns time at the start of cycle @p cycle for a timer of period
/// @p period_ns. Inverse of cycle_number_from_ns (exact: round-trip restores
/// the cycle number, not necessarily the original ns value).
///
/// Also usable as "convert N cycles to a ns delta" since the math is symmetric.
[[nodiscard]] constexpr auto cycle_number_to_ns(int64_t cycle, int64_t period_ns) noexcept -> int64_t
{
    return cycle * period_ns;
}

}  // namespace statusbar::realtime
