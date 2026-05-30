#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Well-known gPTP grandmaster ClockIdentity sentinels.
///
/// **Wire timescale.** IEEE 802.1AS-2020 / IEEE 1588-2019 mandate that the
/// gPTP timescale is **TAI** (the PTP timescale, with epoch at
/// 1970-01-01 00:00:00 TAI). A grandmaster disciplined by an external time
/// source MUST convert that source to TAI before stamping it onto the wire.
/// The conversions are constant offsets:
///
///   TAI = GPS + 19 s
///   TAI = UTC + currentUtcOffset (37 s as of the 2017-01-01 leap second)
///   TAI = GLONASS via UTC(SU) + currentUtcOffset
///   Galileo, BeiDou, QZSS each carry their own constant offset to TAI
///   per their system ICDs.
///
/// **Why these sentinels exist alongside `timeSource`.** IEEE 1588 already
/// carries a `timeSource` field in the Announce message (Table 7 — 0x20
/// for GPS, 0x10 for ATOMIC_CLOCK, 0x30 for TERRESTRIAL_RADIO, 0x90 for
/// OTHER, etc.) along with `clockClass`, `currentUtcOffset`,
/// `PTP_UTC_REASONABLE`, `TIME_TRACEABLE`, `FREQUENCY_TRACEABLE`. When you
/// have access to the Announce, prefer those fields — they are the
/// standards-track answer.
///
/// These ClockIdentity sentinels are for the case where **the only
/// information visible at the receiver is the grandmaster ClockIdentity
/// itself**. In particular, AVTP-over-UDP (IEEE 1722-2025 AAF version 1
/// stream header, Annex J encapsulation) carries `ptp_grandmaster_identity`
/// in every audio packet but not `timeSource`, `clockClass`, or
/// `currentUtcOffset`. A receiver examining those packets in isolation
/// can't otherwise tell a GPS-disciplined source from an
/// internal-oscillator one. The well-known ClockIdentity values let a
/// disciplined-source grandmaster declare its underlying reference in the
/// only field the receiver gets to see.
///
/// They also fill a gap in `timeSource` itself: 1588's enum has a single
/// `GPS` value (0x20) and lumps Galileo / GLONASS / BeiDou / QZSS into
/// `OTHER` (0x90). The sentinels here disambiguate per-constellation,
/// which matters for cross-domain correlation analysis (independence of
/// failure modes, ionospheric error correlation, etc.).
///
/// **Encoding.** Each identity is in the modified EUI-64 form (EUI-48
/// expanded with 0xFF-0xFE in the middle), allocated from the OUI24
/// prefix `70-B3-D5` (Jeff Koftinoff / Jeff Koftinoff Music Services).
/// The reserved block ends at `70-B3-D5-FF-FE-ED-CF-FF` for the most
/// common source (GPS), with adjacent values stepping back through the
/// other GNSS constellations and ending at non-GNSS national-lab sources.
///
/// **Stability guarantee.** These values are intended as proposals for an
/// eventual standards-track allocation. Until that lands, callers SHOULD
/// treat them as locally agreed sentinels and SHOULD compare equality
/// against the named constants here rather than baking the byte sequence
/// into application code.

#include "statusbar/tsn/tsn_clock_identity.hpp"

namespace statusbar::tsn {

/// TAI time, disciplined by GPS (NAVSTAR, US system).
/// TAI = GPS + 19 s. Corresponds to IEEE 1588 `timeSource` value 0x20 (GPS).
inline constexpr ClockIdentity GRANDMASTER_TAI_FROM_GPS{0x70, 0xB3, 0xD5, 0xFF, 0xFE, 0xED, 0xCF, 0xFF};

/// TAI time, disciplined by Galileo (EU system).
/// Galileo System Time has a constant offset to TAI documented in the
/// Galileo OS SIS ICD; the grandmaster handles the conversion.
/// IEEE 1588 `timeSource` carries this as 0x90 (OTHER).
inline constexpr ClockIdentity GRANDMASTER_TAI_FROM_GALILEO{0x70, 0xB3, 0xD5, 0xFF, 0xFE, 0xED, 0xCF, 0xFE};

/// TAI time, disciplined by GLONASS (Russian system).
/// GLONASS broadcasts UTC(SU); the grandmaster derives TAI from UTC via
/// currentUtcOffset. IEEE 1588 `timeSource` carries this as 0x90 (OTHER).
inline constexpr ClockIdentity GRANDMASTER_TAI_FROM_GLONASS{0x70, 0xB3, 0xD5, 0xFF, 0xFE, 0xED, 0xCF, 0xFD};

/// TAI time, disciplined by BeiDou (Chinese system).
/// BeiDou Time has a constant offset to TAI documented in the BeiDou ICD.
/// IEEE 1588 `timeSource` carries this as 0x90 (OTHER).
inline constexpr ClockIdentity GRANDMASTER_TAI_FROM_BEIDOU{0x70, 0xB3, 0xD5, 0xFF, 0xFE, 0xED, 0xCF, 0xFC};

/// TAI time, disciplined by QZSS (Japanese augmentation/standalone system).
/// QZSS Time is GPS-aligned by design.
/// IEEE 1588 `timeSource` carries this as 0x90 (OTHER).
inline constexpr ClockIdentity GRANDMASTER_TAI_FROM_QZSS{0x70, 0xB3, 0xD5, 0xFF, 0xFE, 0xED, 0xCF, 0xFB};

/// TAI time, disciplined by an unspecified or multi-constellation GNSS
/// receiver. Use only when the grandmaster genuinely cannot determine
/// which GNSS source produced the fix (multi-constellation receivers
/// that internally fuse multiple constellations into a single solution).
inline constexpr ClockIdentity GRANDMASTER_TAI_FROM_GNSS_UNSPECIFIED{0x70, 0xB3, 0xD5, 0xFF, 0xFE, 0xED, 0xCF, 0xFA};

/// TAI time, disciplined by a national time-and-frequency laboratory
/// (e.g., NIST/USNO, PTB, NPL) via a non-GNSS path such as a UTC(k)
/// reference clock or two-way satellite time transfer. IEEE 1588
/// `timeSource` carries this as 0x10 (ATOMIC_CLOCK) or 0x30
/// (TERRESTRIAL_RADIO) depending on the dissemination path.
inline constexpr ClockIdentity GRANDMASTER_TAI_FROM_NATIONAL_LAB{0x70, 0xB3, 0xD5, 0xFF, 0xFE, 0xED, 0xCF, 0xF9};

/// Returns true if `id` matches any of the well-known TAI-source sentinels.
[[nodiscard]] constexpr auto is_well_known_grandmaster(ClockIdentity const& id) noexcept -> bool
{
    return id == GRANDMASTER_TAI_FROM_GPS || id == GRANDMASTER_TAI_FROM_GALILEO || id == GRANDMASTER_TAI_FROM_GLONASS ||
        id == GRANDMASTER_TAI_FROM_BEIDOU || id == GRANDMASTER_TAI_FROM_QZSS || id == GRANDMASTER_TAI_FROM_GNSS_UNSPECIFIED ||
        id == GRANDMASTER_TAI_FROM_NATIONAL_LAB;
}

}  // namespace statusbar::tsn
