// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer_error.hpp"

#include <string>
#include <system_error>

namespace statusbar {

auto BufferErrorCategory::message(int ev) const -> std::string
{
    switch (static_cast<BufferError>(ev)) {
        case BufferError::insufficient_space:
            return "Insufficient space in buffer";
        case BufferError::invalid_offset:
            return "Invalid offset";
        case BufferError::insufficient_data:
            return "Insufficient data in buffer";
        default:
            return "Unknown buffer error";
    }
}

auto buffer_error_category() noexcept -> std::error_category const&
{
    static BufferErrorCategory const instance;
    return instance;
}

auto make_error_code(BufferError e) noexcept -> std::error_code
{
    return {static_cast<int>(e), buffer_error_category()};
}

}  // namespace statusbar
