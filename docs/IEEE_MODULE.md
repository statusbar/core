[← back to module index](README.md)

# ieee

Network byte-order integer wrappers and IEEE 802 wire-format structs
(MAC addresses, VLAN tags, Ethernet II frames) plus the protocol
constants that name them.

## Overview

The `ieee` module holds on-the-wire types that follow IEEE 802
conventions. Everything is big-endian — values are *stored* in
network byte order so `memcpy` / `std::bit_cast` into a packet
buffer works without any conversion step.

The foundation is `IeeeOrderedUInt<T>`: a wrapper that holds its
value as `std::array<uint8_t, sizeof(T)>` in network order, swaps on
read/write, and offers bit-field, flag, and span accessors. The
aliases `octet_t`, `doublet_t`, `quadlet_t`, `octlet_t` cover the
four widths IEEE specs use. Byte-array storage gives **alignment 1**,
so these slot into packed protocol structs cleanly.

`Eui48` / `Eui64` are the standard 6- and 8-byte hardware
identifiers, with I/G and U/L bit tests, `to_modified_eui64()`
(inserts `0xFFFE` per IPv6/AVB convention), and `from_string()`
parsers. `VlanTag` carries the 802.1Q TPID/TCI pair with PCP/DEI/VID
accessors. `EthernetFrame` combines two MACs, an optional VLAN tag,
and an EtherType into a 14- or 18-byte header.

Wire I/O integrates with `buffer`: `ieee_buffer.hpp` adds `load` /
`store` overloads for `IeeeOrderedUInt`, and `ieee_ethernet.hpp`
opts the struct types into `is_serializable_fixed_struct` /
`is_serializable_variable_struct` so `BufferSerializerBuilder` and
`CompiledDeserializer` append/parse them by ADL. `wire_size`,
`can_load`, `can_store`, `load_unchecked`, `store_unchecked` are all
provided. `ieee_protocols.hpp` collects `ETHERTYPE_*`, `IP_PROTO_*`,
header sizes, and multicast MACs; `ieee_names.hpp` turns an
EtherType into a readable string; `ieee_ethernet_format.hpp` adds
`format_to` overloads, split out so data-only headers skip
`<format>`.

## Key types

- `IeeeOrderedUInt<T>` — network-order wrapper with `get` / `set`, `network_value` / `set_network`, `span`, flag helpers (`has_flag`, `set_flag`, `clear_flag`, `toggle_flag`), and bit-field accessors (`get_bits`, `set_bits`).
- `octet_t`, `doublet_t`, `quadlet_t`, `octlet_t` — 1/2/4/8-byte aliases of `IeeeOrderedUInt`.
- `Eui48` — 6-byte MAC with `to_uint64` / `from_uint64`, `is_multicast`, `is_locally_administered`, `is_set`, `to_modified_eui64`.
- `Eui64` — 8-byte identifier derived from `IeeeOrderedUInt<uint64_t>` with the same predicate set.
- `VlanTag` — 802.1Q tag (`tpid`, `tci`) with `get_vid` / `set_vid`, `get_pcp` / `set_pcp`, `get_dei` / `set_dei`; free helpers `make_vlan_tag`, `make_empty_vlan_tag`, `is_tagged`.
- `EthernetFrame` — `dest_mac` + `src_mac` + `vlan_tag` + `ethertype`, with `is_valid` and `wire_size` (14 or 18 depending on tag).
- `ethertype_name(uint16_t)` — string lookup; falls back to `"0x1234"` hex for unknown values.

## Quick example

```cpp
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/buffer/buffer.hpp"

using namespace statusbar;
using namespace statusbar::ieee;

int main()
{
    EthernetFrame eth{};
    eth.dest_mac = Eui48{0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E};
    eth.src_mac = Eui48{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    eth.vlan_tag = make_vlan_tag(/*vid=*/100, /*dei=*/false, /*pcp=*/5);
    eth.ethertype = protocols::ETHERTYPE_AVTP;

    BufferSerializerBuilderWithStorage<32> writer;
    writer.append(eth);
    if (!writer.status()) {
        return 1;
    }
    quadlet_t first_word;
    auto loaded = load(writer.get_span().subspan(0, 4), &first_word);
    return loaded ? 0 : 1;
}
```

