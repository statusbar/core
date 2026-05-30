#! /bin/bash
# Standalone build helper for statusbar-core.
#
# core has no statusbar dependencies, so this is just a thin wrapper that
# configures with the bundled clang/libc++ toolchain, builds, and runs
# tests. Extra args are forwarded to `cmake` configure:
#
#   ./local-build.sh                  # configure + build + ctest
#   ./local-build.sh -DENABLE_ASAN=ON # forward cmake args
#
# IDE users: the configure line below (echoed by `set -x`) is exactly what
# you should give your IDE. The only required flag is the toolchain file —
# everything else is optional.

set -e
set -x
cmake -S . -B build -G Ninja --toolchain cmake/toolchain-clang.cmake "$@"
cmake --build build
ctest --test-dir build
