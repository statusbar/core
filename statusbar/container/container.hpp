#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// statusbar::container — generic bounded, zero-heap containers used across
/// protocol state machines and other modules. Include this module header to
/// get every container type in the module, or include individual headers
/// (e.g. `container_slot_table.hpp`) for finer control.

#include "statusbar/container/container_presentation_slot_map.hpp"
#include "statusbar/container/container_slot_table.hpp"
