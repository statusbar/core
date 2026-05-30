#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// TCAM-style compiled classifier rule and semantic rule builder.
///
/// A CompiledRule<N> is a fixed N-byte (mask, match) pair plus metadata.
/// The matcher AND's each rule's mask against the first N bytes of
/// an Ethernet frame and compares the result to the rule's match bytes.
///
/// Typical window sizes:
///   - 32 bytes (default) covers Ethernet + optional VLAN + AVTP
///     common header + 1722 Stream ID. Fits the AVTP RX hot path.
///   - 64 bytes additionally covers IPv4/IPv6 headers and UDP
///     destination port for tagged and untagged frames.
///   - 72 bytes extends to Q-in-Q double-tagged frames.
///
/// N must be a multiple of 8 and at least 32 so the Ethernet/VLAN/
/// AVTP offsets the builder uses remain in range. The evaluator runs
/// N / 8 64-bit AND+CMP pairs per rule; clang/gcc auto-vectorize
/// this into NEON / AVX2 at -O2 when profitable.

#include "statusbar/ieee/ieee.hpp"
#include "statusbar/ip/ip_ipv4_address.hpp"
#include "statusbar/ip/ip_ipv6.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace statusbar::net {

/// Default window size used when the callsite doesn't explicitly
/// choose one.
inline constexpr std::size_t tcam_default_window_bytes = 32;

/// Bit flags returned by the matcher when a rule matches.
///
/// Flag layout (64 bits):
///   bits  0..15  — system-reserved. Library-defined meanings; do
///                  not redefine in user code. Covers the classic
///                  TrafficClassifier actions and coarse protocol
///                  tags, plus room for future library growth.
///   bits 16..63  — user-defined. 48 bits for your own tagging.
///                  The library never inspects or allocates these.
///
/// Construct user flags with `flag::user_bit<I>` (compile-time,
/// bounds-checked via static_assert) or `flag::make_user_bit(i)`
/// (runtime, invalid index returns 0). Combine with `|`. Use
/// `flag::user_bits(v)` / `flag::system_bits(v)` to split a result
/// into its user-owned and library-owned halves.
namespace flag {

// System-reserved bits (0..15) — library-defined.
inline constexpr std::uint64_t none = 0ULL;
inline constexpr std::uint64_t process = 1ULL << 0U;
inline constexpr std::uint64_t forward_tap = 1ULL << 1U;
inline constexpr std::uint64_t drop = 1ULL << 2U;
inline constexpr std::uint64_t rx_avtp_stream = 1ULL << 8U;
inline constexpr std::uint64_t rx_gptp = 1ULL << 9U;
inline constexpr std::uint64_t rx_msrp = 1ULL << 10U;
inline constexpr std::uint64_t rx_mvrp = 1ULL << 11U;
// Bits 12..15 reserved for future library use.

/// First bit user code may claim. Everything at or above this
/// position is yours; the library will never use it.
inline constexpr std::uint64_t USER_BIT_FIRST = 16;

/// Last bit user code may claim (63, the high bit).
inline constexpr std::uint64_t USER_BIT_LAST = 63;

/// Number of user-available bits (48).
inline constexpr std::uint64_t USER_BIT_COUNT = USER_BIT_LAST - USER_BIT_FIRST + 1;

/// Every bit at or above USER_BIT_FIRST — the slice user code owns.
inline constexpr std::uint64_t USER_MASK = ~((1ULL << USER_BIT_FIRST) - 1ULL);

/// Every bit below USER_BIT_FIRST — library-reserved.
inline constexpr std::uint64_t SYSTEM_MASK = (1ULL << USER_BIT_FIRST) - 1ULL;

/// Compile-time user flag by index (0..47). Forms a single-bit mask
/// at position USER_BIT_FIRST + Index.
template <std::uint64_t Index>
inline constexpr std::uint64_t user_bit = []() {
    static_assert(Index < USER_BIT_COUNT, "user_bit<I>: index must be in 0..47");
    return 1ULL << (USER_BIT_FIRST + Index);
}();

/// Runtime user flag by index (0..47). Returns 0 for out-of-range
/// indices so the call is safe; prefer the compile-time form when
/// the index is known.
[[nodiscard]] constexpr auto make_user_bit(std::size_t index) noexcept -> std::uint64_t
{
    return (index < USER_BIT_COUNT) ? (1ULL << (USER_BIT_FIRST + index)) : 0ULL;
}

/// Extract only the library-defined bits.
[[nodiscard]] constexpr auto system_bits(std::uint64_t value) noexcept -> std::uint64_t
{
    return value & SYSTEM_MASK;
}

/// Extract only the user-defined bits.
[[nodiscard]] constexpr auto user_bits(std::uint64_t value) noexcept -> std::uint64_t
{
    return value & USER_MASK;
}

}  // namespace flag

