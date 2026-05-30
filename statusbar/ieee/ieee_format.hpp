#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// IEEE module header including all format_to() overloads.
///
/// Include this header instead of ieee.hpp when the translation unit
/// needs to call format_to on IEEE types. Headers that only need the
/// data structures should include ieee.hpp (or the specific sub-header)
/// to avoid the compile-time cost of <format>.

#include "statusbar/ieee/ieee.hpp"
#include "statusbar/ieee/ieee_ethernet_format.hpp"
