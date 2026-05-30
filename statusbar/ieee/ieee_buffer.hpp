#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee_base.hpp"
#include "statusbar/status/status.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <system_error>
#include <type_traits>

namespace statusbar::ieee {

// Concept for IEEE network-ordered types
template <typename T>
concept IeeeNetworkType = is_ieee_ordered_uint<std::remove_cvref_t<T>>::value;

//
// Single value load/store
//

/// Load a single IeeeOrderedUInt value from a buffer
/// Loads network-ordered bytes directly without conversion
/// @param buf Source buffer containing network-ordered bytes
/// @param result Pointer to the IeeeOrderedUInt value to populate
template <IeeeNetworkType T>
[[nodiscard]] constexpr auto load(std::span<uint8_t const> const buf, T* const result) noexcept -> StatusValue<size_t>
{
    constexpr auto required_len = sizeof(T);
    if (buf.size() < required_len) {
        return failure(BufferError::insufficient_data);
    }
    span_copy(result->span(), buf.subspan(0, required_len));
    return success(required_len);
}

/// Store a single IeeeOrderedUInt value to a buffer
/// Stores network-ordered bytes directly
/// @param buf Destination buffer for network-ordered bytes
/// @param value The IeeeOrderedUInt value to store
template <IeeeNetworkType T>
[[nodiscard]] constexpr auto store(std::span<uint8_t> const buf, T const& value) noexcept -> StatusValue<size_t>
{
    constexpr auto required_len = sizeof(T);
    if (buf.size() < required_len) {
        return failure(BufferError::insufficient_space);
    }
    span_copy(buf.subspan(0, required_len), value.span());
    return success(required_len);
}

//
// Array load/store
//

/// Load an array of IeeeOrderedUInt values from a buffer
/// @param buf Source buffer containing network-ordered bytes
/// @param result Pointer to the array to populate
template <IeeeNetworkType T, std::size_t N>
[[nodiscard]] constexpr auto load(std::span<uint8_t const> const buf, std::array<T, N>* const result) noexcept
    -> StatusValue<size_t>
{
    size_t offset = 0;
    for (auto& elem : *result) {
        auto status = load(buf.subspan(offset), &elem);
        if (!status) {
            return status;
        }
        offset += *status;
    }
    return offset;
}

/// Store an array of IeeeOrderedUInt values to a buffer
/// @param buf Destination buffer for network-ordered bytes
/// @param values The array of IeeeOrderedUInt values to store
template <IeeeNetworkType T, std::size_t N>
[[nodiscard]] constexpr auto store(std::span<uint8_t> const buf, std::array<T, N> const& values) noexcept -> StatusValue<size_t>
{
    size_t offset = 0;
    for (auto const& elem : values) {
        auto status = store(buf.subspan(offset), elem);
        if (!status) {
            return status;
        }
        offset += *status;
    }
    return offset;
}

//
// Span load/store
//

/// Load a span of IeeeOrderedUInt values from a buffer
/// @param buf Source buffer containing network-ordered bytes
/// @param result Span of IeeeOrderedUInt values to populate
template <IeeeNetworkType T, std::size_t N>
[[nodiscard]] constexpr auto load(std::span<uint8_t const> const buf, std::span<T, N> result) noexcept -> StatusValue<size_t>
{
    size_t offset = 0;
    for (auto& elem : result) {
        auto status = load(buf.subspan(offset), &elem);
        if (!status) {
            return status;
        }
        offset += *status;
    }
    return offset;
}

/// Store a span of IeeeOrderedUInt values to a buffer
/// @param buf Destination buffer for network-ordered bytes
/// @param values Span of IeeeOrderedUInt values to store
template <IeeeNetworkType T, std::size_t N>
[[nodiscard]] constexpr auto store(std::span<uint8_t> const buf, std::span<T const, N> values) noexcept -> StatusValue<size_t>
{
    size_t offset = 0;
    for (auto const& elem : values) {
        auto status = store(buf.subspan(offset), elem);
        if (!status) {
            return status;
        }
        offset += *status;
    }
    return offset;
}

}  // namespace statusbar::ieee