/// Plain-old-data classifier rule parameterized by window size.
template <std::size_t N = tcam_default_window_bytes>
struct CompiledRule
{
    static_assert(N >= 32, "CompiledRule window must cover at least Ethernet + VLAN + AVTP header");
    static_assert(N % 8 == 0, "CompiledRule window must be a multiple of 8 bytes");

    static constexpr std::size_t window_bytes = N;
    static constexpr std::size_t lanes = N / 8;

    std::array<std::uint8_t, N> mask{};
    std::array<std::uint8_t, N> match{};
    std::uint64_t result_flags{0};
    std::uint16_t min_frame_size{0};
    std::uint8_t priority{0};
    std::uint8_t _pad{0};
};

using CompiledRule32 = CompiledRule<32>;
using CompiledRule64 = CompiledRule<64>;

enum class RuleVlanMode : std::uint8_t
{
    Untagged,
    TaggedAny,
    TaggedExact,
    Any,
};

namespace detail {

// Layout offsets the builder writes into. Expressed at namespace
// scope so the RuleVlanMode switch in build() reads as a table
// lookup rather than inline magic numbers.
inline constexpr std::size_t OFFSET_DEST_MAC = 0;
inline constexpr std::size_t OFFSET_SRC_MAC = 6;
inline constexpr std::size_t OFFSET_TPID = 12;
inline constexpr std::size_t OFFSET_TCI = 14;
inline constexpr std::size_t OFFSET_ETHERTYPE_UNTAGGED = 12;
inline constexpr std::size_t OFFSET_ETHERTYPE_TAGGED = 16;
inline constexpr std::size_t OFFSET_AVTP_SUBTYPE_UNTAGGED = 14;
inline constexpr std::size_t OFFSET_AVTP_SUBTYPE_TAGGED = 18;
inline constexpr std::size_t OFFSET_STREAM_ID_UNTAGGED = OFFSET_AVTP_SUBTYPE_UNTAGGED + 4;  // 18
inline constexpr std::size_t OFFSET_STREAM_ID_TAGGED = OFFSET_AVTP_SUBTYPE_TAGGED + 4;      // 22

// IPv4 "protocol" byte lives at +9 from the start of the IP header.
// IPv6 "next header" byte lives at +6.
inline constexpr std::size_t OFFSET_IPV4_PROTO_UNTAGGED = 14 + 9;     // 23
inline constexpr std::size_t OFFSET_IPV4_PROTO_TAGGED = 18 + 9;       // 27
inline constexpr std::size_t OFFSET_IPV6_NEXT_HDR_UNTAGGED = 14 + 6;  // 20
inline constexpr std::size_t OFFSET_IPV6_NEXT_HDR_TAGGED = 18 + 6;    // 24

// IP address offsets relative to the IP header start.
inline constexpr std::size_t OFFSET_IPV4_SRC_UNTAGGED = 14 + 12;  // 26
inline constexpr std::size_t OFFSET_IPV4_SRC_TAGGED = 18 + 12;    // 30
inline constexpr std::size_t OFFSET_IPV4_DST_UNTAGGED = 14 + 16;  // 30
inline constexpr std::size_t OFFSET_IPV4_DST_TAGGED = 18 + 16;    // 34
inline constexpr std::size_t OFFSET_IPV6_SRC_UNTAGGED = 14 + 8;   // 22
inline constexpr std::size_t OFFSET_IPV6_SRC_TAGGED = 18 + 8;     // 26
inline constexpr std::size_t OFFSET_IPV6_DST_UNTAGGED = 14 + 24;  // 38
inline constexpr std::size_t OFFSET_IPV6_DST_TAGGED = 18 + 24;    // 42

// ATDECC entity-id field offsets relative to the AVTP subtype byte
// (ACMP / AECP PDUs, IEEE 1722.1). All are 8-byte Eui64s.
//
//   AECP:  target_entity_id     at AVTP +4   (both AECP-AEM and AECP-AA)
//          controller_entity_id at AVTP +12
//   ACMP:  stream_id            at AVTP +4   (reserved — stream_id field)
//          controller_entity_id at AVTP +12
//          talker_entity_id     at AVTP +20
//          listener_entity_id   at AVTP +28
inline constexpr std::size_t OFFSET_AECP_TARGET_UNTAGGED = 14 + 4;         // 18
inline constexpr std::size_t OFFSET_AECP_TARGET_TAGGED = 18 + 4;           // 22
inline constexpr std::size_t OFFSET_ATDECC_CONTROLLER_UNTAGGED = 14 + 12;  // 26
inline constexpr std::size_t OFFSET_ATDECC_CONTROLLER_TAGGED = 18 + 12;    // 30
inline constexpr std::size_t OFFSET_ACMP_TALKER_UNTAGGED = 14 + 20;        // 34
inline constexpr std::size_t OFFSET_ACMP_TALKER_TAGGED = 18 + 20;          // 38
inline constexpr std::size_t OFFSET_ACMP_LISTENER_UNTAGGED = 14 + 28;      // 42
inline constexpr std::size_t OFFSET_ACMP_LISTENER_TAGGED = 18 + 28;        // 46

inline constexpr std::uint16_t TPID_8021Q = 0x8100U;
inline constexpr std::uint16_t TCI_VID_MASK = 0x0FFFU;

/// Write `value.size()` bytes starting at `offset`, gated by `mask`.
/// Silently no-ops when offset + size exceeds N — lets the builder
/// skip fields that don't fit the current window without scribbling
/// past the rule's storage.
template <std::size_t N>
constexpr void write_range(
    CompiledRule<N>& out, std::size_t offset, std::span<std::uint8_t const> value, std::span<std::uint8_t const> mask)
{
    if (offset + value.size() > N) {
        return;
    }
    for (std::size_t i = 0; i < value.size(); ++i) {
        out.mask[offset + i] |= mask[i];
        out.match[offset + i] = (out.match[offset + i] & ~mask[i]) | (value[i] & mask[i]);
    }
}

}  // namespace detail

