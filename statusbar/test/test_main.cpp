// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// Hook for downstream non-CMake build systems (Buck2 etc.); intentionally not referenced from this project's CMake.
// Standalone test main for Buck2 cxx_test targets.
// Links against the test library and any *_test.cpp files;
// TEST() macros self-register via static constructors, so
// main() just needs to call run_all().

#include "statusbar/test/test.hpp"

int main()
{
    return ::statusbar::test::TestRegister::run_all() ? 0 : 1;
}
