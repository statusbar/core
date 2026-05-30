// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// UDP Echo Server Example
/// Receives UDP datagrams and echoes back with character conversion:
/// - a-z converted to z-a (a→z, b→y, c→x, ...)
/// - A-Z converted to Z-A (A→Z, B→Y, C→X, ...)
/// - 0-9 converted to 9-0 (0→9, 1→8, 2→7, ...)
///
/// Usage: net_example_udp_echo --bind=<address> --port=<port> [--dscp=<value>]
/// Example IPv4: net_example_udp_echo --bind=0.0.0.0 --port=8080
/// Example IPv6: net_example_udp_echo --bind=:: --port=8080
/// With DSCP:    net_example_udp_echo --bind=0.0.0.0 --port=8080 --dscp=46

#include "statusbar/config/config.hpp"
#include "statusbar/itc/itc_stop_token.hpp"
#include "statusbar/net/net_address.hpp"
#include "statusbar/net/net_message_reactor.hpp"
#include "statusbar/net/net_server_config.hpp"
#include "statusbar/net/net_socket.hpp"
#include "statusbar/status/status.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <print>
#include <string>

#include <sys/socket.h>

using namespace statusbar::net;

namespace {

/// Convert a character using the mirror transformation
/// a-z → z-a, A-Z → Z-A, 0-9 → 9-0, others unchanged
[[nodiscard]] constexpr auto mirror_char(uint8_t ch) noexcept -> uint8_t
{
    if (ch >= 'a' && ch <= 'z') {
        return static_cast<uint8_t>('z' - (ch - 'a'));
    }
    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<uint8_t>('Z' - (ch - 'A'));
    }
    if (ch >= '0' && ch <= '9') {
        return static_cast<uint8_t>('9' - (ch - '0'));
    }
    return ch;
}

/// UDP echo handler implementing Pollable for MessageReactor
class EchoUdpPollable : public Pollable
{
  public:
    explicit EchoUdpPollable(SocketAddress const& bind_addr, int dscp = -1)
    {
        auto result = create_udp_socket(bind_addr, true, dscp);
        if (result) {
            fd_ = result->release();
            (void)set_nonblocking(fd_);
        }
    }

    ~EchoUdpPollable() override
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    // No copy
    EchoUdpPollable(EchoUdpPollable const&) = delete;
    auto operator=(EchoUdpPollable const&) -> EchoUdpPollable& = delete;

    [[nodiscard]] auto fd() const noexcept -> int override { return fd_; }

    void on_ready(int64_t /*now_ns*/) override
    {
        while (true) {
            SocketAddress source_addr;
            source_addr.reset_length();

            ssize_t const n =
                ::recvfrom(fd_, recv_buf_.data(), recv_buf_.size(), 0, source_addr.sockaddr(), source_addr.length_ptr());

            if (n <= 0) {
                break;
            }

            auto const len = static_cast<size_t>(n);
            std::println(stderr, "Received {} bytes from {}", len, source_addr.to_string());

            // Transform the packet data
            std::array<uint8_t, 1500> transformed{};
            for (size_t i = 0; i < len; ++i) {
                transformed[i] = mirror_char(recv_buf_[i]);
            }

            // Send back to the source address
            ssize_t const sent = ::sendto(fd_, transformed.data(), len, 0, source_addr.sockaddr(), source_addr.length());

            if (sent < 0) {
                std::println(stderr, "Warning: Failed to send response to {}", source_addr.to_string());
            }
        }
    }

    void tick(int64_t /*now_ns*/) override {}

    [[nodiscard]] auto finished() const noexcept -> bool override { return false; }

  private:
    int fd_{-1};
    std::array<uint8_t, 1500> recv_buf_{};
};

