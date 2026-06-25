[← back to module index](README.md)

# safe_arith

Overflow-detecting integer arithmetic predicates — `can_multiply` and
`can_add` — that wrap the compiler's overflow builtins so untrusted
size/count math can be guarded before it is performed.

## Overview

The `safe_arith` module is a single header in namespace `statusbar` with
two `constexpr` predicates for unsigned integers. They answer "would this
operation overflow `T`?" **without** performing it or returning a result —
you check first, then do the arithmetic knowing it is safe:

```cpp
if (can_multiply(count, stride)) { total = count * stride; }
```

The intended use is hardening: `count`, `stride`, length, and offset
fields read from wire-format packets, file headers, or any other
attacker-influenced source must be validated before they feed an
allocation size or a bounds check. A silent wrap there is a classic
heap-overflow primitive; these predicates turn it into an explicit reject.

When the compiler exposes `__builtin_mul_overflow` /
`__builtin_add_overflow` (detected via `__has_builtin` into the
`STATUSBAR_SAFE_ARITH_HAS_BUILTINS` macro) the predicates use them
directly; otherwise they fall back to the standard division /
subtraction-against-`numeric_limits::max()` formulations, so the header is
portable to compilers without the builtins.

## Key types

- `can_multiply<T>(T a, T b) -> bool` — true iff `a * b` is representable
  in `T`. `T` is constrained to `std::unsigned_integral`. `constexpr`,
  `noexcept`, `[[nodiscard]]`.
- `can_add<T>(T a, T b) -> bool` — true iff `a + b` is representable in
  `T`. Same constraints and qualifiers.
- `STATUSBAR_SAFE_ARITH_HAS_BUILTINS` — preprocessor macro, `1` when the
  overflow builtins are available, `0` when the portable fallback is in
  use. Rarely needed by callers; exposed for diagnostics.

## Quick example

```cpp
#include "statusbar/safe_arith/safe_arith.hpp"

#include <cstdint>

using namespace statusbar;

// A wire header claims `count` records of `stride` bytes each — both are
// attacker-controlled. Guard the multiply before trusting the total.
[[nodiscard]] bool record_region_fits(uint32_t count, uint32_t stride, uint32_t available)
{
    if (!can_multiply(count, stride)) {
        return false;  // count * stride would wrap uint32_t — reject outright
    }
    uint32_t const total = count * stride;  // now provably safe
    return total <= available;
}
```

## Headers

- `statusbar/safe_arith/safe_arith.hpp` — the whole module: `can_multiply`,
  `can_add`, and the `STATUSBAR_SAFE_ARITH_HAS_BUILTINS` macro.

## Dependencies

- **Statusbar modules:** none. `safe_arith` is an `INTERFACE`
  (header-only) module with no statusbar dependencies, so any module may
  use it freely.
- **System / external:** `<concepts>` (for `std::unsigned_integral`) and
  `<limits>` (for the portable fallback) from the standard library;
  optionally the compiler's `__builtin_mul_overflow` /
  `__builtin_add_overflow`.

## Notes & caveats

- **Unsigned only.** Both predicates are constrained to
  `std::unsigned_integral`; signed overflow is undefined behaviour and is
  deliberately out of scope. Convert or range-check signed inputs first.
- **Predicates, not operations.** They return a `bool`; they never compute
  `a * b` / `a + b` or hand back the result. Perform the arithmetic
  yourself once the predicate is true.
- **Mixed widths don't deduce.** `T` is a single deduced type, so
  `can_multiply(a, b)` with differing integer types fails to compile.
  Cast both operands to the target width first — that width is also the
  one the predicate checks against.
- **`constexpr`-usable.** Both are `constexpr`, so they can guard
  compile-time computations as well as runtime input validation.

## Further reading

- [`buffer`](BUFFER_MODULE.md) — the (de)serialization layer where
  wire-format length/count fields originate and where these guards are
  most often needed.
- [`status`](STATUS_MODULE.md) — for turning a failed overflow check into
  a typed error to propagate to the caller.
