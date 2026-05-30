#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer_error.hpp"
#include "statusbar/buffer/buffer_traits.hpp"
#include "statusbar/buffer/span_utils.hpp"
#include "statusbar/status/status.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

namespace statusbar {

using traits::PlainCArray;
using traits::PlainElement;
using traits::PlainLinearCollection;
using traits::PlainStdArray;
using traits::PlainStdSpan;
using traits::PlainStdVector;
using traits::PlainType;

}  // namespace statusbar
