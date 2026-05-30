#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// A single latest-value scalar published from one writer thread to
/// reader threads. A naturally-aligned scalar is already tear-free
/// through a plain `std::atomic<T>`; Published<T> is a thin named
/// wrapper that documents the publish/read contract. It is NOT a
/// triple buffer — the triple buffer earns its third slot only for
/// multi-field structs.

#include <atomic>
#include <type_traits>

namespace statusbar::itc {

template <typename T>
class Published
{
  public:
    static_assert(std::is_trivially_copyable_v<T>, "Published<T> requires trivially copyable T");
    static_assert(std::atomic<T>::is_always_lock_free, "Published<T> requires a lock-free std::atomic<T>");

    Published() noexcept = default;
    explicit Published(T initial) noexcept
        : value_{initial}
    {}

    Published(Published const&) = delete;
    Published(Published&&) = delete;
    Published& operator=(Published const&) = delete;
    Published& operator=(Published&&) = delete;

    /// Writer side. Publish the latest value.
    void publish(T v) noexcept { value_.store(v, std::memory_order_release); }

    /// Reader side. Read the most recently published value.
    [[nodiscard]] auto load() const noexcept -> T { return value_.load(std::memory_order_acquire); }

  private:
    std::atomic<T> value_{};
};

}  // namespace statusbar::itc
