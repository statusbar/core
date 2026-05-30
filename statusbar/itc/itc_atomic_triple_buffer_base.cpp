// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/itc/itc_atomic_triple_buffer_base.hpp"

namespace statusbar::itc {

AtomicTripleBufferBase::AtomicTripleBufferBase() noexcept
    : state_{static_cast<uint8_t>(1U)}  // ready=1, dirty=0
    , overruns_{0}
    , back_{0}
    , front_{2}
{}

AtomicTripleBufferBase::AtomicTripleBufferBase(InitiallyDirty) noexcept
    : state_{static_cast<uint8_t>(1U | dirty_bit)}  // ready=1, dirty=1
    , overruns_{0}
    , back_{0}
    , front_{2}
{}

auto AtomicTripleBufferBase::writer_slot() const noexcept -> uint8_t
{
    return back_;
}

void AtomicTripleBufferBase::commit_publish() noexcept
{
    // Single atomic exchange: writes new state (back | DIRTY) and
    // returns the prior state. Release ordering pairs with the
    // consumer's acquire on the same atomic, so the plain write to
    // slots[back_] sequenced-before this exchange is visible to the
    // consumer's subsequent read of the new ready slot.
    uint8_t const desired = static_cast<uint8_t>(back_ | dirty_bit);
    uint8_t const old = state_.exchange(desired, std::memory_order_release);
    if ((old & dirty_bit) != 0) {
        // The previous publish hadn't been consumed yet; this publish
        // overwrites the ready slot's logical contents (technically the
        // ready slot becomes the new back and gets written next).
        overruns_.fetch_add(1, std::memory_order_relaxed);
    }
    back_ = static_cast<uint8_t>(old & slot_mask);  // old ready becomes new back
}

auto AtomicTripleBufferBase::can_consume() const noexcept -> bool
{
    return (state_.load(std::memory_order_acquire) & dirty_bit) != 0;
}

auto AtomicTripleBufferBase::acquire_read_slot() noexcept -> uint8_t
{
    // Fast path: no fresh data since last consume, return same slot.
    if ((state_.load(std::memory_order_acquire) & dirty_bit) == 0) {
        return front_;
    }
    // Atomic exchange: write new state (front_, dirty cleared) and
    // pick up the slot index that was ready. Acquire ordering pairs
    // with the publisher's release on the same atomic, so the
    // publisher's plain write to slots[old_ready] is visible to the
    // consumer's subsequent read.
    uint8_t const old = state_.exchange(front_, std::memory_order_acquire);
    front_ = static_cast<uint8_t>(old & slot_mask);
    return front_;
}

auto AtomicTripleBufferBase::ready_slot() const noexcept -> uint8_t
{
    return static_cast<uint8_t>(state_.load(std::memory_order_relaxed) & slot_mask);
}

auto AtomicTripleBufferBase::overruns() const noexcept -> uint64_t
{
    return overruns_.load(std::memory_order_relaxed);
}

}  // namespace statusbar::itc
