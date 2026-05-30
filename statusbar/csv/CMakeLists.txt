# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  csv
  SOURCES
  csv_writer.cpp
  DEPS
  statusbar-status
  TESTS
  csv_writer_test.cpp)
