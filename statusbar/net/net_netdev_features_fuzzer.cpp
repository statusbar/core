// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// libFuzzer harness for the netdev generic-netlink readers: genl_attributes,
/// parse_family_id and parse_dev read a reply at the offsets it states, so
/// arbitrary bytes must never crash them or read out of bounds; any input may
/// be refused. The first two input octets are the family id parse_dev expects,
/// the rest the message.

#include "statusbar/net/net_netdev_features.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace {

std::uint64_t volatile g_sink = 0;

void consume(std::uint64_t const value) noexcept
{
    g_sink = g_sink ^ value;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    using namespace statusbar::net::netdev;
    auto const input = std::span<std::uint8_t const>(data, size);
    if (input.size() < 2) {
        return 0;
    }
    auto const family_id = static_cast<std::uint16_t>(input[0] | (input[1] << 8U));
    auto const message = input.subspan(2);
    if (auto const id = parse_family_id(message)) {
        consume(*id);
    }
    if (auto const device = parse_dev(message, family_id)) {
        consume(device->xsk.flags ^ device->xdp_features ^ device->xdp_rx_metadata ^ static_cast<std::uint64_t>(device->ifindex));
    }
    if (auto const attributes = genl_attributes(message, family_id, input[0])) {
        consume(attributes->size());
    }
    return 0;
}
