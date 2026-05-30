#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Thread-safe atomic histogram for lock-free live data collection.
/// Designed for realtime use — all operations are lock-free using atomic int64_t.
/// Uniform bins with configurable range, plus underflow/overflow tracking.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <limits>
#include <string>

namespace statusbar::stats {

/// Configuration for an atomic histogram.
struct AtomicHistogramConfig
{
    int64_t low_ns{-50'000};     ///< Low end of range (values below go to underflow bin)
    int64_t high_ns{50'000};     ///< High end of range (values above go to overflow bin)
    int64_t bin_width_ns{1000};  ///< Width of each bin

    /// Calculate number of bins (excluding underflow/overflow)
    [[nodiscard]] constexpr auto num_bins() const noexcept -> int64_t
    {
        if (bin_width_ns <= 0 || high_ns <= low_ns) {
            return 0;
        }
        return (high_ns - low_ns) / bin_width_ns;
    }

    /// Check if configuration is valid
    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
    {
        return bin_width_ns > 0 && high_ns > low_ns && num_bins() > 0;
    }
};

/// Non-template base class for AtomicHistogram.
/// Holds atomic state with implementations in .cpp to avoid module inlining issues.
class AtomicHistogramBase
{
  public:
    static constexpr size_t max_bins = 1024;

    AtomicHistogramBase() noexcept;
    ~AtomicHistogramBase() = default;

    AtomicHistogramBase(AtomicHistogramBase const&) = delete;
    auto operator=(AtomicHistogramBase const&) -> AtomicHistogramBase& = delete;
    AtomicHistogramBase(AtomicHistogramBase&&) = delete;
    auto operator=(AtomicHistogramBase&&) -> AtomicHistogramBase& = delete;

  protected:
    auto reset_atomics() noexcept -> void;
    auto increment_underflow() noexcept -> void;
    auto increment_overflow() noexcept -> void;
    auto increment_bin(size_t index) noexcept -> void;
    [[nodiscard]] auto load_underflow() const noexcept -> int64_t;
    [[nodiscard]] auto load_overflow() const noexcept -> int64_t;
    [[nodiscard]] auto load_bin(size_t index) const noexcept -> int64_t;

  private:
    std::atomic<int64_t> underflow_{0};
    std::atomic<int64_t> overflow_{0};
    std::array<std::atomic<int64_t>, max_bins> bins_{};
};

/// Thread-safe histogram with configurable range and bin width.
/// Uses atomic operations for lock-free updates from RT threads.
///
/// @tparam MaxBins Maximum number of bins (compile-time constant for fixed-size array)
template <size_t MaxBins = 128>
class AtomicHistogram : public AtomicHistogramBase
{
    static_assert(MaxBins <= max_bins, "MaxBins exceeds AtomicHistogramBase::max_bins");

  public:
    /// Snapshot of histogram data for consistent reading.
    struct Snapshot
    {
        AtomicHistogramConfig config;
        int64_t underflow = 0;
        int64_t overflow = 0;
        std::array<int64_t, MaxBins> bins{};
        size_t num_bins = 0;

        [[nodiscard]] auto total_count() const noexcept -> int64_t
        {
            int64_t total = underflow + overflow;
            for (size_t i = 0; i < num_bins; ++i) {
                total += bins[i];
            }
            return total;
        }

        [[nodiscard]] auto bin_low(size_t const bin_index) const noexcept -> int64_t
        {
            return config.low_ns + (static_cast<int64_t>(bin_index) * config.bin_width_ns);
        }

        [[nodiscard]] auto bin_high(size_t const bin_index) const noexcept -> int64_t
        {
            return bin_low(bin_index) + config.bin_width_ns;
        }

        /// Format histogram as ASCII bar chart.
        template <typename OutputIt>
        auto format_to(OutputIt out, int const bar_width = 40) const -> OutputIt
        {
            if (num_bins == 0) {
                return std::format_to(out, "  (histogram not configured)\n");
            }

            int64_t const total = total_count();
            if (total == 0) {
                return std::format_to(out, "  (no samples)\n");
            }

            int64_t max_count = underflow;
            max_count = std::max(max_count, overflow);
            for (size_t i = 0; i < num_bins; ++i) {
                max_count = std::max(max_count, bins[i]);
            }

            auto format_bar = [&](int64_t const count) -> std::string {
                if (max_count == 0) {
                    return std::string(bar_width, ' ');
                }
                int const filled = static_cast<int>((count * bar_width) / max_count);
                return std::string(filled, '#') + std::string(bar_width - filled, ' ');
            };

            auto format_percent = [&](int64_t const count) -> double {
                return (total > 0) ? (100.0 * static_cast<double>(count) / static_cast<double>(total)) : 0.0;
            };

            if (underflow > 0) {
                out = std::format_to(
                    out,
                    "  < {:+8} ns: |{}| {:6} ({:5.1f}%)\n",
                    config.low_ns,
                    format_bar(underflow),
                    underflow,
                    format_percent(underflow));
            }

            for (size_t i = 0; i < num_bins; ++i) {
                int64_t const low = bin_low(i);
                int64_t const count = bins[i];
                auto should_display_bin = [&](size_t idx, int64_t cnt) -> bool {
                    return cnt > 0 || (idx > 0 && bins[idx - 1] > 0) || (idx + 1 < num_bins && bins[idx + 1] > 0);
                };
                if (should_display_bin(i, count)) {
                    out = std::format_to(
                        out, "  {:+8} ns: |{}| {:6} ({:5.1f}%)\n", low, format_bar(count), count, format_percent(count));
                }
            }

            if (overflow > 0) {
                out = std::format_to(
                    out,
                    "  >= {:+7} ns: |{}| {:6} ({:5.1f}%)\n",
                    config.high_ns,
                    format_bar(overflow),
                    overflow,
                    format_percent(overflow));
            }

            return out;
        }
    };

    AtomicHistogram() noexcept = default;

    explicit AtomicHistogram(AtomicHistogramConfig config) noexcept
        : config_{config}
        , num_bins_{static_cast<size_t>(std::min(static_cast<int64_t>(MaxBins), config.num_bins()))}
    {
        reset();
    }

    void reset() noexcept { reset_atomics(); }

    void configure(AtomicHistogramConfig config) noexcept
    {
        config_ = config;
        // The fixed bin array is MaxBins long. We honor the requested
        // bin_width_ns exactly and narrow the configured range to
        // [low, low + MaxBins * bin_width) when the request would
        // otherwise need more bins than the storage can hold. Caller
        // gets the resolution they asked for; values beyond the
        // shrunken range route to the overflow bin. Snapshot's
        // (low_ns, high_ns, bin_width_ns) reflects the actual storage.
        num_bins_ = static_cast<size_t>(std::min(static_cast<int64_t>(MaxBins), config_.num_bins()));
        if (num_bins_ > 0) {
            config_.high_ns = config_.low_ns + static_cast<int64_t>(num_bins_) * config_.bin_width_ns;
        }
        reset();
    }

    void update(int64_t value_ns) noexcept
    {
        if (num_bins_ == 0) {
            return;
        }
        if (value_ns < config_.low_ns) {
            increment_underflow();
            return;
        }
        if (value_ns >= config_.high_ns) {
            increment_overflow();
            return;
        }
        size_t const bin_index = static_cast<size_t>((value_ns - config_.low_ns) / config_.bin_width_ns);
        if (bin_index < num_bins_) {
            increment_bin(bin_index);
            return;
        }
        // Defense in depth: configure() truncates config_.high_ns to
        // num_bins_*bin_width_ns so this branch is normally unreachable.
        // Route to overflow rather than silently dropping if bin_width
        // is ever mutated between configure calls.
        increment_overflow();
    }

    [[nodiscard]] auto config() const noexcept -> AtomicHistogramConfig const& { return config_; }
    [[nodiscard]] auto num_bins() const noexcept -> size_t { return num_bins_; }
    [[nodiscard]] auto is_configured() const noexcept -> bool { return num_bins_ > 0; }

    [[nodiscard]] auto snapshot() const noexcept -> Snapshot
    {
        Snapshot s;
        s.config = config_;
        s.underflow = load_underflow();
        s.overflow = load_overflow();
        s.num_bins = num_bins_;
        for (size_t i = 0; i < num_bins_; ++i) {
            s.bins[i] = load_bin(i);
        }
        return s;
    }

  private:
    AtomicHistogramConfig config_{};
    size_t num_bins_{0};
};

}  // namespace statusbar::stats
