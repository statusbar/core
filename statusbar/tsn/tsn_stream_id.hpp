#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

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

namespace statusbar::tsn {

///
/// IEEE 802.1Q StreamId.
/// Defined in IEEE Std 802.1Q-2014 Clause 35.2.2.8.2 StreamID.
/// An 8-byte identifier consisting of a 6-byte EUI-48 system address and a 2-byte unique_id.
///
/// Wire format (network byte order):
/// - Bytes 0-5: EUI-48 system address
/// - Bytes 6-7: Unique identifier (uint16_t)
///
class StreamId
{
  public:
    static constexpr std::size_t LENGTH = 8U;
    static constexpr std::size_t UNIQUE_ID_BITS = 16U;

    ///
    /// Default constructor - initializes to all zeros (unset state).
    ///
    constexpr StreamId() noexcept = default;

    ///
    /// Construct a StreamId with specified system address and unique_id.
    ///
    /// \param system_address The EUI-48 system address.
    /// \param unique_id The 16-bit unique identifier.
    ///
    constexpr StreamId(ieee::Eui48 const& system_address, std::uint16_t const unique_id) noexcept
        : system_address_(system_address)
        , unique_id_(unique_id)
    {}

    ///
    /// Get the size of the StreamId in bytes.
    ///
    /// \return The size (always 8 bytes).
    ///
    [[nodiscard]] static constexpr auto size() noexcept -> std::size_t { return LENGTH; }

    ///
    /// Check if the StreamId has a valid system address.
    /// A StreamId is considered set if the system address is not all zeros and not all 0xFF.
    ///
    /// \return true if the system address is set.
    ///
    [[nodiscard]] constexpr auto is_set() const noexcept -> bool { return system_address_.is_set(); }

    ///
    /// Get the system address.
    ///
    /// \return The EUI-48 system address.
    ///
    [[nodiscard]] constexpr auto get_system_address() const noexcept -> ieee::Eui48 { return system_address_; }

    ///
    /// Set the system address.
    ///
    /// \param value The EUI-48 system address to set.
    ///
    constexpr auto set_system_address(ieee::Eui48 const& value) noexcept { system_address_ = value; }

    ///
    /// Get the unique identifier.
    ///
    /// \return The 16-bit unique identifier.
    ///
    [[nodiscard]] constexpr auto get_unique_id() const noexcept -> std::uint16_t { return unique_id_; }

    ///
    /// Set the unique identifier.
    ///
    /// \param value The 16-bit unique identifier to set.
    ///
    constexpr auto set_unique_id(std::uint16_t const value) noexcept { unique_id_ = value; }

    ///
    /// Increment the unique identifier.
    ///
    constexpr auto increment_unique_id() noexcept { unique_id_ = unique_id_ + 1; }

    ///
    /// Convert the StreamId to a 64-bit unsigned integer.
    /// The system address occupies the upper 48 bits, and the unique_id occupies the lower 16
    /// bits.
    ///
    /// \return The 64-bit representation of the StreamId.
    ///
    [[nodiscard]] constexpr auto to_uint64() const noexcept -> std::uint64_t
    {
        auto const system_address_part = system_address_.to_uint64();
        return (system_address_part << UNIQUE_ID_BITS) | unique_id_;
    }

    ///
    /// Set the StreamId from a 64-bit unsigned integer.
    /// The upper 48 bits are used for the system address, and the lower 16 bits for the
    /// unique_id.
    ///
    /// \param value The 64-bit value to convert from.
    /// \return Reference to this StreamId for chaining.
    ///
    constexpr auto from_uint64(std::uint64_t const value) noexcept -> StreamId&
    {
        auto const system_address_part = value >> UNIQUE_ID_BITS;
        system_address_.from_uint64(system_address_part);
        unique_id_ = static_cast<std::uint16_t>(value & 0xFFFFU);
        return *this;
    }

    ///
    /// Three-way comparison operator for StreamId.
    ///
    /// \param rhs The right-hand side StreamId to compare with.
    /// \return Strong ordering comparison result.
    ///
    auto operator<=>(StreamId const& rhs) const noexcept -> std::strong_ordering = default;

