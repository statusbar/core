#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/args/args.hpp"
#include "statusbar/itc/itc_atomic_triple_buffer.hpp"
#include "statusbar/itc/itc_stop_token.hpp"
#include "statusbar/realtime/realtime_base.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <sys/stat.h>
#include <sys/types.h>

namespace statusbar::realtime {

/// Exception thrown when tripwire fires on wake latency check.
/// Timer thread catches this to abort the callback early.
struct TripwireFiredException
{};

/// One coherent tripwire-fire event record. Published as a single
/// unit through an AtomicTripleBuffer so a monitor thread can never
/// observe a half-written event (the old design stored six fields as
/// six separate atomics, leaving a torn-read window after the fired_
/// latch was set but before the payload stores landed).
struct TripwireEvent
{
    uint64_t tick{0};               ///< Wake/tick number that triggered the fire
    int64_t late_ns{0};             ///< Wake latency overshoot in nanoseconds
    int64_t duration_ns{0};         ///< Callback duration in nanoseconds (duration trips)
    int64_t now_ns{0};              ///< Actual wake time in nanoseconds
    int64_t sched_ns{0};            ///< Scheduled wake time in nanoseconds
    bool fired_on_duration{false};  ///< True if the trip was a callback-duration overrun
};

/// Non-template base class for Tripwire
/// Holds the fire latch + event channel with implementations in .cpp
/// to avoid module inlining issues
class TripwireBase
{
  public:
    TripwireBase() noexcept;
    ~TripwireBase() = default;

    // Non-copyable due to atomic latch + triple-buffer channel
    TripwireBase(TripwireBase const&) = delete;
    auto operator=(TripwireBase const&) -> TripwireBase& = delete;
    TripwireBase(TripwireBase&&) = delete;
    auto operator=(TripwireBase&&) -> TripwireBase& = delete;

  protected:
    // Implementations in .cpp to prevent inlining
    auto trigger_atomic() noexcept -> void;
    [[nodiscard]] auto has_fired_atomic() const noexcept -> bool;
    [[nodiscard]] auto was_duration_trip_atomic() const noexcept -> bool;
    auto reset_atomics() noexcept -> void;

    /// Try to fire - returns true if not already fired and sets fired to true
    [[nodiscard]] auto try_fire() noexcept -> bool;

    /// Store wake latency data after successful fire
    /// @param late How late the wake was in nanoseconds
    /// @param actual Actual wake time in nanoseconds
    /// @param scheduled Scheduled wake time in nanoseconds
    /// @param tick_val The tick/wake count that triggered the fire
    auto store_wake_data(int64_t late, int64_t actual, int64_t scheduled, uint64_t tick_val) noexcept -> void;

    /// Store duration data after successful fire
    /// @param duration Callback duration in nanoseconds that exceeded threshold
    /// @param tick_val The tick/wake count that triggered the fire
    auto store_duration_data(int64_t duration, uint64_t tick_val) noexcept -> void;

    /// Producer-side accessors. Read the last event recorded by this
    /// thread's own store_*; safe to call only from the thread that
    /// calls observe()/trigger() (the RT thread).
    [[nodiscard]] auto load_tick() const noexcept -> uint64_t;
    [[nodiscard]] auto load_late_ns() const noexcept -> int64_t;
    [[nodiscard]] auto load_duration_ns() const noexcept -> int64_t;
    [[nodiscard]] auto load_now_ns() const noexcept -> int64_t;
    [[nodiscard]] auto load_sched_ns() const noexcept -> int64_t;

    /// Consumer side (monitor thread): true once a fire event has been
    /// published since the last consume_event().
    [[nodiscard]] auto event_ready() const noexcept -> bool;

    /// Consumer side (monitor thread): take a coherent snapshot of the
    /// most recently published fire event.
    [[nodiscard]] auto consume_event() noexcept -> TripwireEvent;

  private:
    /// First-wins fire latch. A single named flag (try_fire/has_fired/
    /// reset); read only on the producer thread after migration.
    std::atomic<bool> fired_{false};

