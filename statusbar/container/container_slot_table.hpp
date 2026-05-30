#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// SlotTable — compile-time bounded, zero-heap container for tracking a small
/// set of outstanding work items that are added, searched by predicate, and
/// removed in arbitrary order.
///
/// Typical use is request/response correlation in a protocol state machine:
/// add an entry when a command is sent, `find_if` when a response arrives or
/// a timer expires, `remove` when handled. Storage is
/// `statusbar::sg14::inplace_vector`, so capacity is a compile-time ceiling
/// (`MaxN`) with an optional runtime soft cap (primarily for tests that want
/// to exercise "full" behavior at a smaller bound).
///
/// Removal is O(1) swap-with-last. Callers must not hold an index or pointer
/// across an `add` / `remove` / `clear`: both the removed slot and the former
/// last slot change identity.
///
/// `find_if` returns `capacity()` (the runtime soft cap) — not `size()` — as
/// the "not found" sentinel, so `index >= table.capacity()` is a uniform
/// not-found check whether or not a soft cap was set.

#include "statusbar/sg14/inplace_vector.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace statusbar::container {

template <typename Entry, size_t MaxN>
class SlotTable
{
  public:
    /// Compile-time hard capacity ceiling.
    static constexpr size_t max_capacity = MaxN;

    /// Default-construct with the full compile-time capacity available.
    SlotTable() noexcept = default;

    /// Construct with a runtime soft cap in [0, MaxN]. Useful for tests that
    /// want to exercise full-slot conditions without changing the compile-time
    /// template parameter. Values greater than MaxN are clamped.
    explicit SlotTable(size_t soft_cap) noexcept
        : soft_cap_{std::min(soft_cap, MaxN)}
    {}

    //
    // Inspection
    //

    [[nodiscard]] auto size() const noexcept -> size_t { return entries_.size(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return entries_.empty(); }
    [[nodiscard]] auto capacity() const noexcept -> size_t { return soft_cap_; }
    [[nodiscard]] auto is_full() const noexcept -> bool { return entries_.size() >= soft_cap_; }

    //
    // Slot lifecycle
    //

    /// Construct a new entry in place. Returns `true` if added, `false` if
    /// the table is at capacity. Callers must check the return value before
    /// sending the associated packet on the wire.
    template <typename... Args>
    auto add(Args&&... args) -> bool
    {
        if (is_full()) {
            return false;
        }
        entries_.emplace_back(std::forward<Args>(args)...);
        return true;
    }

    /// Remove entry at `index`, O(1) via swap-with-last. No-op if the index
    /// is out of range.
    void remove(size_t index) noexcept
    {
        if (index >= entries_.size()) {
            return;
        }
        if (index != entries_.size() - 1) {
            entries_[index] = entries_.back();
        }
        entries_.pop_back();
    }

    /// Clear all entries. Does not change the runtime soft cap.
    void clear() noexcept { entries_.clear(); }

    //
    // Search
    //

    /// Find the first entry satisfying `pred`. Returns its index, or
    /// `capacity()` (the runtime soft cap) if no entry matches, so callers
    /// can use `index >= table.capacity()` as the "not found" check whether
    /// the table was constructed with a soft cap or left at the default
    /// (in which case `capacity() == max_capacity`).
    template <typename Pred>
    [[nodiscard]] auto find_if(Pred const& pred) const -> size_t
    {
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (pred(entries_[i])) {
                return i;
            }
        }
        return soft_cap_;
    }

    //
    // Indexed access (for state-machine actions that hold an index across
    // several steps). UB if `index >= size()`.
    //

    [[nodiscard]] auto operator[](size_t index) noexcept -> Entry& { return entries_[index]; }
    [[nodiscard]] auto operator[](size_t index) const noexcept -> Entry const& { return entries_[index]; }

    /// Bounds-checked pointer access. Returns nullptr if the index is out
    /// of range.
    [[nodiscard]] auto get(size_t index) noexcept -> Entry* { return index < entries_.size() ? &entries_[index] : nullptr; }

    [[nodiscard]] auto get(size_t index) const noexcept -> Entry const*
    {
        return index < entries_.size() ? &entries_[index] : nullptr;
    }

    //
    // Iteration — only over occupied slots.
    //

    [[nodiscard]] auto begin() noexcept { return entries_.begin(); }
    [[nodiscard]] auto end() noexcept { return entries_.end(); }
    [[nodiscard]] auto begin() const noexcept { return entries_.begin(); }
    [[nodiscard]] auto end() const noexcept { return entries_.end(); }

  private:
    statusbar::sg14::inplace_vector<Entry, MaxN> entries_{};
    size_t soft_cap_{MaxN};
};

}  // namespace statusbar::container
