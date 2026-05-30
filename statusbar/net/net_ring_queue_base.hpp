#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include <cstddef>
#include <cstdint>

namespace statusbar::net {

/// Non-template ring queue index management.
/// Operates on raw storage via pointer arithmetic — the typed wrapper
/// (FixedQueue) provides type-safe accessors.
class RingQueueBase
{
  public:
    RingQueueBase(uint8_t* storage, size_t element_size, size_t capacity) noexcept
        : storage_{storage}
        , element_size_{element_size}
        , capacity_{capacity}
    {}

    [[nodiscard]] auto can_push() const noexcept -> bool { return count_ < capacity_; }
    [[nodiscard]] auto can_pop() const noexcept -> bool { return count_ > 0; }
    [[nodiscard]] auto count() const noexcept -> size_t { return count_; }
    [[nodiscard]] auto empty() const noexcept -> bool { return count_ == 0; }
    [[nodiscard]] auto full() const noexcept -> bool { return count_ == capacity_; }

    [[nodiscard]] auto push_slot_ptr() noexcept -> void*
    {
        if (!can_push()) {
            return nullptr;
        }
        return storage_ + (write_pos_ * element_size_);
    }

    void commit_push() noexcept
    {
        write_pos_ = (write_pos_ + 1) % capacity_;
        ++count_;
    }

    [[nodiscard]] auto peek_ptr() const noexcept -> void const*
    {
        if (!can_pop()) {
            return nullptr;
        }
        return storage_ + (read_pos_ * element_size_);
    }

    [[nodiscard]] auto peek_ptr() noexcept -> void*
    {
        if (!can_pop()) {
            return nullptr;
        }
        return storage_ + (read_pos_ * element_size_);
    }

    void pop_advance() noexcept
    {
        read_pos_ = (read_pos_ + 1) % capacity_;
        --count_;
    }

    void reset() noexcept
    {
        read_pos_ = 0;
        write_pos_ = 0;
        count_ = 0;
    }

  private:
    uint8_t* storage_;
    size_t element_size_;
    size_t capacity_;
    size_t read_pos_{0};
    size_t write_pos_{0};
    size_t count_{0};
};

}  // namespace statusbar::net