template <std::size_t N>
constexpr void write_mac_field(CompiledRule<N>& out, std::size_t offset, ieee::Eui48 const& mac)
{
    constexpr std::array<std::uint8_t, 6> full_mask = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    auto const bytes = mac.span();
    detail::write_range(out, offset, bytes, full_mask);
}

template <std::size_t N>
constexpr void write_u16_be_field(CompiledRule<N>& out, std::size_t offset, std::uint16_t value, std::uint16_t mask = 0xFFFFU)
{
    std::array<std::uint8_t, 2> const bytes = {
        static_cast<std::uint8_t>((value >> 8) & 0xFFU), static_cast<std::uint8_t>(value & 0xFFU)};
    std::array<std::uint8_t, 2> const mask_bytes = {
        static_cast<std::uint8_t>((mask >> 8) & 0xFFU), static_cast<std::uint8_t>(mask & 0xFFU)};
    detail::write_range(out, offset, bytes, mask_bytes);
}

/// Fixed-size output of ClassifierRuleBuilder::build(). Holds up to
/// `MaxFanout` CompiledRules plus an active count, so the whole
/// builder chain stays constexpr (no std::vector / heap). The
/// current maximum is 2 (`.vlan_any()` fan-out); bump the template
/// parameter if future builder features grow the fan-out.
template <std::size_t N, std::size_t MaxFanout = 2>
struct BuiltRules
{
    std::array<CompiledRule<N>, MaxFanout> entries{};
    std::size_t count{0};

