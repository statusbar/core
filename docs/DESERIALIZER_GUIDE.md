<!-- Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com> -->
<!-- SPDX-License-Identifier: MIT -->


# Deserializer Guide

> ---
>
> **NOTE: Scope of this guide**
>
> Everything documented here — `make_deserializer()`, `field<T>()`,
> `skip<N>()`, `CompiledDeserializer`, `CompiledSerializer`,
> `make_serializer()`, `serialize_field<T>()`, `serialize_skip<N>()`,
> `serialize_conditional_field<T>()` — is implemented in
> `statusbar/buffer/buffer_serdes_compiled.hpp`. The non-chained
> `BufferDeserializer` and the chained `BufferDeserializerBuilder` live in
> `statusbar/buffer/buffer_deserializer_builder.hpp`.
>
> The compiled **deserializer** intentionally does **not** ship a
> `conditional_field<T>()` helper today — only the serializer side has a
> conditional variant (`serialize_conditional_field<T>()`). To skip optional
> trailing fields on the read side, run a second pass with a different
> deserializer, or fall back to `BufferDeserializerBuilder` and branch on the
> already-parsed bits before calling `.parse(...)` again.
>
> ---

This guide covers the `CompiledDeserializer` machinery for high-performance
network-protocol parsing, plus the `CompiledSerializer` counterpart used to
build the same wire formats.

## Table of Contents

