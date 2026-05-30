#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/net/net_address.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace statusbar::net {

[[nodiscard]] auto split_host_port(std::string_view s, std::string& host_out, std::string& port_out) -> bool;

[[nodiscard]] auto address_to_host_port(SocketAddress const& addr, std::string& host_out, uint16_t& port_out) -> bool;

[[nodiscard]] auto parse_hex_into(std::string_view s, std::span<uint8_t> out) -> bool;

}  // namespace statusbar::net