    /// RT -> monitor channel: the coherent event snapshot.
    itc::AtomicTripleBuffer<TripwireEvent> event_buffer_{};

    /// Producer-thread-private copy of the last event, so the
    /// producer's own accessors do not have to consume the SPSC
    /// triple buffer (whose sole consumer is the monitor thread).
    TripwireEvent last_event_{};
};

/// Non-template base class for TripwireMonitor
/// Holds atomic running state with implementation in .cpp to avoid module inlining issues
class TripwireMonitorBase
{
  public:
    TripwireMonitorBase() noexcept;
    ~TripwireMonitorBase() = default;

    // Non-copyable due to atomic member
    TripwireMonitorBase(TripwireMonitorBase const&) = delete;
    auto operator=(TripwireMonitorBase const&) -> TripwireMonitorBase& = delete;
    TripwireMonitorBase(TripwireMonitorBase&&) = delete;
    auto operator=(TripwireMonitorBase&&) -> TripwireMonitorBase& = delete;

  protected:
    // Atomic operations - implementations in .cpp to prevent inlining
    [[nodiscard]] auto is_running_atomic() const noexcept -> bool;
    auto set_running(bool value) noexcept -> void;

  private:
    std::atomic<bool> running_{false};
};

// Public config
struct TraceConfig
{
    // Tracer for wake latency trips (e.g. "wakeup_rt", "preemptirqsoff")
    std::string tracer{"wakeup_rt"};

    // Tracer for duration trips (e.g. "function_graph", "function")
    std::string duration_tracer{"function_graph"};

    // CPU to trace (your RT CPU). For CPU3, cpu = 3.
    int cpu{3};

    // ftrace buffer size
    int buffer_kb{32768};

    // where to dump artifacts
    std::string out_dir{"/tmp"};

    // tracefs paths
    std::string tr_root{"/sys/kernel/tracing"};

    // tripwire threshold for wake latency in nanoseconds
    int64_t tripwire_threshold_ns{50'000};

    // tripwire threshold for callback duration in nanoseconds (0 = disabled)
    int64_t tripwire_duration_threshold_ns{0};

    // Enable ftrace tracing (set to false for zero-overhead tripwire monitoring)
    // When false, tripwire still fires on threshold violations but no trace is captured
    bool enable_tracing{true};
};

/// Build argument specs for a TraceConfig
/// Returns a standalone ArgumentSpecs that can be merged with a prefix
/// @param config TraceConfig to bind argument specs to
auto build_trace_arg_specs(TraceConfig& config) -> statusbar::args::ArgumentSpecs;

// TraceController
class TraceController
{
  public:
    explicit TraceController(TraceConfig cfg)
        : cfg_(std::move(cfg))
    {}

    // Configure + start tracing. Also writes baseline proc snapshots to out_dir.
    // Returns false if writes fail (permissions, missing tracefs, etc).
    // If enable_tracing is false, skips ftrace setup for zero-overhead operation.
    auto start() -> bool;

    /// Check if the controller has been started
    [[nodiscard]] auto is_started() const noexcept -> bool { return started_; }

    /// Check if tracing is enabled
    [[nodiscard]] auto is_tracing_enabled() const noexcept -> bool { return tracing_enabled_; }

    /// Result of a capture operation
    struct CaptureResult
    {
        bool success{false};           ///< True if trace data was captured and saved
        bool tracing_disabled{false};  ///< True if tracing was disabled (intentionally skipped)
        std::string output_file;       ///< Path to the output file (if success)
    };

    // Capture trace + proc snapshots. Call from non-RT context.
    // If tracing is disabled, only logs the reason without capturing trace data.
    // Returns CaptureResult indicating success/failure and output file path.
    auto capture(std::string const& reason) const -> CaptureResult;

  private:
    TraceConfig cfg_;
    bool snapshot_supported_{false};
    bool started_{false};
    bool tracing_enabled_{false};
};

// Tripwire: RT-thread friendly
struct Tripwire : public TripwireBase
{
    // Threshold for wake latency (default 30 us = 30000 ns)
    int64_t threshold_ns{30000};

