#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/fmt/fmt.hpp"
#include "statusbar/ieee/ieee_base.hpp"
#include "statusbar/ieee/ieee_buffer.hpp"
#include "statusbar/status/status.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace statusbar::ieee {

// Forward declarations
struct Eui64;

///
/// IEEE EUI-48 (Extended Unique Identifier, 48-bit) address.
/// Also known as MAC-48, this is the standard 6-byte Ethernet hardware address.
///
/// The first three bytes are the OUI (Organizationally Unique Identifier),
/// and the last three bytes are assigned by the manufacturer.
///
/// Bit 0 of byte 0 is the I/G (Individual/Group) bit:
/// - 0 = unicast address
/// - 1 = multicast address
///
/// Bit 1 of byte 0 is the U/L (Universal/Local) bit:
/// - 0 = universally administered (globally unique)
/// - 1 = locally administered
///
struct Eui48
{
    constexpr static size_t LENGTH = 6;
    std::array<std::uint8_t, LENGTH> value;
    ///
    /// Default constructor - initializes all bytes to zero.
    ///
    constexpr Eui48() noexcept
        : value{}
    {}

    ///
    /// Construct an EUI-48 from 6 individual bytes in network byte order.
    ///
    /// \param b0 First byte (most significant byte of the address).
    /// \param b1 Second byte.
    /// \param b2 Third byte.
    /// \param b3 Fourth byte.
    /// \param b4 Fifth byte.
    /// \param b5 Sixth byte (least significant byte of the address).
    ///
    constexpr Eui48(
        uint8_t const b0, uint8_t const b1, uint8_t const b2, uint8_t const b3, uint8_t const b4, uint8_t const b5) noexcept
        : value{b0, b1, b2, b3, b4, b5}
    {}

    ///
    /// Get the size of the EUI-48 address in bytes.
    ///
    /// \return The size (always 6 bytes).
    ///
    [[nodiscard]] static constexpr auto size() noexcept -> std::size_t { return LENGTH; }

    ///
    /// Get the right-justified host byte order uint64_t representation of the EUI-48.
    /// The 48-bit MAC address is placed in the lower 6 bytes of the 64-bit value,
    /// with the upper 2 bytes set to zero.
    ///
    /// \return The EUI-48 as a uint64_t in host byte order.
    ///
    [[nodiscard]] constexpr auto to_uint64() const noexcept
    {
        uint64_t v = 0;
        v |= static_cast<uint64_t>(std::get<0>(value)) << (5 * 8);
        v |= static_cast<uint64_t>(std::get<1>(value)) << (4 * 8);
        v |= static_cast<uint64_t>(std::get<2>(value)) << (3 * 8);
        v |= static_cast<uint64_t>(std::get<3>(value)) << (2 * 8);
        v |= static_cast<uint64_t>(std::get<4>(value)) << (1 * 8);
        v |= static_cast<uint64_t>(std::get<5>(value)) << (0 * 8);
        return v;
    }

    ///
    /// Set the EUI-48 from a right-justified host byte order uint64_t.
    /// The lower 6 bytes of the 64-bit value are used for the MAC address.
    ///
    /// \param new_value The 64-bit value to convert from.
    /// \return Reference to this EUI-48 for chaining.
    ///
    constexpr auto from_uint64(std::uint64_t const new_value) noexcept -> Eui48&
    {
        std::get<0>(value) = static_cast<std::uint8_t>((new_value >> (5 * 8)) & 0xFFU);
        std::get<1>(value) = static_cast<std::uint8_t>((new_value >> (4 * 8)) & 0xFFU);
        std::get<2>(value) = static_cast<std::uint8_t>((new_value >> (3 * 8)) & 0xFFU);
        std::get<3>(value) = static_cast<std::uint8_t>((new_value >> (2 * 8)) & 0xFFU);
        std::get<4>(value) = static_cast<std::uint8_t>((new_value >> (1 * 8)) & 0xFFU);
        std::get<5>(value) = static_cast<std::uint8_t>((new_value >> (0 * 8)) & 0xFFU);
        return *this;
    }

    ///
    /// Check if this EUI-48 is a locally administered address.
    /// Tests bit 1 of the first octet (the U/L bit).
    ///
    /// \return true if the locally administered bit is set.
    ///
    [[nodiscard]] constexpr auto is_locally_administered() const noexcept { return (std::get<0>(value) & 0x02U) != 0U; }