1. [Overview](#overview)
2. [Basic Field Parsing](#basic-field-parsing)
3. [Skipping Fields and Padding](#skipping-fields-and-padding)
4. [Building Packets with CompiledSerializer](#building-packets-with-compiledserializer)
5. [Error Handling](#error-handling)
6. [Performance Notes](#performance-notes)

---

## Overview

`CompiledDeserializer` (defined in
`statusbar/buffer/buffer_serdes_compiled.hpp`) is a compile-time list of field
extractors. Define one once, reuse it across many buffers. Each extractor pulls
its value through `BufferDeserializer`, so:

- **IEEE network-ordered types** (`octet_t`, `doublet_t`, `quadlet_t`,
  `octlet_t`) are stored internally as a byte array in network byte order and
  convert to host order on read. Parsing them via `field<doublet_t>(...)` does
  the right thing — the bytes copy verbatim into the wrapper and the callback
  receives the host-order value through the implicit conversion.
- **`SerializableStruct` types** (`Eui48`, `Eui64`, `VlanTag`,
  `EthernetFrame`, ...) go through the protocol ADL load path.

### Key Features

- **Reusable**: define the structure once at the call site, parse many buffers.
- **Type-safe callbacks**: each field has its own typed lambda.
- **Short-circuiting**: the `&&` fold stops at the first failing extractor; no
  callbacks fire past the first error.
- **No allocations**: everything is stack-based.

---

## Basic Field Parsing

### Ethernet Frame Header

```cpp
#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"

#include <array>
#include <cstdint>

using namespace statusbar;
using namespace statusbar::ieee;

Eui48 dst_mac{};
Eui48 src_mac{};
doublet_t ethertype{};

auto const ethernet_deserializer = make_deserializer(
    field<Eui48>([&dst_mac](Eui48 mac) { dst_mac = mac; }),
    field<Eui48>([&src_mac](Eui48 mac) { src_mac = mac; }),
    field<doublet_t>([&ethertype](doublet_t type) { ethertype = type; }));

std::array<uint8_t, 14> frame{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // dest MAC (broadcast)
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55,  // src MAC
    0x08, 0x00                            // EtherType (IPv4)
};

if (auto status = ethernet_deserializer.parse(std::span<uint8_t const>{frame}); status) {
    if (ethertype == 0x0800) {
        // EtherType is host-order on read, no manual byteswap needed.
    }
}
```

### IPv4 Header (Fixed 20 Bytes)

```cpp
uint8_t version = 0;
uint8_t ihl = 0;
octet_t dscp_ecn{};
doublet_t total_length{};
doublet_t identification{};
doublet_t flags_offset{};
octet_t ttl{};
octet_t protocol{};
doublet_t header_checksum{};
quadlet_t src_ip{};
quadlet_t dst_ip{};

auto const ipv4_deserializer = make_deserializer(
    // Version (upper 4 bits) + IHL (lower 4 bits) packed in one byte.
    field<octet_t>([&version, &ihl](octet_t v) {
        uint8_t const raw = v;
        version = raw >> 4;
        ihl     = raw & 0x0F;
    }),
    field<octet_t>([&dscp_ecn](octet_t v) { dscp_ecn = v; }),
    field<doublet_t>([&total_length](doublet_t v) { total_length = v; }),
    field<doublet_t>([&identification](doublet_t v) { identification = v; }),
    field<doublet_t>([&flags_offset](doublet_t v) { flags_offset = v; }),
    field<octet_t>([&ttl](octet_t v) { ttl = v; }),
    field<octet_t>([&protocol](octet_t v) { protocol = v; }),
    field<doublet_t>([&header_checksum](doublet_t v) { header_checksum = v; }),
    field<quadlet_t>([&src_ip](quadlet_t v) { src_ip = v; }),
    field<quadlet_t>([&dst_ip](quadlet_t v) { dst_ip = v; }));
```

### Plain `uintN_t` Fields

`field<T>(...)` also works with plain trivially-copyable types like `uint8_t`,
`uint16_t`, `uint32_t`. Those use the raw `memcpy` parse path and so receive
the value in **host byte order of the source bytes** — no swapping. Use the
IEEE wrappers (`doublet_t`, `quadlet_t`, ...) when you want network-to-host
conversion.

```cpp
uint8_t  type   = 0;
uint16_t length = 0;
uint32_t data   = 0;

auto const packet_deserializer = make_deserializer(
    field<uint8_t>([&type](uint8_t v) { type = v; }),
    field<uint16_t>([&length](uint16_t v) { length = v; }),
    field<uint32_t>([&data](uint32_t v) { data = v; }));

std::array<uint8_t, 7> buf1{0xAA, 0x00, 0x10, 0x12, 0x34, 0x56, 0x78};
(void)packet_deserializer.parse(std::span<uint8_t const>{buf1});
```

---

## Skipping Fields and Padding

`skip<N>()` consumes N bytes without invoking a callback. It returns
`BufferError::insufficient_data` if fewer than N bytes remain.

### Basic Padding

```cpp
auto const deserializer = make_deserializer(
    field<doublet_t>([](doublet_t header) { /* ... */ }),
    skip<4>(),  // 4 reserved bytes
    field<doublet_t>([](doublet_t payload) { /* ... */ }));
```

### Skip the Ethernet Header, Parse Part of the IPv4 Header

```cpp
uint8_t ip_version = 0;
octet_t protocol{};
quadlet_t src_ip{};
quadlet_t dst_ip{};

auto const ipv4_after_ethernet = make_deserializer(
    skip<14>(),  // Ethernet: 6 + 6 + 2

    field<octet_t>([&ip_version](octet_t v) { ip_version = static_cast<uint8_t>(v) >> 4; }),
    skip<8>(),   // DSCP/ECN + total_length + ID + flags/offset
    field<octet_t>([](octet_t /*ttl*/) {}),
    field<octet_t>([&protocol](octet_t v) { protocol = v; }),
    skip<2>(),   // header checksum
    field<quadlet_t>([&src_ip](quadlet_t v) { src_ip = v; }),
    field<quadlet_t>([&dst_ip](quadlet_t v) { dst_ip = v; }));
```

### Useful Skip Sizes

```cpp
// Ethernet
skip<6>()   // one MAC
skip<14>()  // full untagged header

// IPv4 (no options)
skip<20>()  // full minimal header

// UDP
skip<8>()   // full header

// TCP
skip<20>()  // full minimal header (no options)
```

---

## Building Packets with `CompiledSerializer`

`make_serializer(...)` builds a packet using value-supplier lambdas. Use
`serialize_field<T>()` for required fields, `serialize_skip<N>()` for padding,
and `serialize_conditional_field<T>(cond_fn, value_fn)` for fields that are
present only when a runtime predicate is true.

```cpp
std::array<uint8_t, 64> packet{};
MutableBuffer buf{packet};

uint8_t  type    = 0x01;
uint16_t length  = 0x0010;
uint32_t payload = 0x12345678;

auto const packet_serializer = make_serializer(
    serialize_field<uint8_t>([&] { return type; }),
    serialize_field<uint16_t>([&] { return length; }),
    serialize_skip<2>(),  // 2 reserved zero bytes
    serialize_field<uint32_t>([&] { return payload; }));

auto status = packet_serializer.serialize(buf);
// status.has_value() => buf.size() == 1 + 2 + 2 + 4
```

### Conditional Fields (Serializer Side)

```cpp
uint8_t flags = 0x01;          // bit 0 controls whether the optional field is present
uint16_t optional_field = 0xABCD;
uint32_t mandatory_payload = 0x12345678;

auto const ser = make_serializer(
    serialize_field<uint8_t>([&] { return flags; }),
    serialize_conditional_field<uint16_t>(
        [&] { return (flags & 0x01) != 0; },
        [&] { return optional_field; }),
    serialize_field<uint32_t>([&] { return mandatory_payload; }));
```

When the condition returns false, the conditional field writes zero bytes —
the resulting packet is correspondingly shorter.

---

## Error Handling

`CompiledDeserializer::parse(...)` and `CompiledSerializer::serialize(...)`
return a `Status` (`std::expected<void, std::error_code>`). On failure, the
remaining extractors are not invoked.

```cpp
auto const deserializer = make_deserializer(
    field<Eui48>([](Eui48 /*mac*/) { /* ... */ }),
    field<doublet_t>([](doublet_t /*type*/) { /* ... */ }));

if (auto status = deserializer.parse(buffer); !status) {
    if (status.error() == BufferError::insufficient_data) {
        // Truncated packet — fewer bytes than the structure requires.
    }
}
```

### `skip<N>()` Error Behaviour

`skip<N>()` propagates `BufferError::insufficient_data` exactly like a
`field<T>()` whose `sizeof(T)` is too big to fit.

```cpp
auto const deserializer = make_deserializer(
    field<octet_t>([](octet_t /*v*/) {}),
    skip<100>());

std::array<uint8_t, 10> small{};
auto status = deserializer.parse(std::span<uint8_t const>{small});
// status.has_value() == false
// status.error()    == BufferError::insufficient_data
```

### Builder-Style Alternative

For more dynamic parsing (runtime offsets, branching on a previously read
field), prefer `BufferDeserializerBuilder` over `CompiledDeserializer`. It
exposes a chained `.parse(&v)` / `.skip(n)` / `.seek(Position, Length)` API
with the same sticky-error semantics.

```cpp
BufferDeserializerBuilder d{input};

uint8_t  tag = 0;
uint32_t value = 0;
d.parse(&tag).skip(1).parse(&value);

if (!d) {
    return d.status();  // forwards as Status
}
```

---

## Performance Notes

- **No runtime overhead** vs. hand-written parse code: each extractor is a
  fully-inlined `BufferDeserializer::parse(&value)` call.
- **Short-circuiting**: a failure in field K skips the remaining fields and
  their callbacks (see the `&&` fold in `parse_impl`).
- **Stack-only**: no allocations during parse or serialise.
- **Cache-friendly**: sequential read/write through one `std::span`.

### Comparison with Manual Byte Twiddling

```cpp
// Traditional approach — easy to get wrong, byte-order bugs hide here:
void parse_old(uint8_t const* data, size_t len) {
    if (len < 20) return;
    uint8_t  version    = data[0] >> 4;
    uint16_t total_len  = (uint16_t(data[2]) << 8) | data[3];  // manual swap
    // ... and on, and on
}

// CompiledDeserializer approach — define once, reuse, byte order is free:
auto const ipv4_deserializer = make_deserializer(/* ...as above... */);
ipv4_deserializer.parse(packet1);
ipv4_deserializer.parse(packet2);
```

---

## Protocol Sizes (Quick Reference)

| Protocol | Layer | Fixed Size | Notes |
|----------|-------|-----------:|-------|
| Ethernet (untagged) | L2 | 14 | dest + src + ethertype |
| Ethernet + 802.1Q VLAN | L2 | 18 | adds 4-byte VLAN tag |
| IPv4 (no options) | L3 | 20 | IHL == 5 |
| TCP (no options) | L4 | 20 | data offset == 5 |
| UDP | L4 | 8 | src + dst + length + checksum |
| ICMP (header) | L4 | 8 | type + code + checksum + rest |
