#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// TOML Error types
/// Error codes and category for TOML parsing

#include <string>
#include <system_error>

namespace statusbar::toml {

/// TOML parsing error codes
enum class TomlError
{
    file_not_found = 1,
    file_read_error,
    unexpected_character,
    unterminated_string,
    invalid_escape_sequence,
    invalid_number,
    invalid_boolean,
    invalid_date,
    invalid_key,
    duplicate_key,
    expected_equals,
    expected_value,
    expected_newline,
    expected_bracket,
    unterminated_array,
    unterminated_inline_table,
    nested_inline_table,
    invalid_table_header,
    invalid_array_table_header,
    key_not_found,
    type_mismatch,
    index_out_of_range,
    number_overflow,
    invalid_unicode_codepoint,
    nesting_too_deep,
};

/// Error category for TOML errors
class TomlErrorCategory : public std::error_category
{
  public:
    [[nodiscard]] auto name() const noexcept -> char const* override { return "statusbar.toml"; }

    [[nodiscard]] auto message(int ev) const -> std::string override;
};

/// Get the TOML error category singleton
[[nodiscard]] auto toml_error_category() noexcept -> std::error_category const&;

/// Create an error_code from a TomlError
/// @param e The TOML error code to convert
[[nodiscard]] auto make_error_code(TomlError e) noexcept -> std::error_code;

}  // namespace statusbar::toml

template <>
struct std::is_error_code_enum<statusbar::toml::TomlError> : std::true_type
{};
