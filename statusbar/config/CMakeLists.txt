# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  config
  SOURCES
  config_store.cpp
  config_cli.cpp
  config_error.cpp
  DEPS
  statusbar-args
  statusbar-status
  statusbar-toml
  TESTS
  config_test.cpp)
