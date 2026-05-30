// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/colbin/colbin.hpp"

#include <algorithm>
#include <cstring>

namespace statusbar::colbin {

auto type_size(TypeCode t) noexcept -> uint32_t
{
    switch (t) {
        case TypeCode::i8:
        case TypeCode::u8:
        case TypeCode::b8:
            return 1;
        case TypeCode::i16:
        case TypeCode::u16:
            return 2;
        case TypeCode::i32:
        case TypeCode::u32:
        case TypeCode::f32:
            return 4;
        case TypeCode::i64:
        case TypeCode::u64:
        case TypeCode::f64:
            return 8;
    }
    return 0;
}

auto type_align(TypeCode t) noexcept -> uint32_t
{
    return type_size(t);
}

auto type_name(TypeCode t) noexcept -> std::string_view
{
    switch (t) {
        case TypeCode::i8:
            return "i8";
        case TypeCode::u8:
            return "u8";
        case TypeCode::i16:
            return "i16";
        case TypeCode::u16:
            return "u16";
        case TypeCode::i32:
            return "i32";
        case TypeCode::u32:
            return "u32";
        case TypeCode::i64:
            return "i64";
        case TypeCode::u64:
            return "u64";
        case TypeCode::f32:
            return "f32";
        case TypeCode::f64:
            return "f64";
        case TypeCode::b8:
            return "b8";
    }
    return {};
}

auto parse_type(std::string_view name) noexcept -> StatusValue<TypeCode>
{
    if (name == "i8") {
        return TypeCode::i8;
    }
    if (name == "u8") {
        return TypeCode::u8;
    }
    if (name == "i16") {
        return TypeCode::i16;
    }
    if (name == "u16") {
        return TypeCode::u16;
    }
    if (name == "i32") {
        return TypeCode::i32;
    }
    if (name == "u32") {
        return TypeCode::u32;
    }
    if (name == "i64") {
        return TypeCode::i64;
    }
    if (name == "u64") {
        return TypeCode::u64;
    }
    if (name == "f32") {
        return TypeCode::f32;
    }
    if (name == "f64") {
        return TypeCode::f64;
    }
    if (name == "b8") {
        return TypeCode::b8;
    }
    return std::unexpected{make_error_code(ColbinError::invalid_schema)};
}

static auto align_up(uint32_t x, uint32_t a) noexcept -> uint32_t
{
    return (x + a - 1) & ~(a - 1);
}

auto resolve_schema(std::span<ColumnSpec const> cols) -> ResolvedSchema
{
    ResolvedSchema r;
    r.columns.reserve(cols.size());
    uint32_t off = 0;
    uint32_t max_align = 1;
    for (auto const& c : cols) {
        uint32_t const sz = type_size(c.type);
        uint32_t const al = type_align(c.type);
        off = align_up(off, al);
        r.columns.push_back(ResolvedColumn{.name = c.name, .type = c.type, .offset = off, .size = sz});
        off += sz;
        max_align = std::max(max_align, al);
    }
    r.row_align = max_align;
    r.row_size = align_up(off, max_align);
    if (r.row_size == 0) {
        r.row_size = 1;  // degenerate empty schema; one byte/row
    }
    return r;
}

auto serialize_schema(std::span<ColumnSpec const> cols) -> std::string
{
    std::string out;
    bool first = true;
    for (auto const& c : cols) {
        if (!first) {
            out.push_back(',');
        }
        out.append(c.name);
        out.push_back(':');
        out.append(type_name(c.type));
        first = false;
    }
    return out;
}

static auto trim_trailing_newline(std::string_view s) noexcept -> std::string_view
{
    if (!s.empty() && s.back() == '\n') {
        s.remove_suffix(1);
    }
    return s;
}

static auto is_valid_name(std::string_view name) noexcept -> bool
{
    if (name.empty()) {
        return false;
    }
    char const c0 = name.front();
    if ((c0 < 'A' || c0 > 'Z') && (c0 < 'a' || c0 > 'z') && c0 != '_') {
        return false;
    }
    for (char const c : name.substr(1)) {
        bool const alnum = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!alnum && c != '_') {
            return false;
        }
    }
    return true;
}

auto parse_schema(std::string_view text) -> StatusValue<std::vector<ColumnSpec>>
{
    text = trim_trailing_newline(text);
    std::vector<ColumnSpec> out;
    if (text.empty()) {
        return out;
    }
    size_t i = 0;
    while (i < text.size()) {
        size_t const comma = text.find(',', i);
        std::string_view const field = text.substr(i, comma == std::string_view::npos ? text.size() - i : comma - i);
        size_t const colon = field.find(':');
        if (colon == std::string_view::npos) {
            return std::unexpected{make_error_code(ColbinError::invalid_schema)};
        }
        std::string_view const name = field.substr(0, colon);
        std::string_view const tname = field.substr(colon + 1);
        if (!is_valid_name(name)) {
            return std::unexpected{make_error_code(ColbinError::invalid_schema)};
        }
        auto t = parse_type(tname);
        if (!t) {
            return std::unexpected{t.error()};
        }
        out.push_back(ColumnSpec{.name = std::string{name}, .type = *t});
        if (comma == std::string_view::npos) {
            break;
        }
        i = comma + 1;
    }
    return out;
}

}  // namespace statusbar::colbin
