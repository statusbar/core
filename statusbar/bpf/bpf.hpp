#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// Re-export submodules
#include "statusbar/bpf/bpf_base.hpp"
#include "statusbar/bpf/bpf_device_base.hpp"

// Platform-specific device export
#if defined(__linux__)
#    include "statusbar/bpf/bpf_device_linux.hpp"
#else
#    include "statusbar/bpf/bpf_device_darwin.hpp"
#endif

namespace statusbar::bpf {

// Platform-agnostic type alias
#if defined(__linux__)
using BpfDevice = BpfDeviceLinux;
#else
using BpfDevice = BpfDeviceDarwin;
#endif

}  // namespace statusbar::bpf
