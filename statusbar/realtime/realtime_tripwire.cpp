// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Implementation of TripwireBase and TripwireMonitorBase
/// Separated from module to prevent atomic operations from being inlined across module boundaries

#include "statusbar/realtime/realtime_tripwire.hpp"

#include "statusbar/realtime/realtime.hpp"

#include <atomic>
#include <cstdint>

namespace statusbar::realtime {

//
// TripwireBase implementation
//

TripwireBase::TripwireBase() noexcept = default;

auto TripwireBase::trigger_atomic() noexcept -> void
{
    // Manual force-fire. First-wins like observe(): publish a default
    // event so a monitor polling event_ready() still wakes.
    if (try_fire()) {
        last_event_ = TripwireEvent{};
        event_buffer_.publish(last_event_);
    }
}

auto TripwireBase::has_fired_atomic() const noexcept -> bool
{
    return fired_.load(std::memory_order_acquire);
}

auto TripwireBase::was_duration_trip_atomic() const noexcept -> bool
{
    return last_event_.fired_on_duration;
}

auto TripwireBase::reset_atomics() noexcept -> void
{
    // Single-threaded reuse only: callers must not reset() while a
    // monitor thread is consuming. The triple buffer needs no reset —
    // the next fire's publish() supersedes any stale event.
    fired_.store(false, std::memory_order_release);
    last_event_ = TripwireEvent{};
}

auto TripwireBase::try_fire() noexcept -> bool
{
    bool expected = false;
    return fired_.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
}

auto TripwireBase::store_wake_data(int64_t late, int64_t actual, int64_t scheduled, uint64_t tick_val) noexcept -> void
{
    last_event_ = TripwireEvent{
        .tick = tick_val,
        .late_ns = late,
        .duration_ns = 0,
        .now_ns = actual,
        .sched_ns = scheduled,
        .fired_on_duration = false,
    };
    event_buffer_.publish(last_event_);
}

auto TripwireBase::store_duration_data(int64_t duration, uint64_t tick_val) noexcept -> void
{
    last_event_ = TripwireEvent{
        .tick = tick_val,
        .late_ns = 0,
        .duration_ns = duration,
        .now_ns = 0,
        .sched_ns = 0,
        .fired_on_duration = true,
    };
    event_buffer_.publish(last_event_);
}

auto TripwireBase::load_tick() const noexcept -> uint64_t
{
    return last_event_.tick;
}

auto TripwireBase::load_late_ns() const noexcept -> int64_t
{
    return last_event_.late_ns;
}

auto TripwireBase::load_duration_ns() const noexcept -> int64_t
{
    return last_event_.duration_ns;
}

auto TripwireBase::load_now_ns() const noexcept -> int64_t
{
    return last_event_.now_ns;
}

auto TripwireBase::load_sched_ns() const noexcept -> int64_t
{
    return last_event_.sched_ns;
}

auto TripwireBase::event_ready() const noexcept -> bool
{
    return event_buffer_.can_consume();
}

auto TripwireBase::consume_event() noexcept -> TripwireEvent
{
    return event_buffer_.consume();
}

//
// TripwireMonitorBase implementation
//

TripwireMonitorBase::TripwireMonitorBase() noexcept
    : running_(false)
{}

auto TripwireMonitorBase::is_running_atomic() const noexcept -> bool
{
    return running_.load(std::memory_order_acquire);
}

auto TripwireMonitorBase::set_running(bool value) noexcept -> void
{
    running_.store(value, std::memory_order_release);
}

}  // namespace statusbar::realtime
