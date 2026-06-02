[← back to module index](README.md)

# status

The foundational error-handling primitive used by every other module:
type aliases over `std::expected<T, std::error_code>` plus a small set of
helpers for constructing, querying, and forwarding error results.

## Overview

The `status` module sits at the bottom of the dependency graph. Every
other statusbar module reports failure with one of two type aliases:

- `Status` — operation that returns no value.
- `StatusValue<T>` — operation that returns a value of type `T` on success.

Both are spellings of `std::expected<…, std::error_code>`, so all of the
C++23 monadic operations (`and_then`, `or_else`, `transform`, `value_or`,
etc.) are available as ordinary methods on a `Status` / `StatusValue<T>`.

The module adds three things on top of `std::expected`:

1. `success()` / `success(value)` for constructing the happy path.
2. `failure(...)` overloads that return `std::unexpected<std::error_code>`,
   implicitly convertible to *any* `Status` / `StatusValue<T>` at the call
   site — so a single `return failure(std::errc::invalid_argument);`
   works regardless of the function's success type.
3. `forward_failure(expected)` for propagating an error code from a
   `StatusValue<A>` into a `StatusValue<B>` (or `Status`) without naming
   either type.

The `failure` overloads accept `std::error_code` directly, an
`std::errc`, or any user-defined enum that satisfies
`std::is_error_code_enum_v`. That last overload is how modules like
`buffer` expose typed error categories (e.g. `BufferError`) without this
module ever knowing about them.

## Key types

- `Status` — alias for `std::expected<void, std::error_code>`. The return type for operations that report success or failure but carry no value.
- `StatusValue<T>` — alias for `std::expected<T, std::error_code>`. The return type for operations that produce a value of type `T` on success.
- `success()` — produces an empty (successful) `Status`.
- `success(value)` — produces a `StatusValue<std::decay_t<T>>` holding `value`.
- `failure(std::error_code)` / `failure(std::errc)` / `failure(E)` (for `std::is_error_code_enum_v<E>` enums) — each returns `std::unexpected<std::error_code>`, which implicitly converts to any `Status` / `StatusValue<T>`.
- `is_success(s)` / `is_failure(s)` — `constexpr` predicates over any expected-shaped type; thin sugar over `.has_value()` for readability.
- `forward_failure(expected)` — extracts the `error_code` from any `std::expected<T, std::error_code>` in error state and re-wraps it as `std::unexpected`, ready to return into a differently-typed result. Asserts the source is in the failure state.

## Quick example

```cpp
#include "statusbar/status/status.hpp"

#include <string_view>

using namespace statusbar;

StatusValue<int> parse_port(std::string_view str)
{
    if (str.empty()) {
        return failure(std::errc::invalid_argument);
    }
    int value = 0;  // (real code would parse str here)
    return success(value);
}

Status configure(std::string_view port_str)
{
    auto port = parse_port(port_str);
    if (!port) {
        return forward_failure(port);  // StatusValue<int> -> Status
    }
    return success();
}
```

## Headers

- `statusbar/status/status.hpp` — the only header. This is what consumers should `#include`.

## Dependencies

- **Statusbar modules:** none. `status` is the leaf of the dependency graph; every other module depends on it.
- **System / external:** `<expected>`, `<system_error>`, `<type_traits>`, `<utility>`, `<cassert>` from the C++23 standard library.

## Notes & caveats

- `Status` and `StatusValue<T>` are *aliases*, not new types. `Status` **is** `std::expected<void, std::error_code>`, so anything that works on `std::expected` works here — including the C++23 monadic methods (`and_then`, `or_else`, `transform`, `value_or`). The module deliberately does not re-export these; use the `std::expected` member functions directly.
- `failure(...)` returns `std::unexpected<std::error_code>` rather than a `Status` / `StatusValue<T>`. This is what lets the same `return failure(...);` expression work in functions with different success types — the implicit conversion happens at the return statement.
- The `failure(E)` enum overload is constrained on `std::is_error_code_enum_v<E>`. To plug a new error enum in, specialise `std::is_error_code_enum` and provide an ADL `make_error_code(E)` (see `BufferError` in the `buffer` module for the canonical pattern, and `ERROR_HANDLING_EXAMPLES.md` for full examples).
- `forward_failure` asserts that the source is in the failure state. Always guard the call with `if (!result) return forward_failure(result);` — never call it on a value you have not yet checked.
- `success(value)` decays its argument, so `success("literal")` yields `StatusValue<char const*>`. Construct the target type explicitly (`success(std::string{"literal"})`) when you need something other than the decayed type.
- `is_success` / `is_failure` are templated on anything with `.has_value()`, not just `Status` / `StatusValue<T>`. They work on any `std::expected` or `std::optional`.

## Further reading

- [ERROR_HANDLING_EXAMPLES.md](ERROR_HANDLING_EXAMPLES.md) — worked examples of defining custom `std::error_code` enums (the pattern used throughout statusbar) and consuming them via `Status` / `StatusValue<T>`.
