#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// TSN module header including all format_to() overloads.
///
/// Include this header instead of tsn.hpp when the translation unit
/// needs to call format_to on TSN types. Headers that only need the
/// data structures should include tsn.hpp (or the specific sub-header)
/// to avoid the compile-time cost of <format>.

#include "statusbar/tsn/tsn.hpp"
#include "statusbar/tsn/tsn_clock_identity_format.hpp"
#include "statusbar/tsn/tsn_stream_id_format.hpp"
