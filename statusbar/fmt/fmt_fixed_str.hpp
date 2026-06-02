// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// fixed_str<N>: a heap-free, trivially-copyable inline string of capacity N
/// (+1 byte for a NUL terminator). The return type of statusbar::fmt::format.
/// Usable from realtime / ISR paths (no allocation) and converts to
/// std::string_view for host convenience. Deliberately does NOT include
/// <format> or <string> — that is the whole point of this module.

#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace statusbar::fmt {

template <std::size_t N>
struct fixed_str
{
    static constexpr std::size_t capacity = N;

    std::array<char, N + 1> data_{};
    std::size_t len_ = 0;

    constexpr fixed_str() noexcept = default;

    /// Construct from a bounded source, copying min(src.size(), N) bytes.
    constexpr explicit fixed_str(std::string_view src) noexcept
    {
        len_ = src.size() < N ? src.size() : N;
        for (std::size_t i = 0; i < len_; ++i) {
            data_[i] = src[i];
        }
        data_[len_] = '\0';
    }

    /// Widening constructor: fixed_str<M> -> fixed_str<N> when M <= N.
    template <std::size_t M>
        requires(M <= N)
    constexpr fixed_str(fixed_str<M> const& other) noexcept
    {
        len_ = other.size();
        auto const v = other.view();
        for (std::size_t i = 0; i < len_; ++i) {
            data_[i] = v[i];
        }
        data_[len_] = '\0';
    }

    [[nodiscard]] constexpr char const* c_str() const noexcept { return data_.data(); }
    [[nodiscard]] constexpr std::string_view view() const noexcept { return {data_.data(), len_}; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return len_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return len_ == 0; }

    constexpr operator std::string_view() const noexcept { return view(); }

    /// Append one char (no-op past capacity; capacity is sized so this never
    /// truncates for a validated format). Used by the formatter.
    constexpr void push_back(char c) noexcept
    {
        if (len_ < N) {
            data_[len_++] = c;
            data_[len_] = '\0';
        }
    }
};

template <std::size_t N>
constexpr bool operator==(fixed_str<N> const& a, std::string_view b) noexcept
{
    return a.view() == b;
}

// trait: is this type a fixed_str<...>?  (used by the validator)
namespace detail {
template <typename T>
struct is_fixed_str : std::false_type
{};
template <std::size_t N>
struct is_fixed_str<fixed_str<N>> : std::true_type
{};
template <typename T>
inline constexpr std::size_t fixed_str_capacity = 0;
template <std::size_t N>
inline constexpr std::size_t fixed_str_capacity<fixed_str<N>> = N;
}  // namespace detail

}  // namespace statusbar::fmt
