<!-- Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com> -->
<!-- SPDX-License-Identifier: MIT -->


# Custom Error Code Usage Examples

This document demonstrates how to use the custom `std::error_code` system in the
IEEE buffer library.

## Overview

The library provides a custom error enum:

- `BufferError` — for buffer operations (parsing, appending, storing).

This enum is registered as an `is_error_code_enum`, so it converts implicitly to
`std::error_code`. Buffer APIs return their results through `Status` (a
type alias for `std::expected<void, std::error_code>`), `StatusValue<T>` (an
`std::expected<T, std::error_code>`), or — on the chained builders — a sticky
`std::error_code` exposed via `.error()`.

## Basic Usage

### 1. Appending and Checking the Returned `Status`

`MutableBuffer::append()` only knows how to append raw byte spans. To append a
typed value (including the IEEE network-ordered types `octet_t`, `doublet_t`,
`quadlet_t`, `octlet_t`), construct a `BufferSerializerBuilder` over the buffer.
The builder returns a reference for chaining and tracks the first error
internally.

```cpp
#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"

#include <array>
#include <cstdint>
#include <print>

using namespace statusbar;
using namespace statusbar::ieee;

std::array<uint8_t, 16> data{};
MutableBuffer buf{data};
BufferSerializerBuilder builder{buf};

builder.append(quadlet_t{0x12345678});

if (!builder) {
    std::println("Error: {}", builder.error().message());
}
```

The same builder can append byte spans through the underlying `MutableBuffer`:

```cpp
std::array<uint8_t, 3> src{0xAA, 0xBB, 0xCC};
auto status = buf.append(std::span<uint8_t const>{src});  // returns Status
if (!status) {
    std::println("append failed: {}", status.error().message());
}
```

### 2. Comparing Errors

`std::error_code` compares directly against a `BufferError` enumerator.

```cpp
std::array<uint8_t, 2> small{};
MutableBuffer tiny{small};
BufferSerializerBuilder b{tiny};
b.append(quadlet_t{0x12345678});  // does not fit

if (b.error() == BufferError::insufficient_space) {
    std::println("Buffer is full");
}
```

The three values currently defined are `insufficient_space`, `invalid_offset`,
and `insufficient_data`.

## BufferError Enum Values

| Error Code | Description |
|------------|-------------|
| `insufficient_space` | Not enough space in buffer for write operations |
| `invalid_offset` | Offset is out of bounds |
| `insufficient_data` | Not enough data in buffer for read operations |

## Common Patterns

### Pattern 1: Single-Shot `Status` Check

`MutableBuffer::append(std::span<uint8_t const>)`, `MutableBuffer::store(...)`,
`MutableBuffer::advance(...)` and `BufferDeserializer::parse(T*)` all return
`Status` (or `StatusValue<T>`). Treat them like any other `std::expected`.

```cpp
auto status = buf.append(std::span<uint8_t const>{src});
if (!status) {
    std::println(stderr, "Error: {}", status.error().message());
    return status;  // Status is std::expected<void, std::error_code>
}
```

### Pattern 2: Branching on a Specific Error

```cpp
auto status = buf.advance(needed);
if (!status) {
    if (status.error() == BufferError::insufficient_space) {
        // Caller can grow or flush the buffer here.
    } else {
        std::println(stderr, "Unexpected error: {}", status.error().message());
    }
}
```

### Pattern 3: Chained Serialization with `BufferSerializerBuilder`

The builder swallows further operations once an error sticks, so you only need
to check the result at the end. The error is exposed two ways:
`builder.error()` (a `std::error_code const&`) or `builder.status()` (a
`Status`).

```cpp
std::array<uint8_t, 16> data{};
MutableBuffer buf{data};
BufferSerializerBuilder builder{buf};

builder
    .append(octet_t{0x01})
    .append(doublet_t{0x0203})
    .append(quadlet_t{0x04050607});

if (!builder) {
    std::println("Build failed: {}", builder.error().message());
    return builder.status();  // Status: failure(error_code) or success()
}
```

