// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/net/net_message_reactor.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <print>
#include <thread>

namespace statusbar::net {

void MessageReactor::run()
{
    while (poll_once(poll_timeout_ms_)) {
    }
}

auto MessageReactor::poll_once(int timeout_ms) -> bool
{
    if (!should_run()) {
        return false;
    }

    int64_t const now_ns = clock_();

    fill_pollfds();

    int n = 0;
    if (pollfds_.empty()) {
        if (timeout_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
        }
    } else {
        n = ::poll(pollfds_.data(), pollfds_.size(), timeout_ms);
        if (n < 0 && errno != EINTR) {
            std::println(stderr, "MessageReactor: poll() error: {}", std::strerror(errno));
        }
    }

    if (n > 0) {
        dispatch_ready(now_ns);
    }

    tick_all(now_ns);
    remove_finished();

    return should_run();
}

auto MessageReactor::active_count() const noexcept -> size_t
{
    size_t count = 0;
    for (auto const& p : ports_) {
        if (p && !p->finished()) {
            ++count;
        }
    }
    return count;
}

auto MessageReactor::should_run() const noexcept -> bool
{
    // After remove_finished() compacts the vector, !empty() ≡ active_count() > 0.
    // Avoids O(n) virtual finished() calls every cycle.
    return (stop_ == nullptr || !stop_->stop_requested()) && !ports_.empty();
}

void MessageReactor::fill_pollfds()
{
    pollfds_.clear();
    pollfd_port_idx_.clear();
    for (size_t i = 0; i < ports_.size(); ++i) {
        if (!ports_[i] || ports_[i]->finished()) {
            continue;
        }
        int const fd = ports_[i]->fd();
        if (fd < 0) {
            continue;
        }
        pollfds_.push_back(pollfd{.fd = fd, .events = ports_[i]->poll_events(), .revents = 0});
        pollfd_port_idx_.push_back(i);
    }
}

void MessageReactor::dispatch_ready(int64_t now_ns)
{
    for (size_t pfd_idx = 0; pfd_idx < pollfds_.size(); ++pfd_idx) {
        auto const revents = pollfds_[pfd_idx].revents;
        auto& port = ports_[pollfd_port_idx_[pfd_idx]];
        if ((revents & POLLIN) != 0) {
            port->on_ready(now_ns);
        }
        if ((revents & POLLOUT) != 0) {
            port->on_writable(now_ns);
        }
    }
}

void MessageReactor::tick_all(int64_t now_ns)
{
    for (auto& p : ports_) {
        if (p && !p->finished()) {
            p->tick(now_ns);
        }
    }
}

void MessageReactor::remove_finished()
{
    std::erase_if(ports_, [](auto const& p) { return !p || p->finished(); });
}

}  // namespace statusbar::net
