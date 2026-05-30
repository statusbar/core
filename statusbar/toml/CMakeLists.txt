# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  toml
  SOURCES
  toml_error.cpp
  toml_parser.cpp
  toml_value.cpp
  DEPS
  statusbar-status
  TESTS
  toml_test.cpp)
