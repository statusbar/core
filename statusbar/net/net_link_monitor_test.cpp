// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// Tests for net::LinkMonitor — the throttle + transition logic driven by an
// injected link-state reader and a manual clock.

#include "statusbar/net/net_link_monitor.hpp"

#include "statusbar/test/test.hpp"

#include <optional>
#include <vector>

using statusbar::net::LinkMonitor;

namespace {

/// Records (up, now_ns) for each emitted change.
struct Recorder
{
    std::vector<std::pair<bool, int64_t>> changes;
    auto on_change()
    {
        return [this](bool up, int64_t now_ns) { changes.emplace_back(up, now_ns); };
    }
};

constexpr int64_t INTERVAL = 500'000'000;  // 500 ms

}  // namespace

TEST(link_monitor, emits_initial_state_on_first_poll)
{
    Recorder rec;
    std::optional<bool> state = true;
    LinkMonitor mon{[&] { return state; }, rec.on_change(), INTERVAL};

    mon.tick(0);  // first poll happens at now >= 0
    EXPECT_EQ(rec.changes.size(), size_t{1});
    EXPECT_TRUE(rec.changes[0].first);
    EXPECT_EQ(mon.link_up(), std::optional<bool>{true});
}

TEST(link_monitor, throttles_to_poll_interval)
{
    Recorder rec;
    int reads = 0;
    std::optional<bool> state = true;
    LinkMonitor mon{
        [&] {
            ++reads;
            return state;
        },
        rec.on_change(),
        INTERVAL};

    mon.tick(0);             // poll #1 (read)
    mon.tick(1);             // within interval -> no read
    mon.tick(INTERVAL - 1);  // still within interval -> no read
    EXPECT_EQ(reads, 1);
    mon.tick(INTERVAL);  // interval elapsed -> poll #2
    EXPECT_EQ(reads, 2);
}

TEST(link_monitor, emits_only_on_transition)
{
    Recorder rec;
    std::optional<bool> state = true;
    LinkMonitor mon{[&] { return state; }, rec.on_change(), INTERVAL};

    mon.tick(0);         // up (initial) -> emit
    mon.tick(INTERVAL);  // still up -> no emit
    state = false;
    mon.tick(2 * INTERVAL);  // down -> emit
    mon.tick(3 * INTERVAL);  // still down -> no emit
    state = true;
    mon.tick(4 * INTERVAL);  // up -> emit

    EXPECT_EQ(rec.changes.size(), size_t{3});
    EXPECT_TRUE(rec.changes[0].first);               // up
    EXPECT_FALSE(rec.changes[1].first);              // down
    EXPECT_TRUE(rec.changes[2].first);               // up
    EXPECT_EQ(rec.changes[1].second, 2 * INTERVAL);  // carries the reactor clock
}

TEST(link_monitor, unknown_state_emits_nothing_and_keeps_last)
{
    Recorder rec;
    std::optional<bool> state = std::nullopt;  // reader can't determine link state
    LinkMonitor mon{[&] { return state; }, rec.on_change(), INTERVAL};

    mon.tick(0);
    mon.tick(INTERVAL);
    EXPECT_TRUE(rec.changes.empty());
    EXPECT_FALSE(mon.link_up().has_value());

    // Once readable, the first real state is emitted.
    state = true;
    mon.tick(2 * INTERVAL);
    EXPECT_EQ(rec.changes.size(), size_t{1});
    EXPECT_TRUE(rec.changes[0].first);

    // A transient unknown read does NOT spuriously emit a down.
    state = std::nullopt;
    mon.tick(3 * INTERVAL);
    EXPECT_EQ(rec.changes.size(), size_t{1});
    EXPECT_EQ(mon.link_up(), std::optional<bool>{true});  // last-known retained
}

TEST(link_monitor, is_tick_only_pollable)
{
    LinkMonitor mon{[] { return std::optional<bool>{true}; }, [](bool, int64_t) {}, INTERVAL};
    EXPECT_EQ(mon.fd(), -1);
    EXPECT_FALSE(mon.finished());
}

//
// Test Runner
//

TEST_MAIN(statusbar_net, net_link_monitor_test)
