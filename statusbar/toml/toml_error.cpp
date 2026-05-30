// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/toml/toml_error.hpp"

#include <string>
#include <system_error>

namespace statusbar::toml {

auto TomlErrorCategory::message(int ev) const -> std::string
{
    switch (static_cast<TomlError>(ev)) {
        case TomlError::file_not_found:
            return "file not found";
        case TomlError::file_read_error:
            return "file read error";
        case TomlError::unexpected_character:
            return "unexpected character";
        case TomlError::unterminated_string:
            return "unterminated string";
        case TomlError::invalid_escape_sequence:
            return "invalid escape sequence";
        case TomlError::invalid_number:
            return "invalid number";
        case TomlError::invalid_boolean:
            return "invalid boolean";
        case TomlError::invalid_date:
            return "invalid date/time";
        case TomlError::invalid_key:
            return "invalid key";
        case TomlError::duplicate_key:
            return "duplicate key";
        case TomlError::expected_equals:
            return "expected '='";
        case TomlError::expected_value:
            return "expected value";
        case TomlError::expected_newline:
            return "expected newline";
        case TomlError::expected_bracket:
            return "expected ']'";
        case TomlError::unterminated_array:
            return "unterminated array";
        case TomlError::unterminated_inline_table:
            return "unterminated inline table";
        case TomlError::nested_inline_table:
            return "nested inline tables not allowed";
        case TomlError::invalid_table_header:
            return "invalid table header";
        case TomlError::invalid_array_table_header:
            return "invalid array of tables header";
        case TomlError::key_not_found:
            return "key not found";
        case TomlError::type_mismatch:
            return "type mismatch";
        case TomlError::index_out_of_range:
            return "array index out of range";
        case TomlError::number_overflow:
            return "integer overflow";
        case TomlError::invalid_unicode_codepoint:
            return "invalid unicode codepoint (surrogate not allowed)";
        case TomlError::nesting_too_deep:
            return "TOML nesting exceeds maximum supported depth";
        default:
            return "unknown TOML error";
    }
}

auto toml_error_category() noexcept -> std::error_category const&
{
    static TomlErrorCategory const category;
    return category;
}

auto make_error_code(TomlError e) noexcept -> std::error_code
{
    return {static_cast<int>(e), toml_error_category()};
}

}  // namespace statusbar::toml