`BufferSerializerBuilderWithStorage<N>` and `BufferSerializerBuilderWithBuffer`
are convenience wrappers that own/wrap the underlying buffer for you.

### Pattern 4: Single-Field Deserialization

`BufferDeserializer::parse(T*)` returns `Status`. It works with trivially
copyable types (raw memcpy) and with `SerializableStruct` types such as
`Eui48`, `Eui64`, `VlanTag`, and `EthernetFrame` (protocol-aware load).

```cpp
std::span<uint8_t const> input = /* ... */;
BufferDeserializer d{input};

Eui48 mac{};
auto status = d.parse(&mac);
if (!status) {
    if (status.error() == BufferError::insufficient_data) {
        std::println("Need more data");
    } else {
        std::println("Parse error: {}", status.error().message());
    }
    return status;
}

std::println("MAC: {}", to_string(mac));  // or format_to(out, mac);
```

### Pattern 5: Chained Deserialization with `BufferDeserializerBuilder`

`BufferDeserializerBuilder` mirrors the serializer builder: it is chainable,
sticky on errors, and exposes the first failure via `.error()` / `.status()`.
It also supports `.skip(n)` and `.seek(Position, Length)`.

```cpp
std::span<uint8_t const> input = /* ... */;
BufferDeserializerBuilder d{input};

Eui48 dst{};
Eui48 src{};
doublet_t ethertype{};

d.parse(&dst).parse(&src).parse(&ethertype);

if (!d) {
    std::println("Parse failed: {}", d.error().message());
    return d.status();
}
```

### Pattern 6: Error Categories

Every `BufferError` carries `buffer_error_category()` (whose `name()` is
`"statusbar.buffer"`).

```cpp
std::error_code ec = some_operation_returning_status().error();
if (ec.category() == buffer_error_category()) {
    std::println("Buffer-layer failure: {}", ec.message());
}
```

### Pattern 7: Creating Error Codes Directly

```cpp
std::error_code ec1 = BufferError::insufficient_space;             // implicit
std::error_code ec2 = make_error_code(BufferError::invalid_offset); // explicit
```

## Integration with `std::expected`

`Status` is already `std::expected<void, std::error_code>` and `StatusValue<T>`
is `std::expected<T, std::error_code>`, so all the C++23 monadic helpers
(`and_then`, `or_else`, `transform`, `value_or`, ...) are available directly.
`success()` and `failure()` are tiny constructor helpers.

```cpp
auto parse_mac(std::span<uint8_t const> buf) -> StatusValue<Eui48> {
    BufferDeserializer d{buf};
    Eui48 mac{};
    if (auto status = d.parse(&mac); !status) {
        return forward_failure(status);  // re-wraps as StatusValue<Eui48>
    }
    return success(mac);
}

auto result = parse_mac(input);
if (result) {
    std::println("MAC: {}", to_string(*result));
} else {
    std::println("Parse failed: {}", result.error().message());
}
```

## Best Practices

1. **Treat results as `std::expected`.** `Status`, `StatusValue<T>`, and
   builder `.status()` all expose the same `has_value()` / `error()` API.
2. **Use builders for chains.** `BufferSerializerBuilder` and
   `BufferDeserializerBuilder` short-circuit on the first error so you only
   check once at the end.
3. **Compare against `BufferError` enumerators**, not magic numbers, when the
   caller can recover (e.g. grow the buffer on `insufficient_space`).
4. **Provide context** by including `ec.message()` in logs.
5. **Don't throw.** These APIs are `noexcept`; check return values.
6. **Forward failures** with `forward_failure(other_expected)` when crossing
   `StatusValue<T>` boundaries.

## Error Message Examples

```
BufferError::insufficient_space -> "Insufficient space in buffer"
BufferError::invalid_offset     -> "Invalid offset"
BufferError::insufficient_data  -> "Insufficient data in buffer"
```
