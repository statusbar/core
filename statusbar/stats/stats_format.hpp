#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Formatting utilities for descriptive statistics reports.

#include "statusbar/stats/stats_descriptive.hpp"

#include <string>
#include <string_view>

namespace statusbar::stats {

/// Format duration in nanoseconds to human-readable string
[[nodiscard]] auto format_duration(double nanoseconds) -> std::string;

/// Format nanoseconds as a bracketed timestamp: "[  X.XXX]" (seconds.milliseconds)
[[nodiscard]] auto format_timestamp_ns(int64_t nanoseconds) -> std::string;

/// Print statistics report to stdout
void report(std::string_view name, DescriptiveStats const& stats);

}  // namespace statusbar::stats
