#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Wait-free SPSC triple buffer for publishing the latest complete
/// snapshot of a trivially-copyable POD `T` from one thread to
/// another. See itc_atomic_triple_buffer_base.hpp for the
/// protocol details — this template is just typed slot storage on
/// top of that base.
///
/// Usage:
///
///   AtomicTripleBuffer<RateOffset> buf;
///   // ... publisher thread:
///   buf.publish(RateOffset{rate, offset_ns});
///   // ... consumer thread:
///   if (buf.can_consume()) {
///       auto const snap = buf.consume();
///       // use snap.rate, snap.offset_ns
///   }

#include "statusbar/itc/itc_atomic_triple_buffer_base.hpp"

#include <array>
#include <type_traits>

namespace statusbar::itc {

template <typename T>
class AtomicTripleBuffer : private AtomicTripleBufferBase
{
  public:
    static_assert(std::is_trivially_copyable_v<T>, "AtomicTripleBuffer<T> requires trivially copyable T");
    static_assert(std::is_default_constructible_v<T>, "AtomicTripleBuffer<T> requires default constructible T");

    AtomicTripleBuffer() noexcept = default;

    /// Initial-value constructor: every slot is initialized to
    /// `initial` and the buffer is marked dirty so the consumer sees
    /// exactly one fresh value before any explicit publish().
    explicit AtomicTripleBuffer(T const& initial) noexcept
        : AtomicTripleBufferBase{InitiallyDirty{}}
    {
        slots_[0] = initial;
        slots_[1] = initial;
        slots_[2] = initial;
    }

    AtomicTripleBuffer(AtomicTripleBuffer const&) = delete;
    AtomicTripleBuffer(AtomicTripleBuffer&&) = delete;
    AtomicTripleBuffer& operator=(AtomicTripleBuffer const&) = delete;
    AtomicTripleBuffer& operator=(AtomicTripleBuffer&&) = delete;

    /// Publisher (single writer thread). Wait-free.
    void publish(T const& value) noexcept
    {
        slots_[writer_slot()] = value;
        commit_publish();
    }

    /// Consumer (single reader thread). True if a publish has
    /// happened since the last consume(). Wait-free.
    using AtomicTripleBufferBase::can_consume;

    /// Consumer (single reader thread). Returns a snapshot of the
    /// most recently published value. If can_consume() was true,
    /// advances to that fresh value; otherwise returns the previously
    /// consumed value. Wait-free.
    [[nodiscard]] auto consume() noexcept -> T { return slots_[acquire_read_slot()]; }

    /// Diagnostic: publishes that overwrote a still-unconsumed prior
    /// publish.
    using AtomicTripleBufferBase::overruns;

  private:
    std::array<T, 3> slots_{};
};

}  // namespace statusbar::itc