    [[nodiscard]] constexpr auto size() const noexcept -> std::size_t { return count; }
    [[nodiscard]] constexpr auto empty() const noexcept -> bool { return count == 0; }

    constexpr auto begin() const noexcept -> CompiledRule<N> const* { return entries.data(); }
    constexpr auto end() const noexcept -> CompiledRule<N> const* { return entries.data() + count; }

    constexpr auto operator[](std::size_t i) const noexcept -> CompiledRule<N> const& { return entries[i]; }

    /// Implicit span view — feed straight into TcamClassifier::set_rules.
    constexpr operator std::span<CompiledRule<N> const>() const noexcept  // NOLINT(google-explicit-constructor)
    {
        return {entries.data(), count};
    }
};

/// Semantic builder. Window size is a template parameter so AVB
/// callsites can stay at 32 bytes while sites that need IPv4/IPv6
/// address matching use 64 (or 72 for Q-in-Q). Every method is
/// constexpr; `constexpr auto rules = ClassifierRuleBuilder<64>{}
/// .ethertype(0x0800)...build();` evaluates the rule at compile time.
template <std::size_t N = tcam_default_window_bytes>
class ClassifierRuleBuilder
{
  public:
    static_assert(N >= 32 && (N % 8 == 0), "invalid TCAM window size");

    constexpr ClassifierRuleBuilder() = default;

    constexpr auto dest_mac(ieee::Eui48 const& mac) -> ClassifierRuleBuilder&
    {
        state_.dest_mac = mac;
        return *this;
    }
    constexpr auto dest_mac_any() -> ClassifierRuleBuilder&
    {
        state_.dest_mac.reset();
        return *this;
    }

    constexpr auto src_mac(ieee::Eui48 const& mac) -> ClassifierRuleBuilder&
    {
        state_.src_mac = mac;
        return *this;
    }
    constexpr auto src_mac_any() -> ClassifierRuleBuilder&
    {
        state_.src_mac.reset();
        return *this;
    }

    constexpr auto ethertype(std::uint16_t value) -> ClassifierRuleBuilder&
    {
        state_.ethertype = value;
        return *this;
    }
    constexpr auto ethertype_any() -> ClassifierRuleBuilder&
    {
        state_.ethertype.reset();
        return *this;
    }

    constexpr auto vlan_untagged() -> ClassifierRuleBuilder&
    {
        state_.vlan_mode = RuleVlanMode::Untagged;
        state_.vlan_vid.reset();
        return *this;
    }
    constexpr auto vlan_tagged_any() -> ClassifierRuleBuilder&
    {
        state_.vlan_mode = RuleVlanMode::TaggedAny;
        state_.vlan_vid.reset();
        return *this;
    }
    constexpr auto vlan_tagged_exact(std::uint16_t vid) -> ClassifierRuleBuilder&
    {
        state_.vlan_mode = RuleVlanMode::TaggedExact;
        state_.vlan_vid = vid & detail::TCI_VID_MASK;
        return *this;
    }
    constexpr auto vlan_any() -> ClassifierRuleBuilder&
    {
        state_.vlan_mode = RuleVlanMode::Any;
        state_.vlan_vid.reset();
        return *this;
    }

    constexpr auto avtp_subtype(std::uint8_t subtype) -> ClassifierRuleBuilder&
    {
        state_.avtp_subtype = subtype;
        return *this;
    }

    /// IPv4 `protocol` byte at IP-header +9. Pair with
    /// `.ethertype(0x0800)` to pick IPv4-UDP (17), IPv4-TCP (6), etc.
    constexpr auto ipv4_protocol(std::uint8_t proto) -> ClassifierRuleBuilder&
    {
        state_.ipv4_protocol = proto;
        return *this;
    }

