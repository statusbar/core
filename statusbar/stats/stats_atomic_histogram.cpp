// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/stats/stats_atomic_histogram.hpp"

namespace statusbar::stats {

AtomicHistogramBase::AtomicHistogramBase() noexcept
{
    reset_atomics();
}

auto AtomicHistogramBase::reset_atomics() noexcept -> void
{
    underflow_.store(0, std::memory_order_relaxed);
    overflow_.store(0, std::memory_order_relaxed);
    for (auto& bin : bins_) {
        bin.store(0, std::memory_order_relaxed);
    }
}

auto AtomicHistogramBase::increment_underflow() noexcept -> void
{
    underflow_.fetch_add(1, std::memory_order_relaxed);
}

auto AtomicHistogramBase::increment_overflow() noexcept -> void
{
    overflow_.fetch_add(1, std::memory_order_relaxed);
}

auto AtomicHistogramBase::increment_bin(size_t index) noexcept -> void
{
    if (index < max_bins) {
        bins_[index].fetch_add(1, std::memory_order_relaxed);
    }
}

auto AtomicHistogramBase::load_underflow() const noexcept -> int64_t
{
    return underflow_.load(std::memory_order_relaxed);
}

auto AtomicHistogramBase::load_overflow() const noexcept -> int64_t
{
    return overflow_.load(std::memory_order_relaxed);
}

auto AtomicHistogramBase::load_bin(size_t index) const noexcept -> int64_t
{
    if (index < max_bins) {
        return bins_[index].load(std::memory_order_relaxed);
    }
    return 0;
}

}  // namespace statusbar::stats
