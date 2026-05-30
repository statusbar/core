#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include <concepts>
#include <limits>

namespace statusbar {

#if defined(__has_builtin)
#    if __has_builtin(__builtin_mul_overflow) && __has_builtin(__builtin_add_overflow)
#        define STATUSBAR_SAFE_ARITH_HAS_BUILTINS 1
#    endif
#endif
#ifndef STATUSBAR_SAFE_ARITH_HAS_BUILTINS
#    define STATUSBAR_SAFE_ARITH_HAS_BUILTINS 0
#endif

/// True iff `a * b` is representable in `T` without overflow. `T` must be
/// an unsigned integer type. Use to guard subsequent `a * b` arithmetic
/// when `a` and `b` come from untrusted input (wire-format size/count
/// fields, file headers, etc.).
template <std::unsigned_integral T>
[[nodiscard]] constexpr auto can_multiply(T a, T b) noexcept -> bool
{
#if STATUSBAR_SAFE_ARITH_HAS_BUILTINS
    T out{};
    return !__builtin_mul_overflow(a, b, &out);
#else
    return b == T{0} || a <= std::numeric_limits<T>::max() / b;
#endif
}

/// True iff `a + b` is representable in `T` without overflow. `T` must be
/// an unsigned integer type.
template <std::unsigned_integral T>
[[nodiscard]] constexpr auto can_add(T a, T b) noexcept -> bool
{
#if STATUSBAR_SAFE_ARITH_HAS_BUILTINS
    T out{};
    return !__builtin_add_overflow(a, b, &out);
#else
    return a <= std::numeric_limits<T>::max() - b;
#endif
}

}  // namespace statusbar
