#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// format_to() overload for tsn::ClockIdentity. Split from
/// tsn_clock_identity.hpp so consumers that only need the struct do not
/// pay the compile-time cost of <format>.

#include "statusbar/tsn/tsn_clock_identity.hpp"

#include <format>

namespace statusbar::tsn {

/// Format a ClockIdentity to an output iterator.
/// Format: "aa:bb:cc:dd:ee:ff:gg:hh" (8 colon-separated hex bytes)
template <typename OutputIt>
auto format_to(OutputIt out, ClockIdentity const& id) -> OutputIt
{
    auto const s = id.span();
    return std::format_to(
        out, "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]);
}

}  // namespace statusbar::tsn
