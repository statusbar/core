// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// TCP Echo Server Example
/// Accepts TCP connections and echoes back text with character mirroring:
/// - a-z converted to z-a (a->z, b->y, c->x, ...)
/// - A-Z converted to Z-A (A->Z, B->Y, C->X, ...)
/// - 0-9 converted to 9-0 (0->9, 1->8, 2->7, ...)
///
/// Uses a single TcpEchoPollable with MessageReactor:
/// The listener fd is polled by the reactor; client I/O is handled in tick().
///
/// Usage: statusbar-net-tcp-echo --bind=0.0.0.0 --port=8080
/// Config: statusbar-net-tcp-echo --config-load=echo.toml

#include "statusbar/itc/itc_stop_token.hpp"
#include "statusbar/net/net_address.hpp"
#include "statusbar/net/net_message_reactor.hpp"
#include "statusbar/net/net_server_config.hpp"
#include "statusbar/net/net_socket.hpp"

#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <print>
#include <vector>

#include <sys/socket.h>

using namespace statusbar::net;

namespace {

/// Convert a character using the mirror transformation
/// a-z -> z-a, A-Z -> Z-A, 0-9 -> 9-0, others unchanged
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

/// A connected TCP client
struct Client
{
    int fd{-1};
    SocketAddress peer{};

    void close_fd() noexcept
    {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }
};

/// TCP echo server implementing Pollable for MessageReactor.
///
/// The listener fd is polled by the reactor (on_ready accepts new clients).
/// Client I/O is handled in tick() using non-blocking reads/writes.
class TcpEchoPollable : public Pollable
{
  public:
    TcpEchoPollable(SocketAddress const& bind_addr, size_t max_clients, int dscp = -1)
        : max_clients_{max_clients}
        , dscp_{dscp}
    {
        auto result = create_tcp_listener(bind_addr);
        if (result) {
            listener_fd_ = result->release();
            (void)set_nonblocking(listener_fd_);
        }
    }

    ~TcpEchoPollable() override
    {
        for (auto& c : clients_) {
            c.close_fd();
        }
        if (listener_fd_ >= 0) {
            ::close(listener_fd_);
        }
    }

    // No copy
    TcpEchoPollable(TcpEchoPollable const&) = delete;
    auto operator=(TcpEchoPollable const&) -> TcpEchoPollable& = delete;

    [[nodiscard]] auto fd() const noexcept -> int override { return listener_fd_; }

    void on_ready(int64_t /*now_ns*/) override
    {
        // Accept new connections
        while (true) {
            SocketAddress peer;
            peer.reset_length();

            int const client_fd = ::accept(listener_fd_, peer.sockaddr(), peer.length_ptr());
            if (client_fd < 0) {
                break;  // No more pending connections (or error)
            }

            if (clients_.size() >= max_clients_) {
                // At capacity — reject
                ::close(client_fd);
                std::println(stderr, "Rejected client {} (at capacity {})", peer.to_string(), max_clients_);
                continue;
            }

            (void)set_nonblocking(client_fd);

            if (dscp_ >= 0) {
                (void)set_dscp(client_fd, peer.family(), static_cast<uint8_t>(dscp_));
            }

            std::println(stderr, "Client {} connected (fd={})", peer.to_string(), client_fd);
            clients_.push_back(Client{.fd = client_fd, .peer = peer});
        }
    }

    void tick(int64_t /*now_ns*/) override
    {
        // Process I/O for each connected client
        // Iterate in reverse so we can remove disconnected clients in-place
        for (size_t i = clients_.size(); i > 0; --i) {
            auto& client = clients_[i - 1];

            // Non-blocking read
            std::array<uint8_t, 1024> buf{};
            ssize_t const n = ::recv(client.fd, buf.data(), buf.size(), 0);

            if (n > 0) {
                // Transform and echo
                auto const len = static_cast<size_t>(n);
                for (size_t j = 0; j < len; ++j) {
                    buf[j] = mirror_char(buf[j]);
                }
                // Best-effort send
                (void)::send(client.fd, buf.data(), len, 0);
            } else if (n == 0) {
                // EOF — client disconnected
                std::println(stderr, "Client {} disconnected (EOF)", client.peer.to_string());
                client.close_fd();
                clients_.erase(clients_.begin() + static_cast<ptrdiff_t>(i - 1));
            } else {
                // n < 0
                if (!would_block() && !was_interrupted()) {
                    // Real error — disconnect
                    std::println(stderr, "Client {} error, disconnecting", client.peer.to_string());
                    client.close_fd();
                    clients_.erase(clients_.begin() + static_cast<ptrdiff_t>(i - 1));
                }
                // EAGAIN/EINTR — no data yet, continue
            }
        }
    }

    [[nodiscard]] auto finished() const noexcept -> bool override { return false; }

    /// Get the listener's local address (resolves port 0).
    [[nodiscard]] auto local_addr() const -> statusbar::StatusValue<SocketAddress>
    {
        if (listener_fd_ < 0) {
            return statusbar::failure(NetError::not_connected);
        }
        SocketAddress addr;
        addr.reset_length();
        if (::getsockname(listener_fd_, addr.sockaddr(), addr.length_ptr()) < 0) {
            return statusbar::failure(NetError::bind_failed);
        }
        return addr;
    }

  private:
    int listener_fd_{-1};
    size_t max_clients_;
    int dscp_;
    std::vector<Client> clients_;
};

}  // namespace

auto main(int argc, char** argv) -> int
{
    auto& stop = statusbar::itc::install_stop_signal();
    auto [config, sc] = ServerConfig::parse_or_exit(argc, argv, "TCP echo server with character mirroring (a<->z, A<->Z, 0<->9)");

    auto addr = sc.socket_address();
    if (!addr) {
        std::println(stderr, "Error: Invalid address '{}:{}'", sc.bind_host, sc.port);
        return 1;
    }

    auto handler = std::make_unique<TcpEchoPollable>(*addr, sc.max_clients, sc.dscp);

    if (handler->fd() < 0) {
        std::println(stderr, "Error: Failed to create TCP listener on {}:{}", sc.bind_host, sc.port);
        return 1;
    }

    auto local = handler->local_addr();
    if (local) {
        std::println(stderr, "Echo server listening on {}", local->to_string());
    }
    if (sc.dscp >= 0) {
        std::println(stderr, "DSCP: {} (TOS: {:#04X})", sc.dscp, sc.dscp << 2);
    }
    std::println(stderr, "Max clients: {}", sc.max_clients);
    std::println(stderr, "Press Ctrl+C to stop");

    MessageReactor reactor{stop, monotonic_ns, 100};
    reactor.add(std::move(handler));

    reactor.run();

    std::println(stderr, "\nShutting down...");

    return 0;
}
