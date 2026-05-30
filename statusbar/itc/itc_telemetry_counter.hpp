#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// A monotone cross-thread counter. One producer thread accumulates
/// via add(); one or more reader threads sample via load(). Staleness
/// of a sample is acceptable — the contract is "eventually correct
/// count", not a synchronized read.
///
/// Replaces bare `std::atomic<T>` counters that were being used as an
/// ad-hoc RT->reporter telemetry channel.

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace statusbar::itc {

template <typename T = uint64_t>
class TelemetryCounter
{
  public:
    static_assert(std::is_integral_v<T>, "TelemetryCounter<T> requires an integral T");

    TelemetryCounter() noexcept = default;
    explicit TelemetryCounter(T initial) noexcept
        : value_{initial}
    {}

    TelemetryCounter(TelemetryCounter const&) = delete;
    TelemetryCounter(TelemetryCounter&&) = delete;
    TelemetryCounter& operator=(TelemetryCounter const&) = delete;
    TelemetryCounter& operator=(TelemetryCounter&&) = delete;

    /// Producer side. Add `n` to the counter.
    void add(T n = 1) noexcept { value_.fetch_add(n, std::memory_order_release); }

    /// Reader side. Sample the current count.
    [[nodiscard]] auto load() const noexcept -> T { return value_.load(std::memory_order_acquire); }

    /// Reset to zero. Intended for the producer at start-of-run, not
    /// for concurrent use against a live reader.
    void reset() noexcept { value_.store(T{0}, std::memory_order_release); }

  private:
    std::atomic<T> value_{0};
};

}  // namespace statusbar::itc
