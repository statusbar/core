[← back to module index](README.md)

# tsn

Time-Sensitive Networking wire-format identifiers — `ClockIdentity`
(IEEE 802.1AS gPTP) and `StreamId` (IEEE 802.1Q) — plus the TsnError
code set and a small table of well-known GNSS grandmaster sentinels.

## Overview

The `tsn` module supplies the two 8-byte identifiers that pin a TSN
endpoint to a clock domain and a stream. `ClockIdentity` is the gPTP
clock id from IEEE 802.1AS-2020 (also the grandmaster identity in
ATDECC ADP/ACMP and the AVTP stream header); `StreamId` is the
802.1Q-2014 §35.2.2.8.2 pair of EUI-48 system address and `uint16_t`
unique id that names an MSRP talker/listener stream.

Both types store in network byte order so they `memcpy` straight into
a packet buffer, and plug into the serialization machinery from
`buffer`: every type ships `wire_size`, `can_load` / `can_store`,
`load_unchecked` / `store_unchecked`, and checked `load` / `store`
returning `StatusValue<std::size_t>`. `ClockIdentity` derives from
`ieee::IeeeOrderedUInt<uint64_t>`; `StreamId` composes an
`ieee::Eui48` with an `ieee::doublet_t`. Conversions to/from
`ieee::Eui48` (modified-EUI-64) and `ieee::Eui64` are built in.

`tsn_well_known_grandmasters.hpp` declares ClockIdentity sentinels
(`70-B3-D5` OUI24) that a GNSS-disciplined grandmaster can advertise
when the only field on the wire is `ptp_grandmaster_identity` — useful
for AVTP-over-UDP where `timeSource` / `clockClass` aren't
transmitted. `TsnError` covers MRP/MSRP/MVRP capacity errors; the
`MsrpConfig` / `MvrpConfig` types it refers to live in higher-level
modules.

## Key types

- `ClockIdentity` — IEEE 802.1AS 8-byte clock id, derived from `ieee::IeeeOrderedUInt<uint64_t>`; construct from `uint64_t`, eight bytes, `ieee::Eui64`, or via `from_eui48()` (modified-EUI-64); exposes `is_set()`, `to_uint64()`, `to_eui64()`.
- `StreamId` — 802.1Q stream identifier; `ieee::Eui48` system address paired with `uint16_t` unique id. `is_set()`, `to_uint64()` / `from_uint64()`, `increment_unique_id()`, defaulted `<=>`.
- `TsnError` — enum of MRP/MSRP/MVRP failure modes; integrates with `std::error_code` via `tsn_error_category()` and `make_error_code(TsnError)`.
- `kGrandmasterTaiFromGps`, `kGrandmasterTaiFromGalileo`, `kGrandmasterTaiFromGlonass`, `kGrandmasterTaiFromBeiDou`, `kGrandmasterTaiFromQzss`, `kGrandmasterTaiFromGnssUnspecified`, `kGrandmasterTaiFromNationalLab` — `ClockIdentity` sentinels; `is_well_known_grandmaster()` predicate.
- `to_string` / `from_string` (tag-pointer overloads) plus tag-free `clock_identity_from_string` / `stream_id_from_string` returning `std::optional`.
- `format_to(out, ClockIdentity)` / `format_to(out, StreamId)` — `<format>`-based output (`aa:bb:cc:dd:ee:ff:gg:hh` and `aa:bb:cc:dd:ee:ff:0001`).

## Quick example

```cpp
#include "statusbar/tsn/tsn.hpp"
#include "statusbar/buffer/buffer.hpp"

using namespace statusbar;
using namespace statusbar::tsn;
using namespace statusbar::ieee;

int main()
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId original{mac, 0x1234};

    std::array<std::uint8_t, 8> buffer{};
    if (!store(std::span<std::uint8_t>{buffer}, original)) { return 1; }

    StreamId restored{};
    if (!load(std::span<std::uint8_t const>{buffer}, &restored)) { return 1; }

    // Derive a ClockIdentity from the same MAC (modified EUI-64).
    ClockIdentity clk = ClockIdentity::from_eui48(mac);
    return (restored == original && !is_well_known_grandmaster(clk)) ? 0 : 1;
}
```

