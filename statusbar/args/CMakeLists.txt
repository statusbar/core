# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  args
  SOURCES
  args_spec.cpp
  DEPS
  statusbar-status
  statusbar-toml
  TESTS
  args_test.cpp)
