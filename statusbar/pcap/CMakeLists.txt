# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  pcap
  SOURCES
  pcap_error.cpp
  pcap_reader.cpp
  pcap_replay.cpp
  pcap_writer.cpp
  pcapng_reader.cpp
  pcapng_writer.cpp
  DEPS
  statusbar-buffer
  statusbar-ieee
  statusbar-status
  TESTS
  pcap_test.cpp)
