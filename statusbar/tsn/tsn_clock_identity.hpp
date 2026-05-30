#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// TSN Clock Identity
/// IEEE 802.1AS-2020 ClockIdentity - 8-byte identifier for gPTP clocks
/// Also used by ATDECC (IEEE 1722.1) for grandmaster identification in ADP/ACMP

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/status/status.hpp"

#include <array>
#include <compare>
#include <cstdint>
#include <cstring>
#include <expected>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace statusbar::tsn {

///
/// IEEE 802.1AS ClockIdentity.
/// An 8-byte identifier used to uniquely identify a clock in a gPTP domain.
/// Also used as the grandmaster identity in ATDECC ADP and AEM messages.
///
/// Wire format: 8 bytes in network byte order (big-endian)
///
/// ClockIdentity is typically derived from a device's EUI-48 MAC address
/// by inserting 0xFFFE in the middle (modified EUI-64 format).
///
class ClockIdentity : public ieee::IeeeOrderedUInt<uint64_t>
{
  public:
    static constexpr std::size_t LENGTH = 8U;

    ///
    /// Default constructor - initializes to all zeros.
    ///
    constexpr ClockIdentity() noexcept
        : IeeeOrderedUInt<uint64_t>()
    {}

    ///
    /// Construct from a 64-bit value in host byte order.
    ///
    /// \param value The 64-bit clock identity value.
    ///
    constexpr explicit ClockIdentity(std::uint64_t value) noexcept
        : IeeeOrderedUInt<uint64_t>(value)
    {}

    ///
    /// Construct from 8 individual bytes in network byte order.
    ///
    /// \param b0 Byte 0 (most significant).
    /// \param b1 Byte 1.
    /// \param b2 Byte 2.
    /// \param b3 Byte 3.
    /// \param b4 Byte 4.
    /// \param b5 Byte 5.
    /// \param b6 Byte 6.
    /// \param b7 Byte 7 (least significant).
    ///
    constexpr ClockIdentity(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7) noexcept
        : IeeeOrderedUInt<uint64_t>(
              (static_cast<uint64_t>(b0) << 56) | (static_cast<uint64_t>(b1) << 48) | (static_cast<uint64_t>(b2) << 40) |
              (static_cast<uint64_t>(b3) << 32) | (static_cast<uint64_t>(b4) << 24) | (static_cast<uint64_t>(b5) << 16) |
              (static_cast<uint64_t>(b6) << 8) | static_cast<uint64_t>(b7))
    {}

    ///
    /// Construct from an EUI-64 address.
    /// This uses the EUI-64's host-order value directly.
    ///
    /// \param eui64 The EUI-64 address to convert from.
    ///
    constexpr explicit ClockIdentity(ieee::Eui64 const& eui64) noexcept
        : IeeeOrderedUInt<uint64_t>(eui64.get())
    {}

    ///
    /// Get the size of the ClockIdentity in bytes.
    ///
    /// \return The size (always 8 bytes).
    ///
    [[nodiscard]] static constexpr auto size() noexcept -> std::size_t { return LENGTH; }

    ///
    /// Check if the ClockIdentity is set.
    /// Per IEEE, both all-zeros and all-ones are reserved/unset values.
    ///
    /// \return true if the identity is not all-zeros and not all-ones.
    ///
    [[nodiscard]] constexpr auto is_set() const noexcept -> bool { return get() != 0 && get() != 0xFFFFFFFFFFFFFFFFULL; }

    ///
    /// Convert to a 64-bit value in host byte order.
    ///
    /// \return The 64-bit representation of the clock identity.
    ///
    [[nodiscard]] constexpr auto to_uint64() const noexcept -> std::uint64_t { return get(); }

    ///
    /// Set from a 64-bit value in host byte order.
    ///
    /// \param value The 64-bit value to convert from.
    /// \return Reference to this ClockIdentity for chaining.
    ///
    constexpr auto from_uint64(std::uint64_t value) noexcept -> ClockIdentity&
    {
        set(value);
        return *this;
    }

    ///
    /// Convert to an EUI-64 address.
    /// This uses the shared underlying byte representation.
    ///
    /// \return The equivalent EUI-64 address.
    ///
    [[nodiscard]] constexpr auto to_eui64() const noexcept -> ieee::Eui64
    {
        auto const s = span();
        return ieee::Eui64{s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]};
    }

    ///
    /// Set from an EUI-64 address.
    ///
    /// \param eui64 The EUI-64 address to convert from.
    /// \return Reference to this ClockIdentity for chaining.
    ///
    constexpr auto from_eui64(ieee::Eui64 const& eui64) noexcept -> ClockIdentity&
    {
        set(eui64.get());
        return *this;
    }

    ///
    /// Create a ClockIdentity from an EUI-48 using modified EUI-64 format.
    /// Inserts 0xFFFE in the middle of the EUI-48 to create an 8-byte identity.
    ///
    /// \param eui48 The EUI-48 address to convert from.
    /// \return The ClockIdentity in modified EUI-64 format.
    ///
    [[nodiscard]] static constexpr auto from_eui48(ieee::Eui48 const& eui48) noexcept -> ClockIdentity
    {
        return ClockIdentity{
            std::get<0>(eui48.value),
            std::get<1>(eui48.value),
            std::get<2>(eui48.value),
            0xFF,
            0xFE,
            std::get<3>(eui48.value),
            std::get<4>(eui48.value),
            std::get<5>(eui48.value)};
    }
};

