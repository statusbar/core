# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  stats
  SOURCES
  stats_descriptive.cpp
  stats_histogram.cpp
  stats_format.cpp
  stats_atomic_histogram.cpp
  stats_atomic_time_stats.cpp
  stats_atomic_wake_stats.cpp
  TESTS
  stats_test.cpp)