  private:
    ieee::Eui48 system_address_{};  ///< EUI-48 system address
    ieee::doublet_t unique_id_{0};  ///< 16-bit unique identifier (network byte order)
};

///
/// Get the wire size of a StreamId in bytes.
///
/// \param stream_id The StreamId to get the size of.
/// \return The size in bytes (always 8).
///
[[nodiscard]] constexpr auto wire_size(StreamId const& stream_id) noexcept -> std::size_t
{
    return StreamId::LENGTH;
}

///
/// Load a StreamId from a buffer without bounds checking.
/// This is the unchecked (fast path) deserialization function.
///
/// \param buffer The buffer to load from (must have at least 8 bytes).
/// \param stream_id Pointer to the StreamId to load into.
/// \return The number of bytes consumed (always 8).
///
[[nodiscard]] inline auto load_unchecked(std::span<std::uint8_t const> buffer, StreamId* stream_id) noexcept -> std::size_t
{
    ieee::Eui48 system_address{};
    size_t pos = protocol::load_unchecked(buffer, &system_address.value);

    ieee::doublet_t unique_id = 0;
    pos += ieee::load_unchecked(buffer.subspan(pos), &unique_id);

    stream_id->set_system_address(system_address);
    stream_id->set_unique_id(static_cast<std::uint16_t>(unique_id));

    return pos;
}

///
/// Store a StreamId to a buffer without bounds checking.
/// This is the unchecked (fast path) serialization function.
///
/// \param buffer The buffer to store into (must have at least 8 bytes).
/// \param stream_id The StreamId to store.
/// \return The number of bytes written (always 8).
///
[[nodiscard]] inline auto store_unchecked(std::span<std::uint8_t> buffer, StreamId const& stream_id) noexcept -> std::size_t
{
    auto const system_address = stream_id.get_system_address();
    size_t pos = protocol::store_unchecked(buffer, system_address.value);

    ieee::doublet_t const unique_id = stream_id.get_unique_id();
    pos += ieee::store_unchecked(buffer.subspan(pos), unique_id);

    return pos;
}

///
/// Check if a StreamId can be loaded from the buffer.
///
/// \param buffer The buffer to check.
/// \param stream_id Pointer to StreamId (unused, for ADL).
/// \return StatusValue containing the number of bytes needed (8) or error.
///
[[nodiscard]] inline auto can_load(std::span<std::uint8_t const> buffer, StreamId* stream_id) noexcept -> StatusValue<std::size_t>
{
    if (buffer.size() < StreamId::LENGTH) {
        return std::unexpected(make_error_code(BufferError::insufficient_data));
    }
    return StreamId::LENGTH;
}

///
/// Check if a StreamId can be stored to the buffer.
///
/// \param buffer The buffer to check.
/// \param stream_id The StreamId to store (unused, for size calculation).
/// \return StatusValue containing the number of bytes needed (8) or error.
///
[[nodiscard]] inline auto can_store(std::span<std::uint8_t> buffer, StreamId const& stream_id) noexcept -> StatusValue<std::size_t>
{
    if (buffer.size() < StreamId::LENGTH) {
        return std::unexpected(make_error_code(BufferError::insufficient_space));
    }
    return StreamId::LENGTH;
}

///
/// Load a StreamId from a buffer with bounds checking.
///
/// \param buffer The buffer to load from.
/// \param stream_id Pointer to the StreamId to load into.
/// \return StatusValue containing bytes consumed or error.
///
[[nodiscard]] inline auto load(std::span<std::uint8_t const> buffer, StreamId* stream_id) noexcept -> StatusValue<std::size_t>
{
    auto const check_result = can_load(buffer, stream_id);
    if (!check_result) {
        return check_result;
    }
    return load_unchecked(buffer, stream_id);
}

///
/// Store a StreamId to a buffer with bounds checking.
///
/// \param buffer The buffer to store into.
/// \param stream_id The StreamId to store.
/// \return StatusValue containing bytes written or error.
///
[[nodiscard]] inline auto store(std::span<std::uint8_t> buffer, StreamId const& stream_id) noexcept -> StatusValue<std::size_t>
{
    auto const check_result = can_store(buffer, stream_id);
    if (!check_result) {
        return check_result;
    }
    return store_unchecked(buffer, stream_id);
}

/// Convert StreamId to string (e.g., "aa:bb:cc:dd:ee:ff:0001").
///
/// \param stream_id The StreamId to convert.
/// \return The string representation.
[[nodiscard]] auto to_string(StreamId const& stream_id) -> std::string;

/// Parse StreamId from string.
/// Accepts formats: "aa:bb:cc:dd:ee:ff:0001", "aa-bb-cc-dd-ee-ff-0001"
/// Also accepts: "aa:bb:cc:dd:ee:ff.0001" or "aa:bb:cc:dd:ee:ff/1"
///
/// \param str The string to parse.
/// \return StreamId on success, nullopt on parse error.
[[nodiscard]] auto from_string(std::string_view str, StreamId* /*tag*/) noexcept -> std::optional<StreamId>;

/// Convenience overload without tag pointer.
///
/// \param str The string to parse.
/// \return StreamId on success, nullopt on parse error.
[[nodiscard]] auto stream_id_from_string(std::string_view str) noexcept -> std::optional<StreamId>;

/// ConfigParseable ADL opt-in for StreamId. Returns the same value
/// stream_id_from_string() produces, lifted into StatusValue so the
/// args::ConfigParseable concept can require StatusValue everywhere.
[[nodiscard]] inline auto config_parse(std::type_identity<StreamId> /*tag*/, std::string_view sv)
    -> ::statusbar::StatusValue<StreamId>
{
    auto parsed = stream_id_from_string(sv);
    if (!parsed) {
        return ::statusbar::failure(std::errc::invalid_argument);
    }
    return ::statusbar::success(*parsed);
}

inline auto config_format(StreamId const& v) -> std::string
{
    return to_string(v);
}

}  // namespace statusbar::tsn
