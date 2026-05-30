#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Non-template state-machine layer of AtomicTripleBuffer<T>. Owns the
/// wait-free SPSC protocol: a 3-bit atomic `state_` (2 bits ready
/// index, 1 bit dirty flag) plus private back/front indices and an
/// overrun counter. The atomic operations are implemented in
/// itc_atomic_triple_buffer_base.cpp so they compile exactly
/// once into libstatusbar-itc regardless of how many T's the
/// template wrapper is instantiated with.
///
/// Strictly single-producer / single-consumer: one thread calls the
/// publisher API (writer_slot + commit_publish), one thread calls the
/// consumer API (can_consume + acquire_read_slot). Multiple readers
/// would race on the front-index swap and the dirty-bit clear.

#include <atomic>
#include <cstdint>

namespace statusbar::itc {

class AtomicTripleBufferBase
{
  public:
    /// Default: nothing published yet; can_consume() returns false.
    AtomicTripleBufferBase() noexcept;

    /// Tag passed to the alternate constructor to mark state dirty at
    /// construction time. The wrapper template's initial-value
    /// constructor uses this so consumers see exactly one fresh value
    /// before any explicit publish() call.
    struct InitiallyDirty
    {};
    explicit AtomicTripleBufferBase(InitiallyDirty) noexcept;

    AtomicTripleBufferBase(AtomicTripleBufferBase const&) = delete;
    AtomicTripleBufferBase(AtomicTripleBufferBase&&) = delete;
    AtomicTripleBufferBase& operator=(AtomicTripleBufferBase const&) = delete;
    AtomicTripleBufferBase& operator=(AtomicTripleBufferBase&&) = delete;

    // ---- Publisher (single writer thread) ----

    /// Slot index the publisher should write its next T into. Stable
    /// across multiple reads until commit_publish() rotates it.
    [[nodiscard]] auto writer_slot() const noexcept -> uint8_t;

    /// Atomic exchange that promotes the writer's slot to "ready" and
    /// pulls a fresh writable slot in. If the previous publish hadn't
    /// been consumed yet, increments overruns_.
    void commit_publish() noexcept;

    // ---- Consumer (single reader thread) ----

    /// True if a publish has happened since the last consume.
    [[nodiscard]] auto can_consume() const noexcept -> bool;

    /// Slot index the reader should read from. If state was dirty,
    /// atomically swaps reader's front with ready and clears the dirty
    /// bit; otherwise returns the previously-acquired front unchanged.
    [[nodiscard]] auto acquire_read_slot() noexcept -> uint8_t;

    // ---- Diagnostics ----

    /// Currently-published slot index (the value the next consume
    /// would advance to). Exposed for tests that verify the
    /// three-distinct-indices invariant and for higher-level
    /// reporting; not needed for normal publish/consume operation.
    [[nodiscard]] auto ready_slot() const noexcept -> uint8_t;

    /// Total publishes that overwrote a still-unconsumed prior
    /// publish. Read from any thread is safe (single relaxed load).
    [[nodiscard]] auto overruns() const noexcept -> uint64_t;

  private:
    // Bit layout of state_: bits 0-1 = ready index (0..2), bit 2 =
    // dirty flag (1 = fresh publish waiting). Higher bits unused.
    static constexpr uint8_t slot_mask = 0x3U;
    static constexpr uint8_t dirty_bit = 0x4U;

    std::atomic<uint8_t> state_;
    std::atomic<uint64_t> overruns_;
    uint8_t back_;
    uint8_t front_;
};

}  // namespace statusbar::itc
