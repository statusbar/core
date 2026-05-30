// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/itc/itc_stop_token.hpp"

#include "statusbar/test/test.hpp"

#include <atomic>
#include <chrono>
#include <thread>

using statusbar::itc::StopToken;
using namespace std::chrono_literals;

TEST(sync_stop_token_init, fresh_token_is_not_requested)
{
    StopToken t;
    EXPECT_FALSE(t.stop_requested());
    EXPECT_FALSE(t.wait_for_stop(0ms));
}

TEST(sync_stop_token_request, request_stop_sets_flag)
{
    StopToken t;
    t.request_stop();
    EXPECT_TRUE(t.stop_requested());
}

TEST(sync_stop_token_request, signal_set_sets_flag)
{
    StopToken t;
    t.signal_set();
    EXPECT_TRUE(t.stop_requested());
}

TEST(sync_stop_token_wait, wait_returns_true_when_already_set)
{
    StopToken t;
    t.request_stop();
    EXPECT_TRUE(t.wait_for_stop(0ms));
}

TEST(sync_stop_token_wait, wait_times_out_when_not_set)
{
    StopToken t;
    auto const start = std::chrono::steady_clock::now();
    bool const got = t.wait_for_stop(10ms);
    auto const elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_FALSE(got);
    EXPECT_TRUE(elapsed >= 10ms);
}

TEST(sync_stop_token_wakeup, request_stop_wakes_waiter)
{
    StopToken t;
    std::atomic<bool> waiter_returned{false};
    std::thread waiter{[&]() noexcept {
        bool const got = t.wait_for_stop(10s);
        waiter_returned.store(got, std::memory_order_relaxed);
    }};
    std::this_thread::sleep_for(10ms);
    auto const before = std::chrono::steady_clock::now();
    t.request_stop();
    waiter.join();
    auto const elapsed = std::chrono::steady_clock::now() - before;
    EXPECT_TRUE(waiter_returned.load(std::memory_order_relaxed));
    EXPECT_TRUE(elapsed < 500ms);
}

TEST(sync_stop_token_wakeup, signal_set_does_not_wake_waiter_until_timeout)
{
    StopToken t;
    auto const start = std::chrono::steady_clock::now();
    std::thread setter{[&]() noexcept {
        std::this_thread::sleep_for(10ms);
        t.signal_set();
    }};
    bool const got = t.wait_for_stop(50ms);
    auto const elapsed = std::chrono::steady_clock::now() - start;
    setter.join();
    EXPECT_TRUE(got);
    // Without the cv notify, the waiter sleeps the full 50ms timeout
    // even though the flag was set at ~10ms. Allow some scheduling
    // slack on either side.
    EXPECT_TRUE(elapsed >= 40ms);
}

TEST(sync_stop_token_wakeup, multiple_waiters_all_wake_on_request_stop)
{
    StopToken t;
    std::atomic<int> woken{0};
    auto run_waiter = [&]() noexcept {
        if (t.wait_for_stop(10s)) {
            woken.fetch_add(1, std::memory_order_relaxed);
        }
    };
    std::thread a{run_waiter};
    std::thread b{run_waiter};
    std::thread c{run_waiter};
    std::this_thread::sleep_for(10ms);
    t.request_stop();
    a.join();
    b.join();
    c.join();
    EXPECT_EQ(woken.load(std::memory_order_relaxed), 3);
}

TEST_MAIN(statusbar_itc, itc_stop_token_test)