    // Threshold for callback duration (0 = disabled)
    int64_t duration_threshold_ns{0};

    /// Manually trigger the tripwire
    auto trigger() noexcept -> void { trigger_atomic(); }

    /// Check if the tripwire has fired
    [[nodiscard]] auto has_fired() const noexcept -> bool { return has_fired_atomic(); }

    /// Check if tripwire fired due to duration (vs wake latency)
    [[nodiscard]] auto was_duration_trip() const noexcept -> bool { return was_duration_trip_atomic(); }

    /// Reset the tripwire to unfired state
    auto reset() noexcept -> void { reset_atomics(); }

    /// Get the tick value that triggered the tripwire
    [[nodiscard]] auto tick() const noexcept -> uint64_t { return load_tick(); }

    /// Get the late_ns value
    [[nodiscard]] auto late_ns() const noexcept -> int64_t { return load_late_ns(); }

    /// Get the duration_ns value
    [[nodiscard]] auto duration_ns() const noexcept -> int64_t { return load_duration_ns(); }

    /// Get the now_ns value
    [[nodiscard]] auto now_ns() const noexcept -> int64_t { return load_now_ns(); }

    /// Get the sched_ns value
    [[nodiscard]] auto sched_ns() const noexcept -> int64_t { return load_sched_ns(); }

    /// Monitor-thread side: true once a fire event is ready to consume.
    [[nodiscard]] auto event_ready() const noexcept -> bool { return TripwireBase::event_ready(); }

    /// Monitor-thread side: take a coherent snapshot of the fire event.
    [[nodiscard]] auto consume_event() noexcept -> TripwireEvent { return TripwireBase::consume_event(); }

    /// Observe wake latency - call at the moment you start work for a tick
    /// @param tick_index The wake/tick number
    /// @param scheduled_time_ns The scheduled wake time in nanoseconds
    /// @param actual_time_ns The actual wake time in nanoseconds
    auto observe(uint64_t tick_index, int64_t scheduled_time_ns, int64_t actual_time_ns) noexcept -> void
    {
        int64_t const late = actual_time_ns - scheduled_time_ns;

        if (late <= threshold_ns) {
            return;
        }

        if (!try_fire()) {
            return;
        }

        store_wake_data(late, actual_time_ns, scheduled_time_ns, tick_index);
    }

