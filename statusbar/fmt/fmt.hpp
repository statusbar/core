// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// statusbar::fmt — a heap-free, std::format-free, compile-time-checked
/// formatter for integers, hex, binary, and compile-time-bounded strings.
///
///   auto e = fmt::format<"0x{:04x}">(ethertype);  // fixed_str<6>, "0x88f7"
///
/// The format string is a template argument; the result type fixed_str<N> is
/// sized exactly from it. No heap, no Ryu, no locale, no runtime throw.

#pragma once

#include "statusbar/fmt/fmt_fixed_str.hpp"
#include "statusbar/fmt/fmt_spec.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace statusbar::fmt {

namespace detail {

/// An integral argument that is formatted numerically (excludes char/bool).
template <typename T>
concept fmt_integer = std::integral<std::remove_cvref_t<T>> && !std::same_as<std::remove_cvref_t<T>, char> &&
    !std::same_as<std::remove_cvref_t<T>, bool>;

/// Emit `mag` in `base` into `out`, honoring sign, width and zero/space pad.
template <typename Out>
constexpr void emit_uint(Out& out, std::uint64_t mag, unsigned base, bool upper, bool neg, std::size_t width, bool zero) noexcept
{
    char tmp[64] = {};  // zero-init so emit_uint is usable in constant evaluation
    std::size_t n = 0;
    char const* const digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (mag == 0) {
        tmp[n++] = '0';
    }
    while (mag != 0) {
        tmp[n++] = digits[mag % base];
        mag /= base;
    }
    std::size_t const body = n + (neg ? 1 : 0);
    if (zero) {
        if (neg) {
            out.push_back('-');
        }
        for (std::size_t i = body; i < width; ++i) {
            out.push_back('0');
        }
    } else {
        for (std::size_t i = body; i < width; ++i) {
            out.push_back(' ');
        }
        if (neg) {
            out.push_back('-');
        }
    }
    while (n != 0) {
        out.push_back(tmp[--n]);
    }
}

template <typename Out>
constexpr void format_arg(Out& out, fmt_integer auto v, Spec sp) noexcept
{
    using T = decltype(v);
    unsigned const base = (sp.type == 'x' || sp.type == 'X') ? 16U : (sp.type == 'b' || sp.type == 'B') ? 2U : 10U;
    bool const upper = (sp.type == 'X');
    bool neg = false;
    std::uint64_t mag = 0;
    if constexpr (std::is_signed_v<T>) {
        if (v < 0) {
            neg = true;
            mag = static_cast<std::uint64_t>(0) - static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
        } else {
            mag = static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
        }
    } else {
        mag = static_cast<std::uint64_t>(v);
    }
    emit_uint(out, mag, base, upper, neg, sp.width, sp.zero);
}

template <typename Out>
constexpr void format_arg(Out& out, char c, Spec /*sp*/) noexcept
{
    out.push_back(c);
}

template <typename Out, std::size_t K>
constexpr void format_arg(Out& out, char const (&s)[K], Spec /*sp*/) noexcept
{
    for (std::size_t i = 0; (i + 1) < K && s[i] != '\0'; ++i) {
        out.push_back(s[i]);
    }
}

template <typename Out, std::size_t M>
constexpr void format_arg(Out& out, fixed_str<M> const& s, Spec /*sp*/) noexcept
{
    for (char const c : s.view()) {
        out.push_back(c);
    }
}

}  // namespace detail

/// Format `args` per the compile-time format string `Fmt`. Returns an exactly
/// sized fixed_str. Validation happens at compile time in worst_case<>().
template <detail::literal Fmt, typename... Args>
constexpr auto format(Args const&... args) -> fixed_str<detail::worst_case<Fmt, Args...>()>
{
    fixed_str<detail::worst_case<Fmt, Args...>()> out;
    std::string_view const f = Fmt.view();
    std::size_t i = 0;

    auto step = [&](auto const& arg) constexpr {
        while (i < f.size()) {
            char const c = f[i];
            if (c == '{') {
                if (i + 1 < f.size() && f[i + 1] == '{') {
                    out.push_back('{');
                    i += 2;
                    continue;
                }
                break;  // a real field
            }
            if (c == '}' && i + 1 < f.size() && f[i + 1] == '}') {
                out.push_back('}');
                i += 2;
                continue;
            }
            out.push_back(c);
            ++i;
        }
        detail::Spec sp;
        i = detail::parse_field(f, i, sp);
        detail::format_arg(out, arg, sp);
    };
    (step(args), ...);

    while (i < f.size()) {  // trailing literal text
        char const c = f[i];
        if (c == '{' && i + 1 < f.size() && f[i + 1] == '{') {
            out.push_back('{');
            i += 2;
        } else if (c == '}' && i + 1 < f.size() && f[i + 1] == '}') {
            out.push_back('}');
            i += 2;
        } else {
            out.push_back(c);
            ++i;
        }
    }
    return out;
}

}  // namespace statusbar::fmt