    ///
    /// Check if this EUI-48 is a multicast address.
    /// Tests bit 0 of the first octet (the I/G bit).
    ///
    /// \return true if the multicast bit is set.
    ///
    [[nodiscard]] constexpr auto is_multicast() const noexcept { return (std::get<0>(value) & 0x01U) != 0U; }

    ///
    /// Three-way comparison operator for EUI-48 addresses.
    ///
    /// \param rhs The right-hand side EUI-48 to compare with.
    /// \return Strong ordering comparison result.
    ///
    auto operator<=>(Eui48 const& rhs) const noexcept -> std::strong_ordering = default;

    ///
    /// Get a span view of the EUI-48 address bytes.
    ///
    /// \return A span of the 6 bytes representing the address.
    ///
    [[nodiscard]] constexpr auto span() const noexcept -> std::span<uint8_t const> { return value; }

    /// \return A mutable span of the 6 bytes representing the address.
    [[nodiscard]] constexpr auto span() noexcept -> std::span<uint8_t> { return value; }

    ///
    /// Check if the EUI-48 is set.
    /// Per IEEE, both all-zeros and all-ones are reserved/unset values.
    ///
    /// \return true if the address is not all-zeros and not all-ones.
    ///
    [[nodiscard]] constexpr auto is_set() const noexcept
    {
        bool all_zero = true;
        bool all_ones = true;
        for (auto b : value) {
            if (b != 0x00U) {
                all_zero = false;
            }
            if (b != 0xFFU) {
                all_ones = false;
            }
        }
        return !all_zero && !all_ones;
    }

    ///
    /// Convert EUI-48 to modified EUI-64 format per IEEE standard.
    /// Inserts 0xFF and 0xFE in the middle of the MAC address.
    /// Example: AA:BB:CC:DD:EE:FF -> AA:BB:CC:FF:FE:DD:EE:FF
    ///
    /// This is the standard method for converting a MAC-48 (EUI-48) address
    /// to an EUI-64 identifier, commonly used in IPv6 link-local addresses
    /// and IEEE 1722 (AVB/TSN) protocols.
    ///
    /// \return The EUI-64 representation with 0xFFFE inserted in the middle.
    ///
    /// Convert to EUI-64 by inserting a 16-bit index in the middle.
    /// The standard modified EUI-64 uses index 0xFFFE.
    [[nodiscard]] constexpr auto to_eui64_with_index(uint16_t index = 0xFFFE) const noexcept -> Eui64;

    /// Convert to modified EUI-64 (insert 0xFFFE in the middle).
    /// Equivalent to `to_eui64_with_index(0xFFFE)`.
    [[nodiscard]] constexpr auto to_modified_eui64() const noexcept -> Eui64;
};

///
/// IEEE EUI-64 (Extended Unique Identifier, 64-bit) address.
/// This is an 8-byte identifier used in various IEEE protocols.
///
/// EUI-64 addresses can be derived from EUI-48 addresses using the
/// modified EUI-64 format (inserting 0xFFFE in the middle).
///
struct Eui64 : public IeeeOrderedUInt<uint64_t>
{
    constexpr static size_t LENGTH = 8;

    ///
    /// Default constructor - initializes all bytes to zero.
    ///
    constexpr Eui64() noexcept
        : IeeeOrderedUInt<uint64_t>()
    {}

    ///
    /// Construct an EUI-64 from 8 individual bytes in network byte order.
    ///
    /// \param b0 First byte (most significant byte of the address).
    /// \param b1 Second byte.
    /// \param b2 Third byte.
    /// \param b3 Fourth byte.
    /// \param b4 Fifth byte.
    /// \param b5 Sixth byte.
    /// \param b6 Seventh byte.
    /// \param b7 Eighth byte (least significant byte of the address).
    ///
    constexpr Eui64(
        uint8_t const b0,
        uint8_t const b1,
        uint8_t const b2,
        uint8_t const b3,
        uint8_t const b4,
        uint8_t const b5,
        uint8_t const b6,
        uint8_t const b7) noexcept
        : IeeeOrderedUInt<uint64_t>(
              (static_cast<uint64_t>(b0) << 56) | (static_cast<uint64_t>(b1) << 48) | (static_cast<uint64_t>(b2) << 40) |
              (static_cast<uint64_t>(b3) << 32) | (static_cast<uint64_t>(b4) << 24) | (static_cast<uint64_t>(b5) << 16) |
              (static_cast<uint64_t>(b6) << 8) | static_cast<uint64_t>(b7))
    {}

