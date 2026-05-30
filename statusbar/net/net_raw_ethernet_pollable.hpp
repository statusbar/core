#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// RawEthernetPollable — wraps a RawnetContext as a Pollable for MessageReactor.
/// Delivers complete parsed Ethernet frames via a callback.

#include "statusbar/ieee/ieee.hpp"
#include "statusbar/net/net_message_reactor.hpp"
#include "statusbar/net/net_rawnet.hpp"
#include "statusbar/sg14/inplace_function.h"

#include <cstdint>
#include <memory>
#include <span>

namespace statusbar::net {

/// Callback for received Ethernet frames.
/// Parameters: (now_ns, src_mac, payload_after_ethernet_header)
using RawFrameCallback = statusbar::sg14::inplace_function<void(int64_t, ieee::Eui48, std::span<uint8_t const>), 64>;

/// Wraps a RawnetContext and implements Pollable for use with MessageReactor.
///
/// On each on_ready, drains all available frames and calls the frame callback.
/// TX goes directly through send_avtp() — synchronous, no queuing.
class RawEthernetPollable : public Pollable
{
  public:
    /// Construct from an open RawnetContext and a frame callback.
    RawEthernetPollable(RawnetContext context, RawFrameCallback on_frame)
        : context_{std::move(context)}
        , on_frame_{std::move(on_frame)}
    {}

    /// Get the hardware (MAC) address of the bound interface.
    [[nodiscard]] auto hardware_address() const noexcept -> ieee::Eui48 { return context_.my_mac(); }

    /// Send an AVTP payload to the given destination MAC.
    auto send_avtp(ieee::Eui48 const& dest_mac, std::span<uint8_t const> payload) -> Status
    {
        auto result = context_.send(&dest_mac, payload);
        if (!result) {
            return forward_failure(result);
        }
        return success();
    }

    /// Mark as finished (will be removed from reactor).
    void close() noexcept { finished_ = true; }

    // -- Pollable interface --

    [[nodiscard]] auto fd() const noexcept -> int override { return context_.fd(); }

    void on_ready(int64_t now_ns) override
    {
        ieee::Eui48 src_mac{};
        ieee::Eui48 dest_mac{};

        while (true) {
            auto result = context_.recv(&src_mac, &dest_mac, payload_buf_);
            if (!result) {
                break;
            }
            if (*result <= 0) {
                break;
            }
            auto const payload_len = static_cast<size_t>(*result);
            if (on_frame_) {
                on_frame_(now_ns, src_mac, {payload_buf_.data(), payload_len});
            }
        }
    }

    void tick(int64_t /*now_ns*/) override {}

    [[nodiscard]] auto finished() const noexcept -> bool override { return finished_; }

  private:
    RawnetContext context_;
    RawFrameCallback on_frame_;
    std::array<uint8_t, 2048> payload_buf_{};
    bool finished_{false};
};

}  // namespace statusbar::net