    /// IPv6 `next header` byte at IP-header +6. Pair with
    /// `.ethertype(0x86DD)` for IPv6-UDP (17), IPv6-TCP (6), etc.
    /// Only sees the first Next Header; frames with extension
    /// headers before the transport protocol aren't matched here.
    constexpr auto ipv6_next_header(std::uint8_t nh) -> ClassifierRuleBuilder&
    {
        state_.ipv6_next_header = nh;
        return *this;
    }

    /// IPv4 source address. Requires N >= 30 for untagged, N >= 34
    /// for tagged; writes that don't fit the window are silently
    /// skipped.
    constexpr auto ipv4_src(ip::IPv4Address const& addr) -> ClassifierRuleBuilder&
    {
        state_.ipv4_src_addr = addr;
        return *this;
    }

    /// IPv4 destination address. Requires N >= 34 / 38.
    constexpr auto ipv4_dst(ip::IPv4Address const& addr) -> ClassifierRuleBuilder&
    {
        state_.ipv4_dst_addr = addr;
        return *this;
    }

    /// IPv6 source address. Requires N >= 38 / 42.
    constexpr auto ipv6_src(ip::IPv6Address const& addr) -> ClassifierRuleBuilder&
    {
        state_.ipv6_src_addr = addr;
        return *this;
    }

    /// IPv6 destination address. Requires N >= 54 / 58.
    constexpr auto ipv6_dst(ip::IPv6Address const& addr) -> ClassifierRuleBuilder&
    {
        state_.ipv6_dst_addr = addr;
        return *this;
    }

    /// ATDECC controller_entity_id. Shared position across ACMP and
    /// AECP PDUs — the builder doesn't care which one you're
    /// matching, it only writes the 8 bytes at AVTP +12.
    /// Requires N >= 34 / 38.
    constexpr auto atdecc_controller_entity_id(ieee::Eui64 const& id) -> ClassifierRuleBuilder&
    {
        state_.atdecc_controller_entity_id = id;
        return *this;
    }

    /// AECP target_entity_id, at AVTP +4. Requires N >= 26 / 30.
    /// Pair with .ethertype(ETHERTYPE_AVTP).avtp_subtype(AvtpSubtype::aecp)
    /// to scope the rule to AECP frames.
    constexpr auto aecp_target_entity_id(ieee::Eui64 const& id) -> ClassifierRuleBuilder&
    {
        state_.aecp_target_entity_id = id;
        return *this;
    }

    /// ACMP talker_entity_id, at AVTP +20. Requires N >= 42 / 46.
    /// Pair with .avtp_subtype(AvtpSubtype::acmp).
    constexpr auto acmp_talker_entity_id(ieee::Eui64 const& id) -> ClassifierRuleBuilder&
    {
        state_.acmp_talker_entity_id = id;
        return *this;
    }

    /// ACMP listener_entity_id, at AVTP +28. Requires N >= 50 / 54.
    /// Pair with .avtp_subtype(AvtpSubtype::acmp).
    constexpr auto acmp_listener_entity_id(ieee::Eui64 const& id) -> ClassifierRuleBuilder&
    {
        state_.acmp_listener_entity_id = id;
        return *this;
    }

    constexpr auto stream_id(std::array<std::uint8_t, 8> const& sid) -> ClassifierRuleBuilder&
    {
        state_.stream_id = sid;
        return *this;
    }
    constexpr auto stream_id(std::uint64_t sid_be) -> ClassifierRuleBuilder&
    {
        std::array<std::uint8_t, 8> bytes{};
        for (std::size_t i = 0; i < 8; ++i) {
            bytes[i] = static_cast<std::uint8_t>((sid_be >> (56 - (8 * i))) & 0xFFU);
        }
        state_.stream_id = bytes;
        return *this;
    }

    constexpr auto result(std::uint64_t flags) -> ClassifierRuleBuilder&
    {
        state_.result_flags = flags;
        return *this;
    }
    constexpr auto priority(std::uint8_t p) -> ClassifierRuleBuilder&
    {
        state_.priority = p;
        return *this;
    }

