# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  benchmark
  SOURCES
  benchmark.cpp
  DEPS
  statusbar-stats
  TESTS
  benchmark_test.cpp)
