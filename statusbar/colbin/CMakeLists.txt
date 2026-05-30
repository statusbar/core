# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  colbin
  SOURCES
  colbin.cpp
  colbin_error.cpp
  colbin_writer.cpp
  colbin_reader.cpp
  DEPS
  statusbar-status
  TESTS
  colbin_test.cpp)
