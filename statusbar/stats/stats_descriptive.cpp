// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/stats/stats_descriptive.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace statusbar::stats {

auto percentile(std::span<double const> sorted, double p) -> double
{
    if (sorted.empty()) {
        return 0.0;
    }
    auto const idx = static_cast<size_t>(static_cast<double>(sorted.size()) * p);
    return sorted[std::min(idx, sorted.size() - 1)];
}

auto analyze(std::vector<double> measurements) -> DescriptiveStats
{
    if (measurements.empty()) {
        return DescriptiveStats{};
    }

    DescriptiveStats stats;
    stats.sample_count = measurements.size();

    // Sort for median and percentiles
    std::ranges::sort(measurements);

    // Min and Max
    stats.min = measurements.front();
    stats.max = measurements.back();

    // Mean
    double sum = 0.0;
    for (auto const val : measurements) {
        sum += val;
    }
    stats.mean = sum / static_cast<double>(measurements.size());

    // Standard deviation
    double variance_sum = 0.0;
    for (auto const val : measurements) {
        double const diff = val - stats.mean;
        variance_sum += diff * diff;
    }
    stats.stddev = std::sqrt(variance_sum / static_cast<double>(measurements.size()));

    // Median (50th percentile)
    size_t const mid = measurements.size() / 2;
    if (measurements.size() % 2 == 0) {
        stats.median = (measurements[mid - 1] + measurements[mid]) / 2.0;
    } else {
        stats.median = measurements[mid];
    }

    // 95th and 99th percentiles
    std::span<double const> const sorted{measurements};
    stats.p95 = percentile(sorted, 0.95);
    stats.p99 = percentile(sorted, 0.99);

    return stats;
}

}  // namespace statusbar::stats
