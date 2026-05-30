// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT
#include "statusbar/pcap/pcap_replay.hpp"

#include "statusbar/pcap/pcap_error.hpp"

#include <chrono>
#include <utility>

namespace statusbar::pcap {

auto PcapReplayDriver::open(std::string const& input_path, std::pmr::memory_resource* memory_resource)
    -> StatusValue<PcapReplayDriver>
{
    auto reader_result = PcapngReader::open(input_path, memory_resource);
    if (!reader_result) {
        return forward_failure(reader_result);
    }
    return success(PcapReplayDriver{std::move(*reader_result)});
}

auto PcapReplayDriver::next(ReplayEvent& out) -> StatusValue<bool>
{
    uint64_t ts_us = 0;
    out.frame = ieee::EthernetFrame{};
    uint16_t outer_ethertype = 0;
    auto step = reader_.read_packet(&ts_us, out.frame.dest_mac, out.frame.src_mac, &outer_ethertype, payload_storage_);
    if (!step) {
        return forward_failure(step);
    }
    if (!*step) {
        return success(false);
    }
    ++stats_.packets_total;
    out.timestamp = ReplayTime{} + std::chrono::microseconds{ts_us};
    out.payload = std::span<uint8_t const>(payload_storage_);
    current_time_ = out.timestamp;

    // If the outer ethertype is 802.1Q, lift the VLAN tag into
    // `frame.vlan_tag` and advance the inner ethertype / payload. The
    // pcap reader hands us the outer ethertype (0x8100) and the 4-byte
    // VLAN field plus inner ethertype sit at the start of `payload`:
    // [TCI_hi, TCI_lo, innerT_hi, innerT_lo, ...].
    if (outer_ethertype == ieee::VlanTag::ETHERTYPE && out.payload.size() >= 4) {
        uint16_t const raw_tci = static_cast<uint16_t>((static_cast<uint16_t>(out.payload[0]) << 8) | out.payload[1]);
        out.frame.vlan_tag = ieee::VlanTag::from_raw(ieee::VlanTag::ETHERTYPE, raw_tci);
        out.frame.ethertype = static_cast<uint16_t>((static_cast<uint16_t>(out.payload[2]) << 8) | out.payload[3]);
        out.payload = out.payload.subspan(4);
    } else {
        out.frame.ethertype = outer_ethertype;
    }
    return success(true);
}

}  // namespace statusbar::pcap