## Headers

- `statusbar/tsn/tsn.hpp` — data-type header; pulls in clock-identity, stream-id, error, and well-known-grandmasters.
- `statusbar/tsn/tsn_clock_identity.hpp` — `ClockIdentity` plus its `wire_size` / `can_load` / `can_store` / `load_unchecked` / `store_unchecked` / `load` / `store` and the string conversions.
- `statusbar/tsn/tsn_stream_id.hpp` — `StreamId` with the same serialization surface, string conversions, and `config_parse` / `config_format` ADL hooks.
- `statusbar/tsn/tsn_error.hpp` — `TsnError`, `TsnErrorCategory`, `tsn_error_category()`, `make_error_code(TsnError)`, `tsn_error_name`.
- `statusbar/tsn/tsn_well_known_grandmasters.hpp` — the seven `kGrandmasterTaiFrom*` constants and `is_well_known_grandmaster()`.
- `statusbar/tsn/tsn_clock_identity_format.hpp` — `format_to(out, ClockIdentity)`; include only when you need `<format>`.
- `statusbar/tsn/tsn_stream_id_format.hpp` — `format_to(out, StreamId)`; include only when you need `<format>`.
- `statusbar/tsn/tsn_format.hpp` — header that combines `tsn.hpp` with both `_format` headers.

## Dependencies

- **Statusbar modules:** [`ieee`](IEEE_MODULE.md) (`IeeeOrderedUInt<uint64_t>` is the `ClockIdentity` base; `Eui48` / `Eui64` / `doublet_t` and `ieee::load_unchecked` / `store_unchecked` back `StreamId`), [`buffer`](BUFFER_MODULE.md) (`BufferError`, `span_copy`, and `protocol::load_unchecked` / `store_unchecked` for the byte path), [`status`](STATUS_MODULE.md) (`StatusValue<size_t>` returned from every checked load/store; `TsnError` plugs into `std::error_code`).
- **System / external:** `<array>`, `<compare>`, `<cstdint>`, `<expected>`, `<optional>`, `<span>`, `<string>`, `<string_view>`, `<system_error>` from the C++20/23 standard library. `<format>` only via the `*_format.hpp` variants.

## Notes & caveats

- `ClockIdentity::is_set()` rejects **both** all-zeros and all-ones — IEEE reserves both. `StreamId::is_set()` defers to `Eui48::is_set()` on the system-address half only; `unique_id` bits are ignored.
- `ClockIdentity::from_eui48()` inserts `0xFF, 0xFE` between the OUI and NIC halves (modified-EUI-64). The `ClockIdentity(ieee::Eui64 const&)` constructor does **not** — it copies the EUI-64 verbatim. Pick the constructor that matches the wire.
- `StreamId::to_uint64()` puts the system address in the upper 48 bits and the unique id in the lower 16. Don't compare against `Eui48::to_uint64()` directly — they right-justify differently.
- `from_string` accepts colon- or dash-separated hex; `stream_id_from_string` also accepts `…:ff.0001` and `…:ff/1`. Returns `std::nullopt` on any other shape.
- The well-known grandmaster sentinels are **not standards-track** yet — compare against the named constants rather than baking the byte sequence in.
- `StreamId` opts into `ConfigParseable`, so [`args`](ARGS_MODULE.md) accepts `specs.add<StreamId>(...)` directly. `ClockIdentity` does not — wrap it if you need CLI binding.

## Further reading

- [`ieee`](IEEE_MODULE.md) — the `Eui48` / `Eui64` / `IeeeOrderedUInt` foundation.
- [`buffer`](BUFFER_MODULE.md) — serializer/deserializer builders these types append to via `is_serializable_wire_fixed_struct`.
- [`status`](STATUS_MODULE.md) — `StatusValue<size_t>` and the `std::error_code` machinery `TsnError` plugs into.
