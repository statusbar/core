#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include <string>
#include <system_error>

namespace statusbar::colbin {

/// Error codes for the colbin file format module.
enum class ColbinError
{
    /// Bad magic / missing signature.
    bad_magic = 1,
    /// File was written on a host with different byte order.
    endian_mismatch = 2,
    /// Format version is newer than this build understands.
    unsupported_version = 3,
    /// Header is too small or truncated.
    truncated_header = 4,
    /// Schema text is malformed (bad type name, unknown column, etc.).
    invalid_schema = 5,
    /// Row size computed from schema doesn't match what the header says.
    schema_row_size_mismatch = 6,
    /// Header's `committed_rows` * row_size would exceed file size.
    committed_rows_overflow = 7,
    /// open / read / write / mmap / ftruncate / mremap syscall failed
    /// (details captured via errno -> std::generic_category).
    io_error = 8,
    /// The writer was asked to grow past its configured maximum size.
    capacity_exceeded = 9,
    /// Writer's mmap'd region wasn't large enough to grow into.
    grow_failed = 10,
};

/// Error category for ColbinError values.
class ColbinErrorCategory : public std::error_category
{
  public:
    [[nodiscard]] auto name() const noexcept -> char const* override { return "statusbar.colbin"; }

    [[nodiscard]] auto message(int ev) const -> std::string override;
};

[[nodiscard]] auto colbin_error_category() noexcept -> std::error_category const&;

[[nodiscard]] auto make_error_code(ColbinError e) noexcept -> std::error_code;

}  // namespace statusbar::colbin

template <>
struct std::is_error_code_enum<statusbar::colbin::ColbinError> : std::true_type
{};
