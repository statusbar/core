# statusbar/sg14

Vendored single-header libraries from the ISO C++ Study Group 14 (Low Latency)
collection, maintained by Arthur O'Dwyer at
[Quuxplusone/SG14](https://github.com/Quuxplusone/SG14).

These are to be used throughout the project in places that need a fixed-size
std::function replacement without heap allocation.

## Contents

| File | Upstream path | License |
|---|---|---|
| `inplace_vector.h` | `include/sg14/inplace_vector.h` | Boost Software License 1.0 |
| `inplace_function.h` | (SG14 repository, `inplace_function`) | Boost Software License 1.0 |

## Provenance

Both files were imported from
`https://github.com/Quuxplusone/SG14` on **2026-04-05**.

the local import commits are:

- `inplace_vector.h`: imported in commit `f162d8e7` (2026-04-05 21:37 PDT)
- `inplace_function.h`: imported in commit `e7525ee2` (2026-04-05 21:12 PDT)

## Local modifications

Two deliberate changes from upstream, applied immediately after import:

1. **Reformatted** to match the project's clang-format style

2. **Namespace renamed** from `sg14::` to `statusbar::sg14::` so the types
   don't collide with any other vendored copy of SG14 that a downstream
   consumer might also pull in

No functional changes. If upstream ships a bug fix, the re-import procedure
is: (a) re-fetch the file, (b) apply clang-format, (c) change the `namespace sg14`
declaration to `namespace statusbar::sg14`. A side-by-side diff against the
commits above will surface any additional divergence.

