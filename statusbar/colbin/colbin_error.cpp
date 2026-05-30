// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/colbin/colbin_error.hpp"

namespace statusbar::colbin {

auto ColbinErrorCategory::message(int ev) const -> std::string
{
    switch (static_cast<ColbinError>(ev)) {
        case ColbinError::bad_magic:
            return "Bad colbin magic";
        case ColbinError::endian_mismatch:
            return "Colbin endian marker mismatch (file produced on different host endianness)";
        case ColbinError::unsupported_version:
            return "Unsupported colbin format version";
        case ColbinError::truncated_header:
            return "Truncated colbin header";
        case ColbinError::invalid_schema:
            return "Malformed colbin schema text";
        case ColbinError::schema_row_size_mismatch:
            return "Schema-derived row size does not match header row_size";
        case ColbinError::committed_rows_overflow:
            return "committed_rows would extend past end of file";
        case ColbinError::io_error:
            return "Colbin I/O error";
        case ColbinError::capacity_exceeded:
            return "Colbin writer exceeded configured maximum file size";
        case ColbinError::grow_failed:
            return "Colbin writer failed to grow mmap region";
    }
    return "Unknown colbin error";
}

auto colbin_error_category() noexcept -> std::error_category const&
{
    static ColbinErrorCategory const instance;
    return instance;
}

auto make_error_code(ColbinError e) noexcept -> std::error_code
{
    return {static_cast<int>(e), colbin_error_category()};
}

}  // namespace statusbar::colbin