    [[nodiscard]] constexpr auto build() const -> BuiltRules<N>
    {
        auto const materialize = [this](RuleVlanMode framing, CompiledRule<N>& out) constexpr {
            if (state_.dest_mac) {
                write_mac_field(out, detail::OFFSET_DEST_MAC, *state_.dest_mac);
            }
            if (state_.src_mac) {
                write_mac_field(out, detail::OFFSET_SRC_MAC, *state_.src_mac);
            }

            bool const tagged = (framing == RuleVlanMode::TaggedAny || framing == RuleVlanMode::TaggedExact);

            std::size_t const ethertype_offset = tagged ? detail::OFFSET_ETHERTYPE_TAGGED : detail::OFFSET_ETHERTYPE_UNTAGGED;
            std::size_t const subtype_offset = tagged ? detail::OFFSET_AVTP_SUBTYPE_TAGGED : detail::OFFSET_AVTP_SUBTYPE_UNTAGGED;
            std::size_t const sid_offset = tagged ? detail::OFFSET_STREAM_ID_TAGGED : detail::OFFSET_STREAM_ID_UNTAGGED;
            std::size_t const ipv4_proto_offset = tagged ? detail::OFFSET_IPV4_PROTO_TAGGED : detail::OFFSET_IPV4_PROTO_UNTAGGED;
            std::size_t const ipv6_nh_offset = tagged ? detail::OFFSET_IPV6_NEXT_HDR_TAGGED : detail::OFFSET_IPV6_NEXT_HDR_UNTAGGED;
            std::size_t const ipv4_src_offset = tagged ? detail::OFFSET_IPV4_SRC_TAGGED : detail::OFFSET_IPV4_SRC_UNTAGGED;
            std::size_t const ipv4_dst_offset = tagged ? detail::OFFSET_IPV4_DST_TAGGED : detail::OFFSET_IPV4_DST_UNTAGGED;
            std::size_t const ipv6_src_offset = tagged ? detail::OFFSET_IPV6_SRC_TAGGED : detail::OFFSET_IPV6_SRC_UNTAGGED;
            std::size_t const ipv6_dst_offset = tagged ? detail::OFFSET_IPV6_DST_TAGGED : detail::OFFSET_IPV6_DST_UNTAGGED;
            std::size_t const atdecc_controller_offset =
                tagged ? detail::OFFSET_ATDECC_CONTROLLER_TAGGED : detail::OFFSET_ATDECC_CONTROLLER_UNTAGGED;
            std::size_t const aecp_target_offset = tagged ? detail::OFFSET_AECP_TARGET_TAGGED : detail::OFFSET_AECP_TARGET_UNTAGGED;
            std::size_t const acmp_talker_offset = tagged ? detail::OFFSET_ACMP_TALKER_TAGGED : detail::OFFSET_ACMP_TALKER_UNTAGGED;
            std::size_t const acmp_listener_offset =
                tagged ? detail::OFFSET_ACMP_LISTENER_TAGGED : detail::OFFSET_ACMP_LISTENER_UNTAGGED;

            if (tagged) {
                write_u16_be_field(out, detail::OFFSET_TPID, detail::TPID_8021Q);
                if (framing == RuleVlanMode::TaggedExact && state_.vlan_vid) {
                    write_u16_be_field(out, detail::OFFSET_TCI, *state_.vlan_vid & detail::TCI_VID_MASK, detail::TCI_VID_MASK);
                }
            }

            if (state_.ethertype) {
                write_u16_be_field(out, ethertype_offset, *state_.ethertype);
            }
            if (state_.avtp_subtype && subtype_offset < N) {
                out.mask[subtype_offset] |= 0xFFU;
                out.match[subtype_offset] = *state_.avtp_subtype;
            }
            if (state_.ipv4_protocol && ipv4_proto_offset < N) {
                out.mask[ipv4_proto_offset] |= 0xFFU;
                out.match[ipv4_proto_offset] = *state_.ipv4_protocol;
            }
            if (state_.ipv6_next_header && ipv6_nh_offset < N) {
                out.mask[ipv6_nh_offset] |= 0xFFU;
                out.match[ipv6_nh_offset] = *state_.ipv6_next_header;
            }
            if (state_.stream_id) {
                constexpr std::array<std::uint8_t, 8> full_mask = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                detail::write_range(out, sid_offset, *state_.stream_id, full_mask);
            }
            constexpr std::array<std::uint8_t, 4> ipv4_full_mask = {0xFF, 0xFF, 0xFF, 0xFF};
            constexpr std::array<std::uint8_t, 8> eui64_full_mask = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            constexpr std::array<std::uint8_t, 16> ipv6_full_mask = {
                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

            if (state_.ipv4_src_addr) {
                detail::write_range(out, ipv4_src_offset, state_.ipv4_src_addr->span(), ipv4_full_mask);
            }
            if (state_.ipv4_dst_addr) {
                detail::write_range(out, ipv4_dst_offset, state_.ipv4_dst_addr->span(), ipv4_full_mask);
            }
            if (state_.ipv6_src_addr) {
                detail::write_range(out, ipv6_src_offset, state_.ipv6_src_addr->span(), ipv6_full_mask);
            }
            if (state_.ipv6_dst_addr) {
                detail::write_range(out, ipv6_dst_offset, state_.ipv6_dst_addr->span(), ipv6_full_mask);
            }
            if (state_.atdecc_controller_entity_id) {
                detail::write_range(out, atdecc_controller_offset, state_.atdecc_controller_entity_id->span(), eui64_full_mask);
            }
            if (state_.aecp_target_entity_id) {
                detail::write_range(out, aecp_target_offset, state_.aecp_target_entity_id->span(), eui64_full_mask);
            }
            if (state_.acmp_talker_entity_id) {
                detail::write_range(out, acmp_talker_offset, state_.acmp_talker_entity_id->span(), eui64_full_mask);
            }
            if (state_.acmp_listener_entity_id) {
                detail::write_range(out, acmp_listener_offset, state_.acmp_listener_entity_id->span(), eui64_full_mask);
            }

            out.result_flags = state_.result_flags;
            out.priority = state_.priority;
            out.min_frame_size = static_cast<std::uint16_t>(N);
        };

        BuiltRules<N> out{};
        if (state_.vlan_mode == RuleVlanMode::Any) {
            materialize(RuleVlanMode::Untagged, out.entries[0]);
            materialize(RuleVlanMode::TaggedAny, out.entries[1]);
            out.count = 2;
        } else {
            materialize(state_.vlan_mode, out.entries[0]);
            out.count = 1;
        }
        return out;
    }

  private:
    struct SemanticState
    {
        std::optional<ieee::Eui48> dest_mac;
        std::optional<ieee::Eui48> src_mac;
        std::optional<std::uint16_t> ethertype;
        std::optional<std::uint8_t> avtp_subtype;
        std::optional<std::uint8_t> ipv4_protocol;
        std::optional<std::uint8_t> ipv6_next_header;
        std::optional<ip::IPv4Address> ipv4_src_addr;
        std::optional<ip::IPv4Address> ipv4_dst_addr;
        std::optional<ip::IPv6Address> ipv6_src_addr;
        std::optional<ip::IPv6Address> ipv6_dst_addr;
        std::optional<ieee::Eui64> atdecc_controller_entity_id;
        std::optional<ieee::Eui64> aecp_target_entity_id;
        std::optional<ieee::Eui64> acmp_talker_entity_id;
        std::optional<ieee::Eui64> acmp_listener_entity_id;
        std::optional<std::array<std::uint8_t, 8>> stream_id;
        RuleVlanMode vlan_mode{RuleVlanMode::Untagged};
        std::optional<std::uint16_t> vlan_vid;
        std::uint64_t result_flags{0};
        std::uint8_t priority{0};
    };

    SemanticState state_{};
};

}  // namespace statusbar::net
