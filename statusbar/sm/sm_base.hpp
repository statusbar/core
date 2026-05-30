#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include <array>
#include <cstdint>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>

namespace statusbar::sm {

//
// Enum Traits
//

/// Concept for enums that have a UCT (Unconditional Transition) member
template <class E>
concept HasUct = std::is_enum_v<E> && requires { E::UCT; };

/// Trait to extract UCT value from an enum
template <class E>
struct enum_uct;

template <HasUct E>
struct enum_uct<E>
{
    static constexpr auto value = E::UCT;
};

/// Concept for enums that have a Count member
template <class E>
concept HasCount = std::is_enum_v<E> && requires { E::Count; };

/// Trait to extract count from an enum
template <class E>
struct enum_count;

template <HasCount E>
struct enum_count<E>
{
    // Note: Using static_cast instead of std::to_underlying to avoid C++ modules visibility issues
    static constexpr auto value = static_cast<std::underlying_type_t<E>>(E::Count);
};

//
// Compile-time Name Reflection
//

/// Find the end of the NTTP/type value in a source_location function_name() string.
/// Clang uses "[... Value = Foo::Bar]", GCC uses "[... V = Foo::Bar; ...]".
consteval auto find_nttp_value_end(std::string_view name, size_t start) -> size_t
{
    auto end_semi = name.find(';', start);
    auto end_bracket = name.rfind(']');
    auto end = std::string_view::npos;
    if (end_semi != std::string_view::npos && end_semi > start) {
        end = end_semi;
    }
    if (end_bracket != std::string_view::npos && end_bracket > start) {
        if (end == std::string_view::npos || end_bracket < end) {
            end = end_bracket;
        }
    }
    return end;
}

/// Extract the last component after :: from a fully qualified name
consteval auto extract_name(std::string_view full) -> std::string_view
{
    auto colon = full.rfind("::");
    if (colon != std::string_view::npos) {
        return full.substr(colon + 2);
    }
    return full;
}

/// Implementation for enum_name - extracts name from source_location
/// Handles both Clang ("[... Value = Foo::Bar]") and GCC ("[... V = Foo::Bar; ...]") formats.
template <auto Value>
consteval auto enum_name_impl() -> std::string_view
{
    std::string_view const name = std::source_location::current().function_name();
    auto start = name.find("= ");
    if (start == std::string_view::npos) {
        return "UNKNOWN";
    }
    start += 2;
    auto end = find_nttp_value_end(name, start);
    if (end == std::string_view::npos || end <= start) {
        return "UNKNOWN";
    }
    return extract_name(name.substr(start, end - start));
}

/// Compile-time enum value to name conversion
template <auto Value>
constexpr std::string_view enum_name = enum_name_impl<Value>();  // NOLINT(modernize-avoid-c-style-cast)

/// Implementation for type_name - extracts type name from source_location
template <typename T>
consteval auto type_name_impl() -> std::string_view
{
    std::string_view const name = std::source_location::current().function_name();
    auto start = name.find("= ");
    if (start == std::string_view::npos) {
        return "UNKNOWN";
    }
    start += 2;
    auto end = find_nttp_value_end(name, start);
    if (end == std::string_view::npos || end <= start) {
        return "UNKNOWN";
    }
    return extract_name(name.substr(start, end - start));
}

/// Compile-time type to name conversion
template <typename T>
constexpr std::string_view type_name = type_name_impl<T>();

/// Implementation for function_name - extracts function pointer name from source_location
template <auto FnPtr>
consteval auto function_name_impl() -> std::string_view
{
    std::string_view const name = std::source_location::current().function_name();
    auto start = name.find("= ");
    if (start == std::string_view::npos) {
        return "";
    }
    start += 2;
    auto end = find_nttp_value_end(name, start);
    if (end == std::string_view::npos || end <= start) {
        return "";
    }
    std::string_view full = name.substr(start, end - start);
    if (!full.empty() && full[0] == '&') {
        full = full.substr(1);
    }
    return extract_name(full);
}

/// Compile-time function pointer to name conversion
template <auto FnPtr>
constexpr std::string_view function_name = function_name_impl<FnPtr>();

/// Auto-generate array of enum names for enum E with Count values
template <typename E, E Count, typename = std::make_index_sequence<static_cast<size_t>(Count)>>
struct EnumNamesFor;

template <typename E, E Count, size_t... Is>
struct EnumNamesFor<E, Count, std::index_sequence<Is...>>
{
    static constexpr size_t size = sizeof...(Is);
    static constexpr std::array<std::string_view, size> names = {enum_name<static_cast<E>(Is)>...};

    static constexpr auto get(E value) noexcept -> std::string_view
    {
        auto idx = static_cast<size_t>(value);
        return (idx < size) ? names[idx] : "UNKNOWN";
    }
};

//
// Fixed-capacity String for Compile-time Generation
//

/// Fixed-capacity string for compile-time string building
template <size_t Capacity>
struct FixedString
{
    std::array<char, Capacity> data{};
    size_t len{0};

    constexpr void append(std::string_view sv) noexcept
    {
        for (char const c : sv) {
            if (len < Capacity - 1) {
                data[len++] = c;
            }
        }
    }

    constexpr void append(char c) noexcept
    {
        if (len < Capacity - 1) {
            data[len++] = c;
        }
    }

    [[nodiscard]] constexpr auto view() const noexcept -> std::string_view { return {data.data(), len}; }

    [[nodiscard]] constexpr auto size() const noexcept -> size_t { return len; }

    [[nodiscard]] constexpr auto empty() const noexcept -> bool { return len == 0; }
};

}  // namespace statusbar::sm