    ///
    /// Get the size of the EUI-64 address in bytes.
    ///
    /// \return The size (always 8 bytes).
    ///
    [[nodiscard]] static constexpr auto size() noexcept -> std::size_t { return LENGTH; }

    ///
    /// Get the host byte order uint64_t representation of the EUI-64.
    /// The 64-bit address uses all 8 bytes of the uint64_t value.
    ///
    /// \return The EUI-64 as a uint64_t in host byte order.
    ///
    [[nodiscard]] constexpr auto to_uint64() const noexcept { return get(); }

    ///
    /// Set the EUI-64 from a host byte order uint64_t.
    /// All 8 bytes of the 64-bit value are used for the address.
    ///
    /// \param new_value The 64-bit value to convert from.
    /// \return Reference to this EUI-64 for chaining.
    ///
    constexpr auto from_uint64(std::uint64_t const new_value) noexcept -> Eui64&
    {
        set(new_value);
        return *this;
    }

    ///
    /// Check if this EUI-64 is a locally administered address.
    /// Tests bit 1 of the first octet (the U/L bit).
    ///
    /// \return true if the locally administered bit is set.
    ///
    [[nodiscard]] constexpr auto is_locally_administered() const noexcept { return (span()[0] & 0x02U) != 0U; }

    ///
    /// Check if this EUI-64 is a multicast address.
    /// Tests bit 0 of the first octet (the I/G bit).
    ///
    /// \return true if the multicast bit is set.
    ///
    [[nodiscard]] constexpr auto is_multicast() const noexcept { return (span()[0] & 0x01U) != 0U; }

    ///
    /// Check if the EUI-64 is set.
    /// Per IEEE, both all-zeros and all-ones are reserved/unset values.
    ///
    /// \return true if the address is not all-zeros and not all-ones.
    ///
    [[nodiscard]] constexpr auto is_set() const noexcept { return get() != 0 && get() != 0xFFFFFFFFFFFFFFFFULL; }
};

static_assert(sizeof(Eui64) == 8, "Eui64 must be exactly 8 bytes");

/// Implementation of Eui48::to_eui64_with_index() (must be after Eui64 definition)
[[nodiscard]] constexpr auto Eui48::to_eui64_with_index(uint16_t index) const noexcept -> Eui64
{
    return Eui64{
        std::get<0>(value),
        std::get<1>(value),
        std::get<2>(value),
        static_cast<uint8_t>((index >> 8) & 0xFFU),
        static_cast<uint8_t>(index & 0xFFU),
        std::get<3>(value),
        std::get<4>(value),
        std::get<5>(value)};
}

[[nodiscard]] constexpr auto Eui48::to_modified_eui64() const noexcept -> Eui64
{
    return to_eui64_with_index(0xFFFE);
}

///
/// IEEE 802.1Q VLAN Tag structure.
///
/// The VLAN tag is a 4-byte structure inserted into Ethernet frames:
/// - TPID (Tag Protocol Identifier): 2 bytes, always 0x8100 for 802.1Q
/// - TCI (Tag Control Information): 2 bytes, containing PCP, DEI, and VID
///
/// TCI bit layout (16 bits):
/// - Bits 15-13: PCP (Priority Code Point) - 3 bits, 0-7
/// - Bit 12: DEI (Drop Eligible Indicator) - 1 bit
/// - Bits 11-0: VID (VLAN Identifier) - 12 bits, 0-4095
///
struct VlanTag
{
    static constexpr std::uint16_t ETHERTYPE = 0x8100U;  ///< 802.1Q TPID value
    static constexpr std::size_t LENGTH = 4U;            ///< Size of VLAN tag in bytes

    // TCI bit field masks and shifts
    static constexpr std::uint16_t PCP_SHIFT = 13U;
    static constexpr std::uint16_t PCP_MASK = 0x7U;
    static constexpr std::uint16_t DEI_SHIFT = 12U;
    static constexpr std::uint16_t DEI_MASK = 0x1U;
    static constexpr std::uint16_t VID_SHIFT = 0U;
    static constexpr std::uint16_t VID_MASK = 0xfffU;

