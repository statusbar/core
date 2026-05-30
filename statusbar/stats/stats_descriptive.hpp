#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Descriptive statistics for post-hoc analysis of measurement datasets.

#include <cstddef>
#include <span>
#include <vector>

namespace statusbar::stats {

struct DescriptiveStats
{
    double mean{0.0};        ///< Average value
    double median{0.0};      ///< Median value (50th percentile)
    double stddev{0.0};      ///< Standard deviation
    double min{0.0};         ///< Minimum value
    double max{0.0};         ///< Maximum value
    double p95{0.0};         ///< 95th percentile
    double p99{0.0};         ///< 99th percentile
    size_t sample_count{0};  ///< Number of samples
};

/// Compute descriptive stats. The vector is sorted in place.
[[nodiscard]] auto analyze(std::vector<double> measurements) -> DescriptiveStats;

/// Compute a specific percentile from pre-sorted ascending data.
[[nodiscard]] auto percentile(std::span<double const> sorted, double p) -> double;

}  // namespace statusbar::stats
