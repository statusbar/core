// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/net/net_util.hpp"

#include "statusbar/ieee/ieee_ethernet.hpp"

#include <netdb.h>

#include <cstdlib>
#include <limits>

namespace statusbar::net {

[[nodiscard]] auto split_host_port(std::string_view s, std::string& host_out, std::string& port_out) -> bool
{
    if (s.empty()) {
        return false;
    }

    if (s.front() == '[') {
        auto const close_bracket = s.find(']');
        if (close_bracket == std::string_view::npos || close_bracket + 2 >= s.size() || s[close_bracket + 1] != ':') {
            return false;
        }
        host_out = std::string{s.substr(1, close_bracket - 1)};
        port_out = std::string{s.substr(close_bracket + 2)};
        return !host_out.empty() && !port_out.empty();
    }

    auto const pos = s.rfind(':');
    if (pos == std::string_view::npos) {
        return false;
    }
    host_out = std::string{s.substr(0, pos)};
    port_out = std::string{s.substr(pos + 1)};
    return !host_out.empty() && !port_out.empty();
}

[[nodiscard]] auto address_to_host_port(SocketAddress const& addr, std::string& host_out, uint16_t& port_out) -> bool
{
    char host_buf[NI_MAXHOST] = {};
    char port_buf[NI_MAXSERV] = {};
    int const err = ::getnameinfo(
        addr.sockaddr(), addr.length(), host_buf, sizeof(host_buf), port_buf, sizeof(port_buf), NI_NUMERICHOST | NI_NUMERICSERV);
    if (err != 0) {
        return false;
    }
    host_out = host_buf;
    unsigned long const port = std::strtoul(port_buf, nullptr, 10);
    if (port > std::numeric_limits<uint16_t>::max()) {
        return false;
    }
    port_out = static_cast<uint16_t>(port);
    return true;
}

[[nodiscard]] auto parse_hex_into(std::string_view s, std::span<uint8_t> out) -> bool
{
    if (s.size() != out.size() * 2) {
        return false;
    }
    for (size_t i = 0; i < out.size(); ++i) {
        // ieee::parse_hex_digit returns 255 on invalid input; we sentinel
        // on that to keep the same "false on malformed input" contract.
        uint8_t const hi = ieee::parse_hex_digit(s[i * 2]);
        uint8_t const lo = ieee::parse_hex_digit(s[(i * 2) + 1]);
        if (hi == 255 || lo == 255) {
            return false;
        }
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

}  // namespace statusbar::net
