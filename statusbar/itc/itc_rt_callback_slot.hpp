#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// A control thread installs a (function pointer, context pointer)
/// pair; an RT callback thread loads the pair on every invocation.
/// The two pointers must always be observed as a coherent pair — a
/// torn read (new fn with stale ctx) would call a callback with the
/// wrong context. RtCallbackSlot wraps itc::AtomicTripleBuffer
/// so the pair is published and consumed as one atomic unit.
///
/// Strictly single-producer (the control thread: publish/clear) and
/// single-consumer (the RT thread: load).

#include "statusbar/itc/itc_atomic_triple_buffer.hpp"

#include <type_traits>

namespace statusbar::itc {

template <typename Fn>
class RtCallbackSlot
{
  public:
    static_assert(
        std::is_pointer_v<Fn> && std::is_function_v<std::remove_pointer_t<Fn>>,
        "RtCallbackSlot<Fn> expects a function-pointer type");

    /// The coherent callback pair. fn == nullptr means "no callback".
    struct Callback
    {
        Fn fn{nullptr};
        void* ctx{nullptr};
    };

    RtCallbackSlot() noexcept = default;

    RtCallbackSlot(RtCallbackSlot const&) = delete;
    RtCallbackSlot(RtCallbackSlot&&) = delete;
    RtCallbackSlot& operator=(RtCallbackSlot const&) = delete;
    RtCallbackSlot& operator=(RtCallbackSlot&&) = delete;

    /// Control thread. Install a new callback pair.
    void publish(Fn fn, void* ctx) noexcept { buffer_.publish(Callback{fn, ctx}); }

    /// Control thread. Detach the callback (publishes a null pair).
    void clear() noexcept { buffer_.publish(Callback{}); }

    /// RT thread (sole consumer). Returns the most recently published
    /// pair; before any publish() it returns a null Callback. Non-const
    /// because the underlying triple-buffer consume() advances state.
    [[nodiscard]] auto load() noexcept -> Callback { return buffer_.consume(); }

  private:
    itc::AtomicTripleBuffer<Callback> buffer_{};
};

}  // namespace statusbar::itc
