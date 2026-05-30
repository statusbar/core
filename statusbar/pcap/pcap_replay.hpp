#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/ieee/ieee.hpp"
#include "statusbar/ieee/ieee_ethernet.hpp"
#include "statusbar/pcap/pcap_base.hpp"
#include "statusbar/pcap/pcap_error.hpp"
#include "statusbar/pcap/pcapng_reader.hpp"
#include "statusbar/status/status.hpp"

#include <chrono>
#include <concepts>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

namespace statusbar::pcap {

/// Steady-clock time point used to express synthetic replay time.
/// Matches statusbar::sm::TimePoint so state machines can consume it directly
/// without depending on the sm module from here.
using ReplayTime = std::chrono::steady_clock::time_point;

/// One delivered packet plus its synthetic timestamp. `payload` is a view
/// into the driver's internal storage; it is invalidated by the next call
/// to `PcapReplayDriver::next()`.
///
/// `frame` is the full `ieee::EthernetFrame` — including destination MAC,
/// source MAC, optional 802.1Q VLAN tag, and the ethertype of the inner
/// protocol (so callers can compare against the protocol they expect
/// without caring whether the frame was tagged). For AVB/TSN work, check
/// `frame.vlan_tag.is_set()` / `get_vid()` / `get_pcp()` — streams must
/// land on the right VLAN and priority. `payload` is the bytes after the
/// (inner) ethertype.
struct ReplayEvent
{
    ReplayTime timestamp{};
    ieee::EthernetFrame frame{};
    std::span<uint8_t const> payload{};
};

/// Deterministic pcap(ng) replay with synthetic time.
///
/// Pull-based: the caller loops on `next()`, receiving every packet in
/// order with a synthetic timestamp. Between calls the caller advances
/// its own simulated clock (and any internal timers) up to the returned
/// `timestamp`, then does protocol-specific filtering and dispatch. The
/// driver does not schedule, does not filter, and does not write outputs;
/// those concerns stay with the protocol-specific test tool.
///
/// Synthetic time origin: the first packet in the capture is placed at
/// `ReplayTime{}` (epoch), with later packets offset by the pcap-relative
/// timestamps provided by `PcapngReader`.
class PcapReplayDriver
{
  public:
    struct Stats
    {
        size_t packets_total{0};  ///< packets read from the pcap
    };

    /// Open the input capture.
    /// @return Configured driver on success, or a PcapError if the underlying
    /// PcapngReader could not be opened.
    [[nodiscard]] static auto open(std::string const& input_path, std::pmr::memory_resource* memory_resource = nullptr)
        -> StatusValue<PcapReplayDriver>;

    PcapReplayDriver(PcapReplayDriver const&) = delete;
    auto operator=(PcapReplayDriver const&) -> PcapReplayDriver& = delete;
    PcapReplayDriver(PcapReplayDriver&&) noexcept = default;
    auto operator=(PcapReplayDriver&&) noexcept -> PcapReplayDriver& = default;
    ~PcapReplayDriver() = default;

    /// Read the next packet. On success populates `out` and returns
    /// success(true); returns success(false) at EOF; failure() on read error.
    [[nodiscard]] auto next(ReplayEvent& out) -> StatusValue<bool>;

    /// Invoke `on_event(ReplayEvent const&)` for every packet in the
    /// capture, in order, then return. The callback may return `void` to
    /// always continue, or `bool` to short-circuit (return false to stop).
    ///
    /// The callback receives a const reference whose `payload` aliases
    /// driver-owned storage that is invalidated on the next iteration —
    /// copy the bytes if you need to hold them.
    ///
    /// Returns success() on a clean EOF (or callback-stopped) walk, or
    /// failure(PcapError::...) if the underlying read fails.
    template <typename F>
    [[nodiscard]] auto for_each(F&& on_event) -> Status
    {
        auto cb = std::forward<F>(on_event);
        ReplayEvent evt;
        for (;;) {
            auto step = next(evt);
            if (!step) {
                return failure(step.error());
            }
            if (!*step) {
                return success();
            }
            if constexpr (std::is_same_v<std::invoke_result_t<decltype(cb)&, ReplayEvent const&>, bool>) {
                if (!cb(std::as_const(evt))) {
                    return success();
                }
            } else {
                cb(std::as_const(evt));
            }
        }
    }

    /// Timestamp of the most recently returned packet, or `ReplayTime{}`
    /// before any packet has been returned.
    [[nodiscard]] auto current_time() const noexcept -> ReplayTime { return current_time_; }

    /// Running counts; updated after each call to `next()`.
    [[nodiscard]] auto stats() const noexcept -> Stats const& { return stats_; }

  private:
    explicit PcapReplayDriver(PcapngReader reader) noexcept
        : reader_(std::move(reader))
    {}

    PcapngReader reader_;
    ReplayTime current_time_{};
    Stats stats_{};
    Packet payload_storage_;
};

}  // namespace statusbar::pcap