    doublet_t tpid{0U};  ///< Tag Protocol Identifier (0x8100 for 802.1Q)
    doublet_t tci{0U};   ///< Tag Control Information (PCP + DEI + VID)

    VlanTag() = default;
    VlanTag(VlanTag const&) = default;
    auto operator=(VlanTag const&) -> VlanTag& = default;
    VlanTag(VlanTag&&) = default;
    auto operator=(VlanTag&&) -> VlanTag& = default;
    ~VlanTag() = default;

    /// Construct a VLAN tag with the specified parameters.
    /// \param vlan_id The 12-bit VLAN ID (0-4095).
    /// \param dei The Drop Eligible Indicator.
    /// \param pcp The 3-bit Priority Code Point (0-7).
    VlanTag(uint16_t vlan_id, bool dei, uint8_t pcp) noexcept
        : tpid{ETHERTYPE}
        , tci{static_cast<uint16_t>(
              (static_cast<uint16_t>(pcp & PCP_MASK) << PCP_SHIFT) |
              ((static_cast<uint16_t>(dei ? 1U : 0U) & DEI_MASK) << DEI_SHIFT) |
              (static_cast<uint16_t>(vlan_id & VID_MASK) << VID_SHIFT))}
    {}

    /// Factory method for creating VlanTag from raw values (for testing and deserialization).
    /// \param raw_tpid The raw TPID value.
    /// \param raw_tci The raw TCI value.
    /// \return A VlanTag with the specified raw values.
    [[nodiscard]] static constexpr auto from_raw(uint16_t raw_tpid, uint16_t raw_tci) noexcept
    {
        VlanTag vlan{};
        vlan.tpid = raw_tpid;
        vlan.tci = raw_tci;
        return vlan;
    }

    /// Check if this VLAN tag is set/valid (TPID == 0x8100).
    /// \return true if this is a valid 802.1Q VLAN tag.
    [[nodiscard]] constexpr auto is_set() const noexcept { return tpid == ETHERTYPE; }

    /// Get the VLAN Identifier (VID).
    /// \return The 12-bit VLAN ID (0-4095), or 0 if tag is not set.
    [[nodiscard]] constexpr auto get_vid() const noexcept
    {
        if (!is_set()) {
            return static_cast<uint16_t>(0U);
        }
        return static_cast<uint16_t>((tci >> VID_SHIFT) & VID_MASK);
    }

    /// Get the Priority Code Point (PCP).
    /// \return The 3-bit priority (0-7), or 0 if tag is not set.
    [[nodiscard]] constexpr auto get_pcp() const noexcept
    {
        if (!is_set()) {
            return static_cast<uint16_t>(0U);
        }
        return static_cast<uint16_t>((tci >> PCP_SHIFT) & PCP_MASK);
    }

    /// Get the Drop Eligible Indicator (DEI).
    /// \return true if the frame is drop eligible, false otherwise or if tag is not set.
    [[nodiscard]] constexpr auto get_dei() const noexcept
    {
        if (!is_set()) {
            return false;
        }
        return static_cast<bool>(((tci >> DEI_SHIFT) & DEI_MASK) != 0);
    }

    /// Set the VLAN Identifier (VID).
    /// Automatically sets TPID to 0x8100 if not already set.
    /// \param vid The 12-bit VLAN ID (0-4095).
    constexpr auto set_vid(uint16_t vid) noexcept
    {
        // Set TPID if not already set
        if (!is_set()) {
            tpid = ETHERTYPE;
        }
        // Clear VID bits and set new value
        tci = (tci & ~(VID_MASK << VID_SHIFT)) | ((vid & VID_MASK) << VID_SHIFT);
    }

    /// Set the Priority Code Point (PCP).
    /// Automatically sets TPID to 0x8100 if not already set.
    /// \param pcp The 3-bit priority (0-7).
    constexpr auto set_pcp(uint8_t pcp) noexcept
    {
        // Set TPID if not already set
        if (!is_set()) {
            tpid = ETHERTYPE;
        }
        // Clear PCP bits and set new value
        tci = (tci & ~(PCP_MASK << PCP_SHIFT)) | ((pcp & PCP_MASK) << PCP_SHIFT);
    }

