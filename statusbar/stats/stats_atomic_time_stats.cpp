// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/stats/stats_atomic_time_stats.hpp"

namespace statusbar::stats {

AtomicTimeStatsBase::AtomicTimeStatsBase() noexcept
{
    reset_atomics();
}

auto AtomicTimeStatsBase::reset_atomics() noexcept -> void
{
    count_.store(0, std::memory_order_relaxed);
    sum_ns_.store(0, std::memory_order_relaxed);
    sum_sq_.store(0, std::memory_order_relaxed);
    min_ns_.store(std::numeric_limits<int64_t>::max(), std::memory_order_relaxed);
    max_ns_.store(std::numeric_limits<int64_t>::min(), std::memory_order_relaxed);
}

auto AtomicTimeStatsBase::update_atomics(int64_t value_ns) noexcept -> void
{
    count_.fetch_add(1, std::memory_order_relaxed);
    sum_ns_.fetch_add(value_ns, std::memory_order_relaxed);
    sum_sq_.fetch_add(value_ns * value_ns, std::memory_order_relaxed);

    int64_t current_min = min_ns_.load(std::memory_order_relaxed);
    while (value_ns < current_min && !min_ns_.compare_exchange_weak(current_min, value_ns, std::memory_order_relaxed)) {
    }

    int64_t current_max = max_ns_.load(std::memory_order_relaxed);
    while (value_ns > current_max && !max_ns_.compare_exchange_weak(current_max, value_ns, std::memory_order_relaxed)) {
    }
}

auto AtomicTimeStatsBase::load_count() const noexcept -> int64_t
{
    return count_.load(std::memory_order_relaxed);
}
auto AtomicTimeStatsBase::load_sum_ns() const noexcept -> int64_t
{
    return sum_ns_.load(std::memory_order_relaxed);
}
auto AtomicTimeStatsBase::load_sum_sq() const noexcept -> int64_t
{
    return sum_sq_.load(std::memory_order_relaxed);
}
auto AtomicTimeStatsBase::load_min_ns() const noexcept -> int64_t
{
    return min_ns_.load(std::memory_order_relaxed);
}
auto AtomicTimeStatsBase::load_max_ns() const noexcept -> int64_t
{
    return max_ns_.load(std::memory_order_relaxed);
}

}  // namespace statusbar::stats
