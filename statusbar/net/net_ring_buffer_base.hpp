#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/span_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace statusbar::net {

/// Non-template ring buffer operating on a span of bytes.
/// Derived classes provide the std::array storage and pass a span to this base.
class RingBufferBase
{
  public:
    explicit RingBufferBase(std::span<uint8_t> storage) noexcept
        : storage_{storage}
    {}

    [[nodiscard]] auto readable_count() const noexcept -> size_t
    {
        if (write_pos_ >= read_pos_) {
            return write_pos_ - read_pos_;
        }
        return storage_.size() - read_pos_ + write_pos_;
    }

    [[nodiscard]] auto writable_count() const noexcept -> size_t { return storage_.size() - readable_count() - 1; }

    [[nodiscard]] auto readable_span() const noexcept -> std::span<uint8_t const>
    {
        if (write_pos_ >= read_pos_) {
            return storage_.subspan(read_pos_, write_pos_ - read_pos_);
        }
        return storage_.subspan(read_pos_, storage_.size() - read_pos_);
    }

    [[nodiscard]] auto writable_span() noexcept -> std::span<uint8_t>
    {
        size_t const cap = storage_.size();
        size_t const end = (read_pos_ == 0) ? cap - 1 : cap;
        if (write_pos_ >= read_pos_) {
            return storage_.subspan(write_pos_, end - write_pos_);
        }
        return storage_.subspan(write_pos_, read_pos_ - write_pos_ - 1);
    }

    void advance_read(size_t n) noexcept
    {
        n = std::min(n, readable_count());
        read_pos_ = (read_pos_ + n) % storage_.size();
    }

    void advance_write(size_t n) noexcept
    {
        n = std::min(n, writable_count());
        write_pos_ = (write_pos_ + n) % storage_.size();
    }

    auto write(std::span<uint8_t const> data) noexcept -> size_t;

    auto read(std::span<uint8_t> dest) noexcept -> size_t;

    [[nodiscard]] auto empty() const noexcept -> bool { return read_pos_ == write_pos_; }
    [[nodiscard]] auto full() const noexcept -> bool { return writable_count() == 0; }

    void clear() noexcept
    {
        read_pos_ = 0;
        write_pos_ = 0;
    }

  private:
    std::span<uint8_t> storage_;
    size_t read_pos_{0};
    size_t write_pos_{0};
};

}  // namespace statusbar::net
