// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/itc/itc_rt_callback_slot.hpp"

#include "statusbar/test/test.hpp"

#include <atomic>
#include <cstdint>
#include <thread>

using statusbar::itc::RtCallbackSlot;

namespace {

using TestFn = int (*)(void*) noexcept;

auto fn_a(void* /*ctx*/) noexcept -> int
{
    return 1;
}
auto fn_b(void* /*ctx*/) noexcept -> int
{
    return 2;
}

int g_ctx_a = 0;
int g_ctx_b = 0;

}  // namespace

TEST(sync_rt_callback_slot_init, null_before_first_publish)
{
    RtCallbackSlot<TestFn> slot;
    auto const cb = slot.load();
    EXPECT_TRUE(cb.fn == nullptr);
    EXPECT_TRUE(cb.ctx == nullptr);
}

TEST(sync_rt_callback_slot_publish, publish_then_load_returns_pair)
{
    RtCallbackSlot<TestFn> slot;
    slot.publish(&fn_a, &g_ctx_a);
    auto const cb = slot.load();
    EXPECT_TRUE(cb.fn == &fn_a);
    EXPECT_TRUE(cb.ctx == &g_ctx_a);
}

TEST(sync_rt_callback_slot_publish, clear_returns_null_pair)
{
    RtCallbackSlot<TestFn> slot;
    slot.publish(&fn_a, &g_ctx_a);
    slot.clear();
    auto const cb = slot.load();
    EXPECT_TRUE(cb.fn == nullptr);
    EXPECT_TRUE(cb.ctx == nullptr);
}

TEST(sync_rt_callback_slot_threaded, pair_is_always_coherent)
{
    // A publisher alternates between two (fn, ctx) pairs; a consumer
    // loads in a tight loop. Every observed pair must be a matched
    // set from a single publish() — never fn from one with ctx from
    // another.
    RtCallbackSlot<TestFn> slot;
    slot.publish(&fn_a, &g_ctx_a);
    std::atomic<bool> stop{false};
    std::thread publisher{[&]() noexcept {
        for (int i = 0; i < 200000; ++i) {
            if ((i & 1) == 0) {
                slot.publish(&fn_a, &g_ctx_a);
            } else {
                slot.publish(&fn_b, &g_ctx_b);
            }
        }
        stop.store(true, std::memory_order_release);
    }};
    bool coherent = true;
    while (!stop.load(std::memory_order_acquire)) {
        auto const cb = slot.load();
        bool const is_a = (cb.fn == &fn_a && cb.ctx == &g_ctx_a);
        bool const is_b = (cb.fn == &fn_b && cb.ctx == &g_ctx_b);
        if (!is_a && !is_b) {
            coherent = false;
        }
    }
    publisher.join();
    EXPECT_TRUE(coherent);
}

TEST_MAIN(statusbar_itc, itc_rt_callback_slot_test)
