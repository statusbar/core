#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Human-readable names for IEEE protocol constants (ethertypes, etc.)

#include "statusbar/fmt/fmt.hpp"

#include <cstdint>
#include <string_view>

namespace statusbar::ieee {

namespace detail {
struct EthertypeNameEntry
{
    std::uint16_t key;
    std::string_view name;
};

inline constexpr EthertypeNameEntry kEthertypeNames[] = {
    {.key = 0x0800, .name = "IPv4"},
    {.key = 0x0806, .name = "ARP"},
    {.key = 0x86DD, .name = "IPv6"},
    {.key = 0x8100, .name = "VLAN"},
    {.key = 0x88A8, .name = "QinQ"},
    {.key = 0x88F7, .name = "PTP"},
    {.key = 0x22F0, .name = "AVTP"},
    {.key = 0x88CC, .name = "LLDP"},
    {.key = 0x88E1, .name = "HomePlug"},
    {.key = 0x8902, .name = "CFM"},
    {.key = 0x88B8, .name = "GOOSE"},
    {.key = 0x88BA, .name = "SV"},
    {.key = 0x22EA, .name = "SRP"},
};

// Longest known name, or the "0xNNNN" hex fallback (6), whichever is larger.
// Derived from the table so adding a longer name cannot overflow the result.
inline constexpr std::size_t kMaxEthertypeName = [] {
    std::size_t m = 6;  // "0x0000"
    for (auto const& e : kEthertypeNames) {
        if (e.name.size() > m) {
            m = e.name.size();
        }
    }
    return m;
}();
}  // namespace detail

/// Map common ethertypes to human-readable names.
/// Returns the hex form (e.g. "0x1234") for unrecognized values.
/// \param ethertype The 16-bit Ethertype value to look up.
[[nodiscard]] constexpr auto ethertype_name(std::uint16_t ethertype) -> ::statusbar::fmt::fixed_str<detail::kMaxEthertypeName>
{
    for (auto const& e : detail::kEthertypeNames) {
        if (e.key == ethertype) {
            return ::statusbar::fmt::fixed_str<detail::kMaxEthertypeName>{e.name};
        }
    }
    return ::statusbar::fmt::format<"0x{:04x}">(ethertype);
}

}  // namespace statusbar::ieee