    /// Set the Drop Eligible Indicator (DEI).
    /// Automatically sets TPID to 0x8100 if not already set.
    /// \param dei true to mark as drop eligible, false otherwise.
    constexpr auto set_dei(bool dei) noexcept
    {
        // Set TPID if not already set
        if (!is_set()) {
            tpid = ETHERTYPE;
        }
        // Clear DEI bit and set new value
        tci = (tci & ~(DEI_MASK << DEI_SHIFT)) | ((dei ? 1U : 0U) << DEI_SHIFT);
    }

    /// Three-way comparison operator.
    auto operator<=>(VlanTag const& rhs) const noexcept -> std::strong_ordering = default;
};

/// Calculate the TCI (Tag Control Information) value from individual fields.
/// \param vid The 12-bit VLAN ID (0-4095).
/// \param dei The Drop Eligible Indicator.
/// \param pcp The 3-bit Priority Code Point (0-7).
/// \return The combined 16-bit TCI value.
[[nodiscard]] constexpr auto calculate_vlan_tci(uint16_t vid, bool dei, uint8_t pcp) noexcept
{
    return static_cast<uint16_t>(
        ((pcp & VlanTag::PCP_MASK) << VlanTag::PCP_SHIFT) | (((dei ? 1U : 0U) & VlanTag::DEI_MASK) << VlanTag::DEI_SHIFT) |
        ((vid & VlanTag::VID_MASK) << VlanTag::VID_SHIFT));
}

/// Create a VLAN tag with the specified parameters.
/// \param vid The 12-bit VLAN ID (0-4095).
/// \param dei The Drop Eligible Indicator.
/// \param pcp The 3-bit Priority Code Point (0-7).
/// \return A VlanTag with TPID set to 0x8100 and the calculated TCI.
[[nodiscard]] constexpr auto make_vlan_tag(uint16_t vid, bool dei, uint8_t pcp) noexcept -> VlanTag
{
    return VlanTag(vid, dei, pcp);
}

/// Create an empty (unset) VLAN tag.
/// \return A VlanTag with TPID and TCI both set to 0.
[[nodiscard]] constexpr auto make_empty_vlan_tag() noexcept -> VlanTag
{
    return VlanTag();
}

/// Check if a VLAN tag is set (has a valid TPID).
/// \param vlan_tag The VLAN tag to check.
/// \return true if the VLAN tag is set (TPID == 0x8100).
[[nodiscard]] constexpr auto is_tagged(VlanTag const& vlan_tag) noexcept -> bool
{
    return vlan_tag.is_set();
}

///
/// IEEE 802.3 Ethernet Frame structure.
///
/// This structure represents the Ethernet II frame header with optional
/// 802.1Q VLAN tagging support. The frame can be either:
/// - Untagged: 14 bytes (dest_mac + src_mac + ethertype)
/// - VLAN tagged: 18 bytes (dest_mac + src_mac + vlan_tag + ethertype)
///
struct EthernetFrame
{
    Eui48 dest_mac;       ///< Destination MAC address
    Eui48 src_mac;        ///< Source MAC address
    VlanTag vlan_tag;     ///< Optional 802.1Q VLAN tag (tpid==0 if not present)
    doublet_t ethertype;  ///< Ethertype/Length field

    /// Check if this Ethernet frame is valid.
    /// Validates source MAC, VLAN tag (if present), and ethertype.
    /// \return true if the frame header is valid.
    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
    {
        // Source MAC should be set (not all zeros or broadcast)
        if (!src_mac.is_set()) {
            return false;
        }
        // Ethertype should be > 0x0600 for Ethernet II
        // (values <= 0x0600 are 802.3 length fields)
        if (ethertype <= 0x0600) {
            return false;
        }
        return true;
    }
};

// Compile-time layout verification for safe memcpy serialization
static_assert(sizeof(EthernetFrame) == 18, "EthernetFrame must be exactly 18 bytes with no padding");
static_assert(alignof(EthernetFrame) <= 2, "EthernetFrame alignment must not exceed 2 bytes");
static_assert(offsetof(EthernetFrame, dest_mac) == 0, "dest_mac must be at offset 0");
static_assert(offsetof(EthernetFrame, src_mac) == 6, "src_mac must be at offset 6");
static_assert(offsetof(EthernetFrame, vlan_tag) == 12, "vlan_tag must be at offset 12");
static_assert(offsetof(EthernetFrame, ethertype) == 16, "ethertype must be at offset 16");

