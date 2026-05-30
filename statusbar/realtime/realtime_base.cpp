// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Implementation of shutdown_token() for the realtime module.
/// Delegates to statusbar::itc::install_stop_signal() so the realtime
/// timers and the rest of the codebase share one coordinated stop signal.
/// Separated from the header to prevent inlining across module boundaries.

#include "statusbar/realtime/realtime_base.hpp"

#include "statusbar/itc/itc_stop_token.hpp"
#include "statusbar/realtime/realtime.hpp"

namespace statusbar::realtime {

auto shutdown_token() noexcept -> statusbar::itc::StopToken&
{
    return statusbar::itc::install_stop_signal();
}

}  // namespace statusbar::realtime
