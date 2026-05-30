# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

set(_bpf_sources bpf_base.cpp bpf_device_base.cpp)

if(APPLE)
  list(APPEND _bpf_sources bpf_device_darwin.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND _bpf_sources bpf_device_linux.cpp)
else()
  # The bpf module has no portable backend — bpf_device_base.cpp alone calls
  # virtuals defined on BpfDeviceLinux / BpfDeviceDarwin, so a library built
  # without one of those would link but fail at runtime / instantiation. Fail
  # loudly during configure rather than ship a half-built target.
  message(
    FATAL_ERROR
      "statusbar/bpf has no backend for CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}. "
      "Supported platforms: Linux, Darwin. Disable the bpf module or add a "
      "backend before configuring on this platform.")
endif()

statusbar_add_module(
  NAME
  bpf
  SOURCES
  ${_bpf_sources}
  DEPS
  statusbar-buffer
  statusbar-status
  TESTS
  bpf_base_test.cpp)