/// Convert Eui48 to string (e.g., "aa:bb:cc:dd:ee:ff"). Heap-free.
/// \param mac The EUI-48 address to convert.
[[nodiscard]] auto to_string(Eui48 const& mac) -> ::statusbar::fmt::fixed_str<17>;

/// Convert Eui64 to string (e.g., "aa:bb:cc:dd:ee:ff:00:11"). Heap-free.
/// \param mac The EUI-64 address to convert.
[[nodiscard]] auto to_string(Eui64 const& mac) -> ::statusbar::fmt::fixed_str<23>;

/// Trim leading and trailing whitespace (spaces and tabs) from a string_view.
/// \param str The string view to trim.
/// \return The trimmed string view.
[[nodiscard]] constexpr auto trim_whitespace(std::string_view str) noexcept -> std::string_view
{
    while (!str.empty() && (str.front() == ' ' || str.front() == '\t')) {
        str.remove_prefix(1);
    }
    while (!str.empty() && (str.back() == ' ' || str.back() == '\t')) {
        str.remove_suffix(1);
    }
    return str;
}

/// Parse a hex digit character to its value
/// \param c The hexadecimal character ('0'-'9', 'a'-'f', 'A'-'F').
/// \return 0-15 on success, 255 on invalid character
[[nodiscard]] constexpr auto parse_hex_digit(char const c) noexcept -> uint8_t
{
    if (c >= '0' && c <= '9') {
        return static_cast<uint8_t>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<uint8_t>(c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<uint8_t>(c - 'A' + 10);
    }
    return 255;
}

/// Parse Eui48 from string
/// Accepts formats: "aa:bb:cc:dd:ee:ff", "aa-bb-cc-dd-ee-ff", "aabbccddeeff"
/// \param str The string to parse.
/// \return Eui48 on success, nullopt on parse error
[[nodiscard]] auto from_string(std::string_view str, Eui48* /*tag*/) noexcept -> std::optional<Eui48>;

/// Parse Eui64 from string
/// Accepts formats: "aa:bb:cc:dd:ee:ff:00:11", "aa-bb-cc-dd-ee-ff-00-11", "aabbccddeeff0011"
/// \param str The string to parse.
/// \return Eui64 on success, nullopt on parse error
[[nodiscard]] auto from_string(std::string_view str, Eui64* /*tag*/) noexcept -> std::optional<Eui64>;

/// Convenience overloads without tag pointer
/// \param str The string to parse as an EUI-48 address.
[[nodiscard]] inline auto eui48_from_string(std::string_view str) noexcept -> std::optional<Eui48>
{
    return from_string(str, static_cast<Eui48*>(nullptr));
}

/// \param str The string to parse as an EUI-64 address.
[[nodiscard]] inline auto eui64_from_string(std::string_view str) noexcept -> std::optional<Eui64>
{
    return from_string(str, static_cast<Eui64*>(nullptr));
}

/// ConfigParseable ADL opt-in for Eui48. Lifts eui48_from_string() into
/// StatusValue so the args::ConfigParseable concept can require StatusValue
/// uniformly.
[[nodiscard]] inline auto config_parse(std::type_identity<Eui48> /*tag*/, std::string_view sv) -> ::statusbar::StatusValue<Eui48>
{
    auto parsed = eui48_from_string(sv);
    if (!parsed) {
        return ::statusbar::failure(std::errc::invalid_argument);
    }
    return ::statusbar::success(*parsed);
}

inline auto config_format(Eui48 const& v) -> std::string
{
    return std::string{to_string(v).view()};
}

/// ConfigParseable ADL opt-in for Eui64. Same pattern as the Eui48 overload.
[[nodiscard]] inline auto config_parse(std::type_identity<Eui64> /*tag*/, std::string_view sv) -> ::statusbar::StatusValue<Eui64>
{
    auto parsed = eui64_from_string(sv);
    if (!parsed) {
        return ::statusbar::failure(std::errc::invalid_argument);
    }
    return ::statusbar::success(*parsed);
}

inline auto config_format(Eui64 const& v) -> std::string
{
    return std::string{to_string(v).view()};
}

}  // namespace statusbar::ieee

