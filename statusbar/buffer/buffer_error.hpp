#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include <array>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace statusbar {

///
/// Error codes for generic buffer operations.
///
enum class BufferError
{
    insufficient_space = 1,  ///< Not enough space in buffer for write operations
    invalid_offset = 2,      ///< Offset is out of bounds
    insufficient_data = 3,   ///< Not enough data in buffer for read operations
};

///
/// Error category for buffer errors.
///
class BufferErrorCategory : public std::error_category
{
  public:
    [[nodiscard]] auto name() const noexcept -> char const* override { return "statusbar.buffer"; }

    [[nodiscard]] auto message(int ev) const -> std::string override;
};

///
/// Get the global buffer error category instance.
///
[[nodiscard]] auto buffer_error_category() noexcept -> std::error_category const&;

///
/// Create an error_code from a BufferError.
///
[[nodiscard]] auto make_error_code(BufferError e) noexcept -> std::error_code;

}  // namespace statusbar

///
/// Register BufferError as an error_code enum.
///
template <>
struct std::is_error_code_enum<statusbar::BufferError> : std::true_type
{};
