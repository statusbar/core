// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/stats/stats_atomic_wake_stats.hpp"

namespace statusbar::stats {

AtomicWakeStatsBase::AtomicWakeStatsBase() noexcept
    : skipped_counts_(0)
{}

auto AtomicWakeStatsBase::reset_skipped() noexcept -> void
{
    skipped_counts_.store(0, std::memory_order_relaxed);
}

auto AtomicWakeStatsBase::add_skipped(int64_t counts) noexcept -> void
{
    skipped_counts_.fetch_add(counts, std::memory_order_relaxed);
}

auto AtomicWakeStatsBase::load_skipped() const noexcept -> int64_t
{
    return skipped_counts_.load(std::memory_order_relaxed);
}

}  // namespace statusbar::stats
