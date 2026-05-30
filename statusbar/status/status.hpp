#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/status/statusbar_assert.hpp"

#include <expected>
#include <system_error>
#include <type_traits>
#include <utility>

namespace statusbar {

///
/// Status represents the result of an operation that returns no value.
/// It's either success (empty) or failure (contains std::error_code).
///
/// Example:
/// \code
///   Status do_something() {
///       if (error_condition) {
///           return failure(std::errc::invalid_argument);
///       }
///       return success();
///   }
///
///   // Usage:
///   auto result = do_something();
///   if (result) {
///       // Success
///   } else {
///       std::println("Error: {}", result.error().message());
///   }
/// \endcode
///
using Status = std::expected<void, std::error_code>;

///
/// StatusValue<T> represents the result of an operation that returns a value of type T.
/// It's either success (contains T) or failure (contains std::error_code).
///
/// Example:
/// \code
///   StatusValue<int> parse_int(std::string_view str) {
///       if (str.empty()) {
///           return failure(std::errc::invalid_argument);
///       }
///       int value = /* parse */;
///       return success(value);
///   }
/// \endcode
///
template <typename ValueT>
using StatusValue = std::expected<ValueT, std::error_code>;

//
// Success helpers
//
/// Create a successful Status (void result).
[[nodiscard]] inline auto success() noexcept -> Status
{
    return {};
}

/// Create a successful StatusValue<T> from a value.
template <typename ValueT>
[[nodiscard]] inline auto success(ValueT&& value) noexcept(std::is_nothrow_constructible_v<std::decay_t<ValueT>, ValueT>)
    -> StatusValue<std::decay_t<ValueT>>
{
    return {std::forward<ValueT>(value)};
}

//
// Failure helpers — each returns std::unexpected<std::error_code>, which
// implicitly converts to both Status and StatusValue<T> at the call site.
//
[[nodiscard]] inline auto failure(std::error_code err) noexcept -> std::unexpected<std::error_code>
{
    return std::unexpected<std::error_code>(err);
}

[[nodiscard]] inline auto failure(std::errc err) noexcept -> std::unexpected<std::error_code>
{
    return std::unexpected<std::error_code>(std::make_error_code(err));
}

template <typename E>
    requires std::is_error_code_enum_v<E>
[[nodiscard]] inline auto failure(E err) noexcept -> std::unexpected<std::error_code>
{
    return std::unexpected<std::error_code>(std::error_code(err));
}

//
// Query helpers — thin sugar over `.has_value()` for readability at call sites.
// Everything else (and_then, or_else, transform, value_or, value_or_throw, etc.)
// is available as a method on std::expected in C++23 — use those directly.
//
template <typename StatusT>
    requires requires(StatusT const& s) { s.has_value(); }
[[nodiscard]] constexpr auto is_success(StatusT const& status) noexcept -> bool
{
    return status.has_value();
}

template <typename StatusT>
    requires requires(StatusT const& s) { s.has_value(); }
[[nodiscard]] constexpr auto is_failure(StatusT const& status) noexcept -> bool
{
    return !status.has_value();
}

//
// Error forwarding — extract the error_code from any std::expected<T, error_code>
// and return it as std::unexpected so it can implicitly convert back into a
// differently-typed Status or StatusValue. The source must be in error state.
//
/// Example:
/// \code
///   StatusValue<size_t> load_data() {
///       auto result = parse_header();  // Returns StatusValue<Header>
///       if (!result) {
///           return forward_failure(result);
///       }
///       // ... continue processing
///   }
/// \endcode
template <typename T = void>
[[nodiscard]] constexpr auto forward_failure(std::expected<T, std::error_code> const& v) noexcept
{
    auto const has_value = v.has_value();
    STATUSBAR_ASSERT(!has_value && "forward_failure called on a success value");
    return std::unexpected(v.error());
}

}  // namespace statusbar
