[← back to module index](README.md)

# csv

RFC 4180 CSV output primitives. Generic; no domain knowledge of any
specific record shape.

## Overview

The `csv` module is a single thin class — `CsvWriter` — that streams
rows to a file in RFC 4180 form. Construction opens the file (binary
mode) and writes the header row immediately; each subsequent
`write_row()` emits one data row, escaping any field that contains
comma, double-quote, CR, or LF by wrapping it in double-quotes and
doubling internal double-quotes. Line terminators are CRLF.

The writer owns a stdio `FILE*` and is intentionally non-movable so the
pointer stays at a stable address while writes are in progress.
Construction is the one place in the project that reports failure by
throwing (`std::system_error`) rather than returning a `Status`;
`write_row()` and `flush()` both return `Status` like the rest of the
codebase.

## Key types

- `CsvWriter` — the entire public surface. Construct with a path and a
  `std::span<std::string_view const>` header; call `write_row()` per
  data row; call `flush()` to push stdio's buffer to the OS; read
  `row_count()` for the number of data rows written so far.

The header span is consumed during construction (used only to emit
the first row), so its backing storage does not need to outlive the
constructor call. The per-row span passed to `write_row()` is likewise
consumed before the call returns — pass arrays of `std::string_view`
that point at whatever caller-side storage you already have.

## Quick example

```cpp
#include "statusbar/csv/csv.hpp"

#include <array>
#include <string_view>

using namespace statusbar;

int main()
{
    std::array<std::string_view, 3> const header{"a", "b", "c"};
    csv::CsvWriter w{"/tmp/example.csv", header};

    std::array<std::string_view, 3> const row{"1", "hi,there", "ok"};
    auto const st = w.write_row(row);
    if (!st) {
        return 1;
    }
    (void)w.flush();
    // File now contains: a,b,c\r\n1,"hi,there",ok\r\n
    return 0;
}
```

## Headers

- `statusbar/csv/csv.hpp` — module header (pulls in `csv_writer.hpp`). This is what consumers should `#include`.
- `statusbar/csv/csv_writer.hpp` — declaration of `CsvWriter`.

## Dependencies

- **Statusbar modules:** [`status`](STATUS_MODULE.md) (`write_row()` and `flush()` return `Status`).
- **System / external:**
  - `<cstdio>` — the underlying `FILE*` used for buffered writes.
  - `<span>`, `<string>`, `<string_view>`, `<cstdint>` from the C++20/23 standard library.

## Notes & caveats

- Construction throws `std::system_error` on open or header-write
  failure. This is the project's documented exception to the
  `Status`-based convention; callers that prefer `Status` should wrap
  the constructor in try/catch.
- `CsvWriter` is non-copyable *and* non-movable. Construct it in place
  (e.g. as a local, or via `std::optional::emplace`) — do not try to
  return it by value.
- Row terminators are CRLF (`\r\n`), per RFC 4180, regardless of host
  platform.
- Empty fields are written as the empty string, not as `""`. Quoting
  only kicks in for fields containing comma, double-quote, CR, or LF.
- `write_row()` leaves the file in an unspecified state on write
  failure; stop writing on the first error rather than trying to
  recover mid-stream.
- `flush()` pushes stdio's buffer to the OS but does not `fsync()`. It
  is intended for streaming writers that want recent rows to survive an
  unclean process exit, not for durability guarantees.

## Further reading

- [`status`](STATUS_MODULE.md) — error-reporting type returned by `write_row()` and `flush()`.