    /// Observe callback duration - call after callback completes
    /// @param tick_index The wake/tick number
    /// @param duration The callback duration in nanoseconds
    auto observe_duration(uint64_t tick_index, int64_t duration) noexcept -> void
    {
        if (duration_threshold_ns <= 0) {
            return;  // Duration check disabled
        }

        if (duration <= duration_threshold_ns) {
            return;
        }

        if (!try_fire()) {
            return;
        }

        store_duration_data(duration, tick_index);
    }
};

// Helper: run on housekeeping CPU(s)
/// Monitor a tripwire and capture trace when it fires
/// Returns a std::thread that monitors the tripwire. Client should join or detach.
///
/// @param controller TraceController to use for capturing traces
/// @param tripwire Tripwire to monitor
/// @param shutdown_token Optional stop token to check for shutdown (nullptr to ignore)
/// @param running_flag Optional local running flag for direct stop control (nullptr to ignore)
/// @param poll_interval_us Polling interval in microseconds (default 10ms)
/// @return std::thread that will run until tripwire fires or shutdown/stop requested
auto monitor_tripwire(
    TraceController& controller,
    Tripwire& tripwire,
    statusbar::itc::StopToken* shutdown_token = nullptr,
    std::atomic<bool>* running_flag = nullptr,
    int poll_interval_us = 10'000) -> std::thread;

// TripwireMonitor: combines TraceController + Tripwire + monitor thread
/// Convenience class that manages a TraceController, Tripwire, and monitor thread together.
/// Handles startup logging and clean shutdown.
class TripwireMonitor : public TripwireMonitorBase
{
  public:
    /// Construct a TripwireMonitor with the given config
    /// @param config TraceConfig for the controller and tripwire threshold
    explicit TripwireMonitor(TraceConfig config)
        : config_{std::move(config)}
        , controller_{config_}
        , tripwire_{.threshold_ns = config_.tripwire_threshold_ns, .duration_threshold_ns = config_.tripwire_duration_threshold_ns}
    {}

    ~TripwireMonitor() noexcept { stop(); }

    // Non-copyable and non-movable
    TripwireMonitor(TripwireMonitor const&) = delete;
    auto operator=(TripwireMonitor const&) -> TripwireMonitor& = delete;
    TripwireMonitor(TripwireMonitor&&) = delete;
    auto operator=(TripwireMonitor&&) -> TripwireMonitor& = delete;

    /// Start the trace controller and monitor thread
    /// @param shutdown_token Optional stop token to check for shutdown (also used by check_and_handle_fired)
    /// @return true if trace controller started successfully
    auto start(statusbar::itc::StopToken* shutdown_token = nullptr) -> bool
    {
        shutdown_token_ = shutdown_token;
        set_running(true);
        running_flag_.store(true, std::memory_order_release);

        bool const ok = controller_.start();
        if (!ok) {
            std::print(stderr, "Warning: Failed to start trace controller (may need root)\n");
        }

        monitor_thread_ = monitor_tripwire(controller_, tripwire_, shutdown_token, &running_flag_);
        return ok;
    }

    /// Stop the monitor thread (blocks until thread exits)
    auto stop() noexcept -> void
    {
        set_running(false);
        running_flag_.store(false, std::memory_order_release);  // Also signal monitor thread
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }

    /// Check if the monitor is running
    [[nodiscard]] auto is_running() const noexcept -> bool { return is_running_atomic(); }

    /// Get the tripwire for observing latency
    [[nodiscard]] auto tripwire() noexcept -> Tripwire& { return tripwire_; }
    [[nodiscard]] auto tripwire() const noexcept -> Tripwire const& { return tripwire_; }

    /// Get the trace controller
    [[nodiscard]] auto controller() noexcept -> TraceController& { return controller_; }
    [[nodiscard]] auto controller() const noexcept -> TraceController const& { return controller_; }

    /// Get the config
    [[nodiscard]] auto config() const noexcept -> TraceConfig const& { return config_; }

    /// Check if tripwire fired and handle it by printing a message and requesting shutdown.
    /// Call this from the timer callback after observing the tripwire.
    /// Uses the shutdown_token passed to start().
    /// @param wake_count The current wake count
    /// @param error_ns The wake error in nanoseconds
    /// @param duration_ns The callback duration in nanoseconds (optional, for duration trip reporting)
    /// @return true if tripwire fired and shutdown was requested
    auto check_and_handle_fired(int64_t wake_count, int64_t error_ns, int64_t duration_ns = 0) -> bool
    {
        if (!tripwire_.has_fired()) {
            return false;
        }
        if (tripwire_.was_duration_trip()) {
            std::print(
                "\n*** TRIPWIRE FIRED at wake {} (callback duration {} us exceeded threshold) ***\n",
                wake_count,
                static_cast<double>(duration_ns) / 1000.0);
        } else {
            std::print("\n*** TRIPWIRE FIRED at wake {} (late by {} us) ***\n", wake_count, static_cast<double>(error_ns) / 1000.0);
        }
        if (shutdown_token_ != nullptr) {
            shutdown_token_->request_stop();
        }
        return true;
    }

  private:
    TraceConfig config_;
    TraceController controller_;
    Tripwire tripwire_;
    std::thread monitor_thread_;
    statusbar::itc::StopToken* shutdown_token_{nullptr};
    std::atomic<bool> running_flag_{false};  ///< Local running flag for monitor thread to poll
};

/// RAII wrapper that starts TripwireMonitor on construction
struct TripwireMonitorStart
{
    explicit TripwireMonitorStart(TripwireMonitor& monitor, statusbar::itc::StopToken* shutdown_token = nullptr)
        : monitor_{monitor}
    {
        monitor_.start(shutdown_token);
    }

  private:
    TripwireMonitor& monitor_;
};

}  // namespace statusbar::realtime
