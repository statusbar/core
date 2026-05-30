#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// STATUSBAR_ASSERT(expr) — debug-checked precondition with optimizer hint.
//
// - Debug builds: runs the runtime check via assert(expr), then issues
//   [[assume(expr)]] so the compiler verifies that `expr` is side-effect-
//   free. The expression evaluates twice in debug, which is intentional —
//   the predicate is required to be pure.
//
// - Release builds (NDEBUG defined): only [[assume(expr)]] is emitted —
//   the compiler may rely on `expr` being true to eliminate downstream
//   branches but never evaluates it. Violating the precondition in
//   release is undefined behaviour.
//
// Putting [[assume(expr)]] in BOTH branches is deliberate: every call
// site goes through the same side-effect / pure-function analysis, so a
// predicate that accidentally calls a non-pure function fails to build
// where today it would silently mutate state in debug but evaporate in
// release.

#include <cassert>

#ifdef NDEBUG
#    define STATUSBAR_ASSERT(expr) [[assume(expr)]]
#else
#    define STATUSBAR_ASSERT(expr)                                                                                                 \
        do {                                                                                                                       \
            assert(expr);                                                                                                          \
            [[assume(expr)]];                                                                                                      \
        } while (false)
#endif
