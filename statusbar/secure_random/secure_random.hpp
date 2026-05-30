#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <span>

namespace statusbar {

/// Fill `out` with cryptographically secure random bytes.
///
/// On Linux this uses `getrandom(2)`. On macOS / BSD this uses
/// `arc4random_buf(3)`. There is no fallback to weaker sources.
///
/// On Linux, the first call may block briefly while the kernel
/// entropy pool initialises (typically once shortly after boot);
/// subsequent calls never block. The function aborts the process if
/// the underlying entropy syscall fails irrecoverably — silently
/// returning weak entropy from a degraded source would be worse than
/// aborting.
///
/// Safe to call from any thread.
void secure_random_bytes(std::span<uint8_t> out) noexcept;

}  // namespace statusbar
