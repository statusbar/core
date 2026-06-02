[← back to module index](README.md)

# ip

Wire-format structs and protocol constants for the IP family: IPv4,
IPv6, UDP, ARP, ICMP, ICMPv6 (with NDP/MLD), and IGMP.

## Overview

The `ip` module is a header-only collection of packed protocol headers
plus constants and helpers for reading, writing, and classifying them.
Field offsets are checked with `static_assert`, and every multi-byte
field is an `ieee::octet_t` / `ieee::doublet_t` / `ieee::quadlet_t`
storing its bytes in network order — so a default-constructed
`IPv4Header` already has the right wire layout and no `htons` /
`ntohs` calls appear in user code.

Each protocol ships as a pair: `ip_<proto>.hpp` for the data
structures and `ip_<proto>_format.hpp` for the `format_to(out, value)`
overloads. Include `ip.hpp` for structures alone, `ip_format.hpp` when
you also need pretty-printing — the split keeps the `<format>`
compile cost out of code that does not need it.

The wire structs satisfy the `buffer` module's serialization traits,
so `load_unchecked` / `store_unchecked` / `protocol::can_load` work
via ADL. Most headers expose an `init_…()` family that fills the
canonical field combination for a message, plus predicates like
`is_echo_reply()` / `is_multicast()` / `is_private()` / `is_valid()`.
IPv4 is the one protocol that needs an explicit checksum step —
`update_ipv4_checksum` / `verify_ipv4_checksum`.

## Key types

- `IPv4Address` — 4-byte network-order address; `is_loopback()` / `is_multicast()` / `is_broadcast()` / `is_private()` (RFC 1918).
- `IPv6Address` — 16-byte address; `is_loopback()` / `is_multicast()` / `is_link_local()` / `is_unique_local()` / `is_ipv4_mapped()`.
- `IPv4Header` — 20-byte IPv4 header with `version()` / `ihl()` / `dscp()` / `flags()` / `fragment_offset()` accessors, `init()`, `is_valid()`, `PROTOCOL_ICMP` / `PROTOCOL_IGMP` / `PROTOCOL_TCP` / `PROTOCOL_UDP`. Checksum via free `update_ipv4_checksum` / `verify_ipv4_checksum`.
- `IPv6Header` — 40-byte IPv6 header with `traffic_class()` / `dscp()` / `ecn()` / `flow_label()` and `NEXT_HEADER_*` constants.
- `UdpHeader` — 8-byte UDP header with `init()` and `payload_length()`. Well-known ports in `statusbar::ip::port` (`port::DNS`, `port::HTTPS`, `port::MDNS`, `port::AVTP`, `port::ATDECC`, …).
- `ArpHeader` — 28-byte Ethernet/IPv4 ARP with `init_request()` / `init_reply()`, `is_request()` / `is_reply()` / `is_ethernet_ipv4()`, and `arp_op_name()`.
- `IcmpHeader` / `Icmpv6Header` — 8- and 4-byte base headers with `is_echo_request()` / `is_echo_reply()`; v6 also has `is_error()` / `is_informational()` / `is_neighbor_solicitation()` / `is_router_advertisement()`. Bodies: `Icmpv6EchoHeader`, `Icmpv6NeighborSolicitation`, `Icmpv6NeighborAdvertisement`, `Icmpv6RouterSolicitation`. `icmp_type_name()` / `icmpv6_type_name()` print names.
- `IgmpHeader` — 8-byte IGMP header with `init_membership_query()` / `init_membership_report()` / `init_leave_group()` and matching predicates.

## Quick example

```cpp
#include "statusbar/ip/ip.hpp"
#include "statusbar/buffer/buffer.hpp"
#include <array>

using namespace statusbar;
using namespace statusbar::ip;

// Build a 28-byte IPv4 + UDP header pair, then parse it back.
void build_and_parse()
{
    std::array<uint8_t, 28> buf{};
    IPv4Header ip_hdr;
    ip_hdr.init(IPv4Header::PROTOCOL_UDP, UdpHeader::LENGTH + 100);
    ip_hdr.src_addr = IPv4Address(192, 168, 1, 1);
    ip_hdr.dst_addr = IPv4Address(192, 168, 1, 2);
    update_ipv4_checksum(ip_hdr);
    UdpHeader udp_hdr;
    udp_hdr.init(12345, port::HTTP, 100);
    (void)store_unchecked(buf, ip_hdr);
    (void)store_unchecked(make_span(buf).subspan(IPv4Header::LENGTH), udp_hdr);

    IPv4Header parsed;
    (void)load_unchecked(buf, &parsed);
    bool ok = parsed.is_valid() && verify_ipv4_checksum(parsed) && parsed.dst_addr.is_private();
}
```

## Headers

- `statusbar/ip/ip.hpp` — header for the data structures (no `<format>` cost).
- `statusbar/ip/ip_format.hpp` — adds every `format_to` overload.
- Per-protocol pairs `ip_<proto>.hpp` / `ip_<proto>_format.hpp` for `ipv4_address`, `ipv4`, `ipv6`, `udp`, `arp`, `icmp`, `icmpv6`, `igmp`.
- `statusbar/ip/ip_port_numbers.hpp` — `statusbar::ip::port::*` well-known port constants.

## Dependencies

- **Statusbar modules:** [`buffer`](BUFFER_MODULE.md) (`BufferError`, the `SerializableWireFixedStruct` / `SerializableFixedStruct` traits, and `protocol::load_unchecked` / `store_unchecked` / `can_load` every wire struct delegates to), [`ieee`](IEEE_MODULE.md) (`octet_t` / `doublet_t` / `quadlet_t` and `Eui48`), [`status`](STATUS_MODULE.md) (`StatusValue<size_t>` / `BufferError`).
- **System / external:** `<array>`, `<compare>`, `<cstdint>`, `<expected>`, `<format>` (only in `*_format.hpp`), `<span>`.

## Notes & caveats

- Do not read raw `uint16_t` / `uint32_t` members directly — every multi-byte field is an `octet_t` / `doublet_t` / `quadlet_t` stored in network order. Call `.get()` or the named accessor.
- `IPv4Header::init()` sets Don't Fragment and leaves `header_checksum` zero; call `update_ipv4_checksum(hdr)` after filling in addresses. `verify_ipv4_checksum` returns `false` on a zero field.
- `is_valid()` is per-field-consistency (IPv4: version/IHL/total_length; IPv6: just `version()==6`), not a checksum check.
- `UdpHeader::init()` leaves `checksum = 0`. Legal for IPv4 but **not** for IPv6 — fill it in before transmit.
- `IPv6Address` does **not** parse string form. Construct from 16 bytes or 8 host-order 16-bit words; `*_format.hpp` handles the print direction.
- `port::*` constants are transport-agnostic IANA numbers; the module ships UDP wire structs but not TCP.
- ICMPv6 / NDP covers the base headers and common Echo / Router / Neighbor bodies. NDP option-list parsing is left to the caller (`NDP_OPTION_*` constants only).
- Address classifiers implement the obvious RFC ranges; they do *not* consult routing tables or interfaces.

## Further reading

- [`buffer`](BUFFER_MODULE.md) — serialization traits these structs satisfy; [Deserializer Guide](DESERIALIZER_GUIDE.md) shows how `field<IPv4Header>()` composes.
- [`ieee`](IEEE_MODULE.md) — `octet_t` / `doublet_t` / `quadlet_t` and `Eui48`.
- [`status`](STATUS_MODULE.md) — `StatusValue` / `BufferError` returned by `can_load` / `can_store`.
