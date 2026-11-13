// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// libFuzzer harness for the netlink framing readers: walk_messages,
/// walk_attributes, find_attribute, ack_status, dump_status and end_status read
/// a reply at the offsets the reply itself states, so arbitrary bytes must
/// never crash them or read out of bounds; any input may be refused. The first
/// input octet is the request's flags (shifted into the NLM_F_CAPPED /
/// NLM_F_ACK_TLVS range), the second its size, the rest the datagram.

#include "statusbar/net/net_netlink.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace {

std::uint64_t volatile g_sink = 0;

void consume(std::uint64_t const value) noexcept
{
    g_sink = g_sink ^ value;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    namespace netlink = statusbar::net::netlink;
    auto const input = std::span<std::uint8_t const>(data, size);
    if (input.size() < 2) {
        return 0;
    }
    auto const flags = static_cast<std::uint16_t>(input[0] << 4U);
    std::size_t const request_size = input[1];
    auto const datagram = input.subspan(2);
    std::string text;
    consume(netlink::ack_status(datagram, flags, request_size, text) ? 1U : 0U);
    consume(text.size());
    consume(netlink::dump_status(datagram) ? 1U : 0U);
    for (std::uint16_t const type : {netlink::TYPE_ERROR, netlink::TYPE_DONE, netlink::TYPE_MIN_FAMILY}) {
        if (auto const end = netlink::end_status(datagram, type, flags, request_size, text)) {
            consume(*end ? 2U : 3U);
        }
    }
    if (auto const found = netlink::find_attribute(datagram, input[1]); found && *found) {
        consume((*found)->size());
    }
    (void)netlink::walk_messages(datagram, [&](std::span<std::uint8_t const> const message) -> std::optional<statusbar::Status> {
        consume(message.size());
        (void)netlink::walk_attributes(
            message.subspan(netlink::MESSAGE_HEADER), [&](std::uint16_t type, std::span<std::uint8_t const> payload) {
                consume(static_cast<std::uint64_t>(type) + netlink::without_trailing_nuls(payload).size());
                return statusbar::success();
            });
        return std::nullopt;
    });
    return 0;
}