auto print_usage(char const* program_name) -> void
{
    std::println(stderr, "Usage: {} --bind=<address> --port=<port> [--dscp=<value>]", program_name);
    std::println(stderr, "");
    std::println(stderr, "UDP echo server that converts characters:");
    std::println(stderr, "  a-z → z-a (a→z, b→y, c→x, ...)");
    std::println(stderr, "  A-Z → Z-A (A→Z, B→Y, C→X, ...)");
    std::println(stderr, "  0-9 → 9-0 (0→9, 1→8, 2→7, ...)");
    std::println(stderr, "");
    std::println(stderr, "Options:");
    std::println(stderr, "  --bind=ADDRESS   IP address to bind to (e.g., 0.0.0.0, ::, 127.0.0.1)");
    std::println(stderr, "  --port=PORT      Port number to listen on");
    std::println(stderr, "  --dscp=VALUE     DSCP value (0-63) for QoS marking on outgoing packets");
    std::println(stderr, "                   Common values: 0=Best Effort, 46=EF (Expedited Forwarding),");
    std::println(stderr, "                   34=AF41, 26=AF31, 18=AF21, 10=AF11");
    std::println(stderr, "  --config=FILE    Load configuration from TOML file");
    std::println(stderr, "  --help           Show this help message");
    std::println(stderr, "");
    std::println(stderr, "Examples:");
    std::println(stderr, "  {} --bind=0.0.0.0 --port=8080        # Listen on all IPv4 interfaces", program_name);
    std::println(stderr, "  {} --bind=127.0.0.1 --port=8080      # Listen on localhost only", program_name);
    std::println(stderr, "  {} --bind=:: --port=8080             # Listen on all IPv6 interfaces", program_name);
    std::println(stderr, "  {} --bind=0.0.0.0 --port=8080 --dscp=46  # With DSCP EF", program_name);
    std::println(stderr, "");
    std::println(stderr, "Test with:");
    std::println(stderr, "  echo -n 'Hello World 12345' | nc -u <host> <port>");
}

}  // namespace

auto main(int argc, char** argv) -> int
{
    statusbar::config::Config config;
    auto remaining = config.apply_cli_overrides(argc, argv);

    // Check for --help
    if (config.get_boolean("help", false)) {
        print_usage(argv[0]);
        return 0;
    }

    // Get required values
    auto bind_opt = config.get_string("bind");
    auto port_opt = config.get_integer("port");

    if (!bind_opt || !port_opt) {
        std::println(stderr, "Error: --bind and --port are required\n");
        print_usage(argv[0]);
        return 1;
    }

    std::string bind_host{*bind_opt};
    std::string bind_port = std::to_string(*port_opt);
    int dscp = static_cast<int>(config.get_integer("dscp", -1));

    // Validate DSCP if specified
    if (!is_valid_dscp(dscp)) {
        std::println(stderr, "Error: DSCP must be between 0 and 63");
        return 1;
    }

    // Parse bind address (SocketDatagram for UDP)
    auto addr_result = SocketAddress::from_string(bind_host, bind_port, SocketDatagram);
    if (!addr_result) {
        std::println(stderr, "Error: Invalid address '{}:{}'", bind_host, bind_port);
        return 1;
    }

    // Create handler with optional DSCP
    auto handler_ptr = std::make_unique<EchoUdpPollable>(*addr_result, dscp);

    if (handler_ptr->fd() < 0) {
        std::println(stderr, "Error: Failed to create UDP socket on {}:{}", bind_host, bind_port);
        return 1;
    }

    std::println(stderr, "UDP echo server listening on {}", addr_result->to_string());
    if (dscp >= 0) {
        std::println(stderr, "DSCP: {} (TOS: {:#04X})", dscp, dscp << 2);
    }
    std::println(stderr, "Press Ctrl+C to stop");

    // Create reactor and run
    auto& stop = statusbar::itc::install_stop_signal();
    MessageReactor reactor{stop, monotonic_ns, 100};
    reactor.add(std::move(handler_ptr));

    reactor.run();

    std::println(stderr, "\nShutting down...");

    return 0;
}