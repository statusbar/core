#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// format_to() overload for tsn::StreamId. Split from tsn_stream_id.hpp
/// so consumers that only need the struct do not pay the compile-time
/// cost of <format>.

#include "statusbar/tsn/tsn_stream_id.hpp"

#include <format>
#include <tuple>

namespace statusbar::tsn {

/// Format a StreamId to an output iterator.
/// Format: "aa:bb:cc:dd:ee:ff:0001" (EUI-48:unique_id)
template <typename OutputIt>
auto format_to(OutputIt out, StreamId const& stream_id) -> OutputIt
{
    auto const& addr = stream_id.get_system_address();
    return std::format_to(
        out,
        "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:04x}",
        std::get<0>(addr.value),
        std::get<1>(addr.value),
        std::get<2>(addr.value),
        std::get<3>(addr.value),
        std::get<4>(addr.value),
        std::get<5>(addr.value),
        stream_id.get_unique_id());
}

}  // namespace statusbar::tsn