static_assert(sizeof(ClockIdentity) == 8, "ClockIdentity must be exactly 8 bytes");

///
/// Get the wire size of a ClockIdentity in bytes.
///
/// \param id The ClockIdentity (unused, for ADL).
/// \return The size in bytes (always 8).
///
[[nodiscard]] constexpr auto wire_size(ClockIdentity const& id) noexcept -> std::size_t
{
    (void)id;
    return ClockIdentity::LENGTH;
}

///
/// Load a ClockIdentity from a buffer without bounds checking.
///
/// \param buffer The buffer to load from (must have at least 8 bytes).
/// \param id Pointer to the ClockIdentity to load into.
/// \return The number of bytes consumed (always 8).
///
[[nodiscard]] inline auto load_unchecked(std::span<std::uint8_t const> buffer, ClockIdentity* id) noexcept -> std::size_t
{
    auto const s = id->span();
    span_copy(s, buffer.subspan(0, ClockIdentity::LENGTH));
    return ClockIdentity::LENGTH;
}

///
/// Store a ClockIdentity to a buffer without bounds checking.
///
/// \param buffer The buffer to store into (must have at least 8 bytes).
/// \param id The ClockIdentity to store.
/// \return The number of bytes written (always 8).
///
[[nodiscard]] inline auto store_unchecked(std::span<std::uint8_t> buffer, ClockIdentity const& id) noexcept -> std::size_t
{
    auto const s = id.span();
    span_copy(buffer, s);
    return ClockIdentity::LENGTH;
}

///
/// Check if a ClockIdentity can be loaded from the buffer.
///
/// \param buffer The buffer to check.
/// \param id Pointer to ClockIdentity (unused, for ADL).
/// \return StatusValue containing the number of bytes needed (8) or error.
///
[[nodiscard]] inline auto can_load(std::span<std::uint8_t const> buffer, ClockIdentity* id) noexcept
    -> statusbar::StatusValue<std::size_t>
{
    (void)id;
    if (buffer.size() < ClockIdentity::LENGTH) {
        return std::unexpected(statusbar::make_error_code(statusbar::BufferError::insufficient_data));
    }
    return ClockIdentity::LENGTH;
}

///
/// Check if a ClockIdentity can be stored to the buffer.
///
/// \param buffer The buffer to check.
/// \param id The ClockIdentity to store (unused, for size calculation).
/// \return StatusValue containing the number of bytes needed (8) or error.
///
[[nodiscard]] inline auto can_store(std::span<std::uint8_t> buffer, ClockIdentity const& id) noexcept
    -> statusbar::StatusValue<std::size_t>
{
    (void)id;
    if (buffer.size() < ClockIdentity::LENGTH) {
        return std::unexpected(statusbar::make_error_code(statusbar::BufferError::insufficient_space));
    }
    return ClockIdentity::LENGTH;
}

///
/// Load a ClockIdentity from a buffer with bounds checking.
///
/// \param buffer The buffer to load from.
/// \param id Pointer to the ClockIdentity to load into.
/// \return StatusValue containing bytes consumed or error.
///
[[nodiscard]] inline auto load(std::span<std::uint8_t const> buffer, ClockIdentity* id) noexcept
    -> statusbar::StatusValue<std::size_t>
{
    auto const check_result = can_load(buffer, id);
    if (!check_result) {
        return check_result;
    }
    return load_unchecked(buffer, id);
}

///
/// Store a ClockIdentity to a buffer with bounds checking.
///
/// \param buffer The buffer to store into.
/// \param id The ClockIdentity to store.
/// \return StatusValue containing bytes written or error.
///
[[nodiscard]] inline auto store(std::span<std::uint8_t> buffer, ClockIdentity const& id) noexcept
    -> statusbar::StatusValue<std::size_t>
{
    auto const check_result = can_store(buffer, id);
    if (!check_result) {
        return check_result;
    }
    return store_unchecked(buffer, id);
}

/// Convert ClockIdentity to string (e.g., "aa:bb:cc:dd:ee:ff:gg:hh").
///
/// \param id The ClockIdentity to convert.
/// \return The string representation.
[[nodiscard]] auto to_string(ClockIdentity const& id) -> std::string;

/// Parse ClockIdentity from string.
/// Accepts formats: "aa:bb:cc:dd:ee:ff:gg:hh", "aa-bb-cc-dd-ee-ff-gg-hh"
///
/// \param str The string to parse.
/// \return ClockIdentity on success, nullopt on parse error.
[[nodiscard]] auto from_string(std::string_view str, ClockIdentity* /*tag*/) -> std::optional<ClockIdentity>;

/// Convenience overload without tag pointer.
///
/// \param str The string to parse.
/// \return ClockIdentity on success, nullopt on parse error.
[[nodiscard]] auto clock_identity_from_string(std::string_view str) -> std::optional<ClockIdentity>;

}  // namespace statusbar::tsn

// Serialization trait - ClockIdentity is a packed wire format struct
template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<statusbar::tsn::ClockIdentity> : std::true_type
{};
