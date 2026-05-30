#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Reusable server configuration helper.
///
/// Extracts bind, port, max_clients, dscp from a config::Config.
/// Provides parse_or_exit() for one-liner setup in server mains.

#include "statusbar/config/config.hpp"
#include "statusbar/net/net_address.hpp"

#include <cstdint>
#include <optional>
#include <print>
#include <string>
#include <string_view>

namespace statusbar::net {

/// Check whether a DSCP value is valid (0..63) or unset (-1).
[[nodiscard]] constexpr auto is_valid_dscp(int dscp) noexcept -> bool
{
    return dscp == -1 || (dscp >= 0 && dscp <= 63);
}

/// Common server configuration extracted from config::Config.
struct ServerConfig
{
    std::string bind_host{"0.0.0.0"};
    uint16_t port{8080};
    size_t max_clients{8};
    int dscp{-1};  // -1 = not set

    /// Parse server config from a config::Config.
    /// Returns nullopt and prints errors if required fields are missing.
    [[nodiscard]] static auto from_config(statusbar::config::Config const& config) -> std::optional<ServerConfig>
    {
        ServerConfig sc;

        if (auto v = config.get_string("bind")) {
            sc.bind_host = std::string{*v};
        }
        if (auto v = config.get_integer("port")) {
            sc.port = static_cast<uint16_t>(*v);
        }
        if (auto v = config.get_integer("max_clients")) {
            sc.max_clients = static_cast<size_t>(*v);
        }
        sc.dscp = static_cast<int>(config.get_integer("dscp", -1));

        if (!is_valid_dscp(sc.dscp)) {
            std::println(stderr, "Error: DSCP must be between 0 and 63");
            return std::nullopt;
        }

        return sc;
    }

    /// Parse bind address into a SocketAddress.
    [[nodiscard]] auto socket_address() const -> std::expected<SocketAddress, std::error_code>
    {
        return SocketAddress::from_string(bind_host, std::to_string(port), SocketStream);
    }

    /// Parse CLI args, extract server config, exit on --help or errors.
    /// Returns (Config, ServerConfig) pair.
    [[nodiscard]] static auto parse_or_exit(int argc, char** argv, std::string_view usage_header = "")
        -> std::pair<statusbar::config::Config, ServerConfig>
    {
        statusbar::config::Config config;
        (void)config.apply_cli_overrides(argc, argv);

        if (config.get_boolean("help", false)) {
            if (!usage_header.empty()) {
                std::println(stderr, "{}", usage_header);
            }
            std::println(stderr, "Options:");
            std::println(stderr, "  --bind=ADDRESS      IP address to bind (default: 0.0.0.0)");
            std::println(stderr, "  --port=PORT         Port number (default: 8080)");
            std::println(stderr, "  --max-clients=N     Max concurrent clients (default: 8)");
            std::println(stderr, "  --dscp=VALUE        DSCP value 0-63 (default: none)");
            std::println(stderr, "  --config-load=FILE  Load TOML config file");
            std::println(stderr, "  --help              Show this help");
            std::exit(0);
        }

        auto sc = from_config(config);
        if (!sc) {
            std::exit(1);
        }

        return {std::move(config), std::move(*sc)};
    }
};

}  // namespace statusbar::net
