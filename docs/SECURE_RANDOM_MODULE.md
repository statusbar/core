[← back to module index](README.md)

# secure_random

Cryptographic-quality random byte generation — a single
`secure_random_bytes()` call backed by the platform CSPRNG syscall, with
no fallback to weaker sources.

## Overview

The `secure_random` module exposes one function in namespace `statusbar`:

```cpp
void secure_random_bytes(std::span<uint8_t> out) noexcept;
```

It fills `out` with bytes from the operating system's cryptographically
secure generator — `getrandom(2)` on Linux, `arc4random_buf(3)` on
macOS / *BSD. Use it for keys, nonces, salts, session identifiers, and
anywhere unpredictability matters; do **not** use `std::rand`, a
`std::mt19937` seeded from the clock, or similar for these.

The module's defining property is that it has **no weak fallback**. If the
entropy syscall fails irrecoverably the function aborts the process rather
than return predictable bytes from a degraded source — silently handing
back guessable "random" data is a worse outcome than a crash. A platform
with no supported entropy source is a compile-time `#error`, not a
silent downgrade.

## Key types

- `secure_random_bytes(std::span<uint8_t> out) noexcept` — fill `out` with
  CSPRNG bytes. An empty span is a no-op. Safe to call from any thread.
  Backed by `getrandom(2)` (Linux) or `arc4random_buf(3)` (macOS / *BSD);
  the Linux path loops to handle short reads and retries on `EINTR`.

## Quick example

```cpp
#include "statusbar/secure_random/secure_random.hpp"

#include <array>
#include <cstdint>

using namespace statusbar;

int main()
{
    std::array<uint8_t, 16> key{};
    secure_random_bytes(key);  // 128 bits of CSPRNG output
    // ... derive / use the key ...
    return 0;
}
```

## Headers

- `statusbar/secure_random/secure_random.hpp` — declares
  `secure_random_bytes()`. The implementation lives in
  `secure_random.cpp`, so link the `secure_random` library (it is not
  header-only).

## Dependencies

- **Statusbar modules:** none.
- **System / external:** `<span>` and `<cstdint>` in the header; the
  implementation uses `<sys/random.h>` (`getrandom`) on Linux and
  `arc4random_buf` on macOS / *BSD. Any other platform fails to compile.

## Notes & caveats

- **Aborts instead of degrading.** An unrecoverable entropy-syscall
  failure calls `std::abort()`. The function never returns weak or partial
  randomness. Callers do not need to (and cannot) check for an error
  result — there isn't one.
- **First call may block briefly (Linux).** `getrandom(2)` can block once,
  shortly after boot, until the kernel entropy pool is initialised.
  Subsequent calls never block. Avoid calling it on a hard-realtime path
  during early boot; pre-warm it during startup if that matters.
- **Not constant-time, not zeroizing.** The function only fills the buffer.
  It does not constant-time-compare and does not wipe the output on
  destruction — clear sensitive buffers yourself when done.
- **Unsupported platforms are a build error.** The `#error` is intentional:
  adding a platform means wiring up a real CSPRNG, never a placeholder.

## Further reading

- The `statusbar-crypto` package (separate repository) provides the
  higher-level cryptographic primitives that consume this entropy source.
