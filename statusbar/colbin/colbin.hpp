#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/colbin/colbin_error.hpp"
#include "statusbar/status/status.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace statusbar::colbin {

/// On-disk magic for v1 .colbin files. 8 ASCII bytes, no NUL terminator.
inline constexpr std::array<char, 8> MAGIC{'S', 'T', 'B', 'C', 'O', 'L', 'B', 'N'};

/// Wire endian marker — 0x12345678 read as a little-endian uint32 means
/// the file was produced on a host with the same byte order as the reader.
inline constexpr uint32_t ENDIAN_MARKER = 0x12345678u;

/// Format version. Incompatible changes bump this.
inline constexpr uint32_t FORMAT_VERSION = 1;

/// Fixed-size header preamble. Schema text follows immediately, zero-
/// padded out to `header_total` bytes (always a multiple of `row_size`,
/// rounded up to at least 64 B for cache-line alignment).
inline constexpr uint32_t HEADER_PREAMBLE_BYTES = 64;

/// Type codes used both in the on-disk schema and at runtime. Numeric
/// values are stable across format versions.
enum class TypeCode : uint8_t
{
    i8 = 1,
    u8 = 2,
    i16 = 3,
    u16 = 4,
    i32 = 5,
    u32 = 6,
    i64 = 7,
    u64 = 8,
    f32 = 9,
    f64 = 10,
    b8 = 11,  ///< 1-byte bool (0 or 1)
};

/// Byte width of a value of `t`.
[[nodiscard]] auto type_size(TypeCode t) noexcept -> uint32_t;

/// Natural alignment of a value of `t` (== type_size for the types we
/// support; kept as a separate accessor for clarity).
[[nodiscard]] auto type_align(TypeCode t) noexcept -> uint32_t;

/// Schema text spelling (e.g. TypeCode::i64 → "i64").
[[nodiscard]] auto type_name(TypeCode t) noexcept -> std::string_view;

/// Parse a type spelling (inverse of type_name).
[[nodiscard]] auto parse_type(std::string_view name) noexcept -> StatusValue<TypeCode>;

/// One column in the schema.
struct ColumnSpec
{
    std::string name;
    TypeCode type;
};

/// Column resolved against the row's byte layout.
struct ResolvedColumn
{
    std::string name;
    TypeCode type;
    uint32_t offset;
    uint32_t size;
};

/// Result of resolving a column list into row offsets.
struct ResolvedSchema
{
    std::vector<ResolvedColumn> columns;
    uint32_t row_size;   ///< total row size in bytes (alignment-padded)
    uint32_t row_align;  ///< max field alignment in the row
};

/// Lay out `cols` into a packed row with natural alignment per field.
/// Matches numpy's `np.dtype([...], align=True)` semantics: each field
/// is bumped to its own alignment, and `row_size` is rounded up to the
/// largest field's alignment so consecutive rows stay aligned.
[[nodiscard]] auto resolve_schema(std::span<ColumnSpec const> cols) -> ResolvedSchema;

/// Serialize a resolved schema to the on-disk text form
/// (`name:type,name:type,...`, no trailing newline).
[[nodiscard]] auto serialize_schema(std::span<ColumnSpec const> cols) -> std::string;

/// Parse the on-disk schema text into a column list. Accepts an
/// optional trailing newline. Returns failure on malformed input.
[[nodiscard]] auto parse_schema(std::string_view text) -> StatusValue<std::vector<ColumnSpec>>;

}  // namespace statusbar::colbin
