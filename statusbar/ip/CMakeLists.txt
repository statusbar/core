# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

statusbar_add_module(
  NAME
  ip
  SOURCES
  ip_arp.cpp
  ip_icmp.cpp
  ip_icmpv6.cpp
  ip_igmp.cpp
  ip_ipv4.cpp
  DEPS
  statusbar-buffer
  statusbar-ieee
  statusbar-status
  TESTS
  ip_test.cpp)
