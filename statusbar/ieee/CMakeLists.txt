# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  ieee
  SOURCES
  ieee_ethernet.cpp
  DEPS
  statusbar-buffer
  statusbar-status
  statusbar-fmt
  TESTS
  ieee_base_test.cpp
  ieee_buffer_test.cpp
  ieee_ethernet_test.cpp
  ieee_protocol_example_test.cpp)

add_executable(ieee_bench_tool ${CMAKE_CURRENT_LIST_DIR}/ieee_bench_tool.cpp)
target_link_libraries(ieee_bench_tool PRIVATE statusbar)
set_target_properties(ieee_bench_tool PROPERTIES OUTPUT_NAME
                                                 statusbar-ieee-bench)

# Runnable target for invoking the benchmark from the Makefile.
add_custom_target(
  run-ieee-bench
  COMMAND $<TARGET_FILE:ieee_bench_tool>
  DEPENDS ieee_bench_tool
  USES_TERMINAL VERBATIM)
add_dependencies(run-all-benches run-ieee-bench)