## Headers

- `statusbar/ieee/ieee.hpp` — module header; pulls in base, buffer, ethernet, names, protocols.
- `statusbar/ieee/ieee_base.hpp` — `IeeeOrderedUInt<T>`, the `octet_t` / `doublet_t` / `quadlet_t` / `octlet_t` aliases, and `is_ieee_*` traits.
- `statusbar/ieee/ieee_buffer.hpp` — `load` / `store` overloads for `IeeeOrderedUInt` values, arrays, and spans (the `IeeeNetworkType` concept).
- `statusbar/ieee/ieee_ethernet.hpp` — `Eui48`, `Eui64`, `VlanTag`, `EthernetFrame`; their `wire_size` / `can_load` / `can_store` / `load_unchecked` / `store_unchecked` overloads.
- `statusbar/ieee/ieee_ethernet_format.hpp` — `format_to` for `Eui48`, `Eui64`, `EthernetFrame`; include only when you need `<format>`.
- `statusbar/ieee/ieee_format.hpp` — header that adds the format overloads to `ieee.hpp`.
- `statusbar/ieee/ieee_names.hpp` — `ethertype_name(uint16_t)`.
- `statusbar/ieee/ieee_protocols.hpp` — `ETHERTYPE_*`, `IP_PROTO_*`, header-size constants, multicast MACs.

## Dependencies

- **Statusbar modules:** [`buffer`](BUFFER_MODULE.md) (every header pulls in `statusbar/buffer/buffer.hpp` for `BufferError`, `span_copy`, the `PlainType` traits, and `protocol::load_unchecked` / `store_unchecked` that the EUI structs delegate to), [`status`](STATUS_MODULE.md) (`StatusValue<size_t>` is the return type of every checked load/store; `failure(BufferError::…)` reports out-of-space).
- **System / external:** `<array>`, `<bit>` (`std::bit_cast`, `std::byteswap`, `std::endian`), `<span>`, `<compare>`, `<concepts>`, `<expected>`, `<optional>`, `<string>`, `<string_view>`, `<system_error>` from the C++20/23 standard library. `<format>` only via the `*_format.hpp` variants.

## Notes & caveats

- `IeeeOrderedUInt` stores **in network order** — `network_value()` returns raw bytes as a `T` without swapping; `get()` / `operator T()` swap to host order. Don't compare `network_value()` to host-order constants.
- `Eui48::to_uint64()` right-justifies the address (upper two bytes are zero); `Eui64::to_uint64()` uses all eight bytes. Don't mix them.
- `Eui48::is_set()` rejects **both** all-zero and all-ones; treat `FF:FF:FF:FF:FF:FF` (broadcast) as unset — match it explicitly if you need broadcast detection.
- `VlanTag` reports `wire_size() == 0` when `tpid != 0x8100`. Calling `set_vid` / `set_pcp` / `set_dei` auto-promotes TPID to `0x8100` and the frame becomes 18 bytes.
- `can_load(buf, EthernetFrame*)` peeks at offset 12 to decide between 14 or 18 bytes — feed it the start of the frame.
- `from_string` for EUI types accepts `aa:bb:…`, `aa-bb-…`, and bare-hex `aabb…`; returns `std::nullopt` otherwise.
- `ethertype_name()` returns `"0x1234"` for unknown EtherTypes rather than throwing — check `protocols::ETHERTYPE_*` first if you need typed dispatch.

## Further reading

- [`buffer`](BUFFER_MODULE.md) — serializer/deserializer builders these types plug into via the `SerializableStruct` traits at the bottom of `ieee_ethernet.hpp`.
- [Deserializer Guide](DESERIALIZER_GUIDE.md) — how `field<T>()` / `skip<N>()` compose `Eui48` / `EthernetFrame` parses.
- [`status`](STATUS_MODULE.md) — the `StatusValue<size_t>` / `BufferError` types returned by checked load/store.
