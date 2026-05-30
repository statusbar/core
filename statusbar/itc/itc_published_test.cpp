// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/itc/itc_published.hpp"

#include "statusbar/test/test.hpp"

#include <atomic>
#include <cstdint>
#include <thread>

using statusbar::itc::Published;

TEST(sync_published_init, fresh_value_is_default)
{
    Published<int64_t> p;
    EXPECT_EQ(p.load(), 0);
}

TEST(sync_published_init, initial_value_ctor)
{
    Published<int64_t> p{1234};
    EXPECT_EQ(p.load(), 1234);
}

TEST(sync_published_roundtrip, publish_then_load)
{
    Published<int64_t> p;
    p.publish(7777);
    EXPECT_EQ(p.load(), 7777);
    p.publish(-42);
    EXPECT_EQ(p.load(), -42);
}

TEST(sync_published_roundtrip, double_value)
{
    Published<double> p;
    p.publish(3.5);
    EXPECT_EQ(p.load(), 3.5);
}

TEST(sync_published_threaded, reader_only_sees_published_values)
{
    Published<int64_t> p{0};
    std::atomic<bool> stop{false};
    std::thread writer{[&]() noexcept {
        for (int64_t i = 1; i <= 200000; ++i) {
            p.publish(i);
        }
        stop.store(true, std::memory_order_release);
    }};
    bool ok = true;
    while (!stop.load(std::memory_order_acquire)) {
        int64_t const v = p.load();
        if (v < 0 || v > 200000) {
            ok = false;
        }
    }
    writer.join();
    EXPECT_TRUE(ok);
    EXPECT_EQ(p.load(), 200000);
}

TEST_MAIN(statusbar_itc, itc_published_test)