template <>
struct statusbar::traits::is_serializable_fixed_struct<statusbar::ieee::Eui48> : std::true_type
{};

template <>
struct statusbar::traits::is_serializable_fixed_struct<statusbar::ieee::Eui64> : std::true_type
{};

template <>
struct statusbar::traits::is_serializable_variable_struct<statusbar::ieee::VlanTag> : std::true_type
{};

template <>
struct statusbar::traits::is_serializable_variable_struct<statusbar::ieee::EthernetFrame> : std::true_type
{};

// Export free functions for ADL to work with modules
namespace statusbar::ieee {

/// Load an EUI-48 address from a buffer without bounds checking.
/// \param buf Source buffer containing the 6-byte EUI-48.
/// \param item Pointer to the Eui48 to populate.
[[nodiscard]] inline auto load_unchecked(std::span<uint8_t const> const buf, Eui48* const item) noexcept
{
    return protocol::load_unchecked(buf, &item->value);
}

/// Store an EUI-48 address to a buffer without bounds checking.
/// \param buf Destination buffer for the 6-byte EUI-48.
/// \param item The Eui48 to store.
[[nodiscard]] inline auto store_unchecked(std::span<uint8_t> const buf, Eui48 const& item) noexcept
{
    return protocol::store_unchecked(buf, item.value);
}

/// Load an EUI-64 address from a buffer without bounds checking.
/// \param buf Source buffer containing the 8-byte EUI-64.
/// \param item Pointer to the Eui64 to populate.
[[nodiscard]] inline auto load_unchecked(std::span<uint8_t const> const buf, Eui64* const item) noexcept
{
    span_copy(item->span(), buf.subspan(0, 8));
    return size_t{8};
}

/// Store an EUI-64 address to a buffer without bounds checking.
/// \param buf Destination buffer for the 8-byte EUI-64.
/// \param item The Eui64 to store.
[[nodiscard]] inline auto store_unchecked(std::span<uint8_t> const buf, Eui64 const& item) noexcept
{
    span_copy(buf.subspan(0, 8), item.span());
    return size_t{8};
}

///
/// Get the wire size of a VLAN tag.
/// Returns 4 bytes if the tag is set (tpid == 0x8100), otherwise 0 bytes.
///
/// \param item The VlanTag to get the size of.
/// \return The number of bytes required (0 or 4).
///
[[nodiscard]] constexpr auto wire_size(VlanTag const& item) noexcept -> size_t
{
    return item.is_set() ? VlanTag::LENGTH : 0;
}

///
/// Check if a buffer has sufficient data to load a VLAN tag.
/// Peeks at the TPID field to determine if a VLAN tag is present.
///
/// \param buf The buffer to check for available data.
/// \param item Pointer to VlanTag (unused, for ADL compatibility).
/// \return Success with byte count (0 or 4) if sufficient data, or error if insufficient.
///
/// \note If buffer has at least 2 bytes and first doublet is 0x8100, requires 4 bytes total.
///       Otherwise, returns success with 0 bytes (no VLAN tag present).
///
[[nodiscard]] auto can_load(std::span<uint8_t const> buf, VlanTag const* item) noexcept -> StatusValue<size_t>;

///
/// Check if a buffer has sufficient space to store a VLAN tag.
/// Returns 0 if tag is not set, or 4 if tag is set.
///
/// \param buf The buffer to check for available space.
/// \param item The VlanTag to be stored.
/// \return Success with byte count (0 or 4) if sufficient space, or error if insufficient.
///
[[nodiscard]] inline auto can_store(std::span<uint8_t> const buf, VlanTag const& item) noexcept -> StatusValue<size_t>
{
    size_t const required_size = item.is_set() ? VlanTag::LENGTH : 0;
    if (buf.size() < required_size) {
        return failure(BufferError::insufficient_space);
    }
    return success(required_size);
}

///
/// Deserialize a VLAN tag from a buffer without bounds checking.
/// If the TPID is 0x8100, parses the 4-byte VLAN tag. Otherwise, resets to untagged (tpid=0, tci=0).
///
/// \param buf The buffer containing the VLAN tag data (must have sufficient space if tag present).
/// \param item Pointer to the VlanTag structure to populate.
/// \return The number of bytes consumed (0 if no tag, 4 if VLAN tag present).
///
/// \note Caller must ensure the buffer has sufficient space before calling.
///
[[nodiscard]] auto load_unchecked(std::span<uint8_t const> buf, VlanTag* item) noexcept -> size_t;

///
/// Serialize a VLAN tag to a buffer without bounds checking.
/// If the tag is set (tpid == 0x8100), writes 4 bytes. Otherwise, writes 0 bytes.
///
/// \param buf The buffer to write the VLAN tag data to (must have sufficient space if tag is set).
/// \param item The VlanTag to serialize.
/// \return The number of bytes written (0 or 4).
///
/// \note Caller must ensure the buffer has sufficient space before calling.
///
[[nodiscard]] auto store_unchecked(std::span<uint8_t> buf, VlanTag const& item) noexcept -> size_t;

///
/// Get the wire size of an Ethernet frame.
/// Returns 14 bytes for untagged frames, 18 bytes for VLAN tagged frames.
///
/// \param item The EthernetFrame to get the size of.
/// \return The number of bytes required (14 or 18).
///
[[nodiscard]] constexpr auto wire_size(EthernetFrame const& item) noexcept -> size_t
{
    return 14 + wire_size(item.vlan_tag);
}

///
/// Check if a buffer has sufficient data to load an Ethernet frame.
/// Peeks at the ethertype field to determine if the frame is VLAN tagged.
///
/// \param buf The buffer to check for available data.
/// \param item Pointer to EthernetFrame (unused, for ADL compatibility).
/// \return Success with byte count (14 or 18) if sufficient data, or error if insufficient.
///
/// \note Requires minimum 14 bytes. If ethertype at offset 12 is 0x8100, requires 18 bytes total.
///
[[nodiscard]] auto can_load(std::span<uint8_t const> buf, EthernetFrame const* item) noexcept -> StatusValue<size_t>;

///
/// Check if a buffer has sufficient space to store an Ethernet frame.
/// Returns the number of bytes required based on whether the frame has a VLAN tag.
///
/// \param buf The buffer to check for available space.
/// \param item The EthernetFrame to be stored.
/// \return Success with byte count (14 or 18) if sufficient space, or error if insufficient.
///
/// \note VLAN tagged frames require 18 bytes, untagged frames require 14 bytes.
///
[[nodiscard]] inline auto can_store(std::span<uint8_t> const buf, EthernetFrame const& item) noexcept -> StatusValue<size_t>
{
    size_t const required_size = 14 + wire_size(item.vlan_tag);
    if (buf.size() < required_size) {
        return failure(BufferError::insufficient_space);
    }
    return success(required_size);
}

///
/// Deserialize an Ethernet frame from a buffer without bounds checking.
/// Handles both VLAN tagged (802.1Q) and untagged frames based on ethertype.
///
/// \param buf The buffer containing the Ethernet frame data (must have sufficient space).
/// \param item Pointer to the EthernetFrame structure to populate.
/// \return The number of bytes consumed (14 for untagged, 18 for VLAN tagged).
///
/// \note If ethertype is 0x8100, the frame is treated as VLAN tagged and the VLAN tag
///       is parsed. Otherwise, the frame is treated as untagged and vlan_tag is reset.
/// \note Caller must ensure the buffer has sufficient space before calling.
///
[[nodiscard]] auto load_unchecked(std::span<uint8_t const> buf, EthernetFrame* item) noexcept -> size_t;

///
/// Serialize an Ethernet frame to a buffer without bounds checking.
/// Handles both VLAN tagged (802.1Q) and untagged frames based on vlan_tag presence.
///
/// \param buf The buffer to write the Ethernet frame data to (must have sufficient space).
/// \param item The EthernetFrame to serialize.
/// \return The number of bytes written (14 for untagged, 18 for VLAN tagged).
///
/// \note If item.vlan_tag has a value, the frame is serialized as VLAN tagged (802.1Q).
///       Otherwise, the frame is serialized as untagged.
/// \note Caller must ensure the buffer has sufficient space before calling.
///
[[nodiscard]] auto store_unchecked(std::span<uint8_t> buf, EthernetFrame const& item) noexcept -> size_t;

}  // namespace statusbar::ieee
