#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// PresentationSlotMap — fixed-capacity time-keyed slot ring.
///
/// Each slot holds one (timestamp, payload) pair. The slot index for a
/// given time is `(time / slot_width) % Capacity`; the timestamp stored
/// in the slot is the slot's quantized boundary, so any query within
/// the same slot resolves to the same answer.
///
/// Stale-vs-live discrimination happens at READ time by comparing the
/// requested time's quantized boundary to the slot's stored timestamp.
/// There is no scan, retire, or sweep — slots simply hold the most
/// recent payload written to that index until overwritten. Callers walk
/// the ring at their own consumer cadence (typically a report timer or
/// a playout clock).
///
/// Single-threaded use only. The owning thread polls inputs and queries
/// the container in the same loop; no atomics, no memory ordering. If
/// you need cross-thread access, copy the result into a queue at the
/// boundary rather than sharing the map directly.
///
/// Storage is `std::array<Slot, Capacity>` — fully embedded; no heap
/// allocation after construction. `empty_slot` (= INT64_MIN) is the sentinel
/// for "never written" so any positive presentation time naturally
/// distinguishes from empty.

#include "statusbar/status/statusbar_assert.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace statusbar::container {

template <typename Payload, size_t Capacity>
class PresentationSlotMap
{
  public:
    static_assert(Capacity > 0, "PresentationSlotMap requires Capacity >= 1");

    /// Sentinel timestamp meaning "this slot has never been written."
    /// INT64_MIN is chosen so it can never collide with a real
    /// presentation time (which is always positive in practice).
    static constexpr int64_t empty_slot = std::numeric_limits<int64_t>::min();

    /// Compile-time capacity (number of slots).
    static constexpr size_t capacity = Capacity;

    /// @param slot_width_ns Quantization width per slot, in nanoseconds.
    ///        Must be > 0. The total ring covers `slot_width_ns *
    ///        Capacity` ns of presentation time before reuse.
    explicit PresentationSlotMap(int64_t slot_width_ns) noexcept
        : slot_width_ns_{slot_width_ns}
    {
        STATUSBAR_ASSERT(slot_width_ns_ > 0 && "PresentationSlotMap: slot_width_ns must be > 0");
    }

    /// Width of one slot in ns.
    [[nodiscard]] auto slot_width_ns() const noexcept -> int64_t { return slot_width_ns_; }

    /// Total time span the ring covers before wraparound.
    [[nodiscard]] auto window_ns() const noexcept -> int64_t { return slot_width_ns_ * static_cast<int64_t>(Capacity); }

    /// Unconditional store. Overwrites any prior payload (live or
    /// stale) at the slot owning `time_ns`. Use this for the canonical
    /// source — e.g. a primary stream copy.
    void store(int64_t time_ns, Payload const& payload) noexcept
    {
        size_t const idx = index_for(time_ns);
        slots_[idx].timestamp_ns = quantize(time_ns);
        slots_[idx].payload = payload;
    }

    /// Conditional store. Writes only if the slot does not already hold
    /// a payload whose stored timestamp matches `time_ns`'s quantized
    /// boundary. Use this for fallback sources — e.g. a redundant copy
    /// that should not displace a primary that already arrived.
    /// @return true if the write happened.
    auto store_if_absent(int64_t time_ns, Payload const& payload) noexcept -> bool
    {
        size_t const idx = index_for(time_ns);
        int64_t const q = quantize(time_ns);
        if (slots_[idx].timestamp_ns == q) {
            return false;
        }
        slots_[idx].timestamp_ns = q;
        slots_[idx].payload = payload;
        return true;
    }

    /// Read the slot owning `time_ns`. Returns the stored payload only
    /// when the slot's timestamp matches the requested time's quantized
    /// boundary (i.e. the data really is for the time we asked about).
    /// Returns nullopt for empty / stale / wrong-time slots.
    [[nodiscard]] auto load(int64_t time_ns) const noexcept -> std::optional<Payload>
    {
        size_t const idx = index_for(time_ns);
        if (slots_[idx].timestamp_ns != quantize(time_ns)) {
            return std::nullopt;
        }
        return slots_[idx].payload;
    }

    /// True iff `load(time_ns)` would return a payload. Equivalent to
    /// `load(time_ns).has_value()` without the payload copy.
    [[nodiscard]] auto has_payload_at(int64_t time_ns) const noexcept -> bool
    {
        size_t const idx = index_for(time_ns);
        return slots_[idx].timestamp_ns == quantize(time_ns);
    }

    /// Force-clear the slot owning `time_ns`. After this call,
    /// `load(time_ns)` returns nullopt until a subsequent `store` /
    /// `store_if_absent`. Useful for consumers that want to mark slots
    /// "consumed" so a duplicate read is detectable.
    void clear(int64_t time_ns) noexcept
    {
        size_t const idx = index_for(time_ns);
        slots_[idx].timestamp_ns = empty_slot;
    }

    /// Wipe all slots back to empty.
    void reset() noexcept
    {
        for (auto& s : slots_) {
            s.timestamp_ns = empty_slot;
        }
    }

    /// Snapshot of slot count currently holding a payload (any timestamp
    /// other than empty_slot). Linear scan; intended for tests/diagnostics,
    /// not the hot path.
    [[nodiscard]] auto live_count() const noexcept -> size_t
    {
        size_t n = 0;
        for (auto const& s : slots_) {
            if (s.timestamp_ns != empty_slot) {
                ++n;
            }
        }
        return n;
    }

  private:
    struct Slot
    {
        int64_t timestamp_ns{empty_slot};
        Payload payload{};
    };

    /// Quantize a time to its slot boundary (largest multiple of
    /// slot_width_ns_ that does not exceed time_ns). Negative inputs are
    /// rejected by the debug assert; release builds rely on caller
    /// discipline. Real presentation times are gPTP ns / unix ns and
    /// always positive in practice.
    [[nodiscard]] auto quantize(int64_t time_ns) const noexcept -> int64_t
    {
        STATUSBAR_ASSERT(time_ns >= 0 && "PresentationSlotMap: time_ns must be non-negative");
        return (time_ns / slot_width_ns_) * slot_width_ns_;
    }

    [[nodiscard]] auto index_for(int64_t time_ns) const noexcept -> size_t
    {
        STATUSBAR_ASSERT(time_ns >= 0 && "PresentationSlotMap: time_ns must be non-negative");
        return static_cast<size_t>((time_ns / slot_width_ns_) % static_cast<int64_t>(Capacity));
    }

    int64_t slot_width_ns_;
    std::array<Slot, Capacity> slots_{};
};

}  // namespace statusbar::container
