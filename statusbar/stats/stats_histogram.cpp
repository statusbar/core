// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/stats/stats_histogram.hpp"

#include <array>

namespace statusbar::stats {

auto log_scale_us_buckets() -> std::span<Bucket<double> const>
{
    static constexpr std::array<Bucket<double>, 8> buckets = {{
        {.label = "    0-500us", .low = 0.0, .high = 500.0},
        {.label = "  500-1000us", .low = 500.0, .high = 1000.0},
        {.label = " 1000-2000us", .low = 1000.0, .high = 2000.0},
        {.label = " 2000-5000us", .low = 2000.0, .high = 5000.0},
        {.label = "   5-10ms", .low = 5000.0, .high = 10000.0},
        {.label = "  10-50ms", .low = 10000.0, .high = 50000.0},
        {.label = " 50-250ms", .low = 50000.0, .high = 250000.0},
        {.label = "   >250ms", .low = 250000.0, .high = 1e12},
    }};
    return buckets;
}

}  // namespace statusbar::stats
