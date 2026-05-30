# Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
# SPDX-License-Identifier: MIT

# The lightweight unit-test framework. A real module so exported packages can
# link statusbar::test when building their own test binaries.
statusbar_add_module(NAME test SOURCES test.cpp)
