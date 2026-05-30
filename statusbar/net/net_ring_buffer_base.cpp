// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/net/net_ring_buffer_base.hpp"

namespace statusbar::net {

auto RingBufferBase::write(std::span<uint8_t const> data) noexcept -> size_t
{
    size_t total = 0;
    while (!data.empty() && writable_count() > 0) {
        auto span = writable_span();
        size_t const n = std::min(span.size(), data.size());
        span_copy(span.subspan(0, n), data.subspan(0, n));
        advance_write(n);
        data = data.subspan(n);
        total += n;
    }
    return total;
}

auto RingBufferBase::read(std::span<uint8_t> dest) noexcept -> size_t
{
    size_t total = 0;
    while (!dest.empty() && readable_count() > 0) {
        auto span = readable_span();
        size_t const n = std::min(span.size(), dest.size());
        span_copy(dest.subspan(0, n), span.subspan(0, n));
        advance_read(n);
        dest = dest.subspan(n);
        total += n;
    }
    return total;
}

}  // namespace statusbar::net
