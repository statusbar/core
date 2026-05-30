# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  tsn
  SOURCES
  tsn_clock_identity.cpp
  tsn_error.cpp
  tsn_stream_id.cpp
  DEPS
  statusbar-buffer
  statusbar-ieee
  statusbar-status
  TESTS
  tsn_clock_identity_test.cpp
  tsn_stream_id_test.cpp)
