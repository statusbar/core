// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/itc/itc_atomic_triple_buffer.hpp"

#include "statusbar/test/test.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

using statusbar::itc::AtomicTripleBuffer;

namespace {

struct Pod
{
    int64_t a;
    int64_t b;
};

}  // namespace

TEST(itc_atb_typed, publish_then_consume_returns_value)
{
    AtomicTripleBuffer<Pod> buf;
    EXPECT_FALSE(buf.can_consume());

    buf.publish(Pod{.a = 42, .b = -42});
    EXPECT_TRUE(buf.can_consume());

    Pod const v = buf.consume();
    EXPECT_EQ(v.a, int64_t{42});
    EXPECT_EQ(v.b, int64_t{-42});
    EXPECT_FALSE(buf.can_consume());
}

TEST(itc_atb_typed, initial_value_ctor_yields_initial_once)
{
    AtomicTripleBuffer<Pod> buf{Pod{.a = 7, .b = 11}};
    EXPECT_TRUE(buf.can_consume());

    Pod const v = buf.consume();
    EXPECT_EQ(v.a, int64_t{7});
    EXPECT_EQ(v.b, int64_t{11});
    EXPECT_FALSE(buf.can_consume());

    // Subsequent consume without a publish returns the same value.
    Pod const v2 = buf.consume();
    EXPECT_EQ(v2.a, int64_t{7});
    EXPECT_EQ(v2.b, int64_t{11});
}

TEST(itc_atb_typed, multiple_publishes_then_consume_returns_latest)
{
    AtomicTripleBuffer<Pod> buf;
    buf.publish(Pod{.a = 1, .b = -1});
    buf.publish(Pod{.a = 2, .b = -2});
    buf.publish(Pod{.a = 3, .b = -3});
    buf.publish(Pod{.a = 4, .b = -4});
    buf.publish(Pod{.a = 5, .b = -5});

    Pod const v = buf.consume();
    EXPECT_EQ(v.a, int64_t{5});
    EXPECT_EQ(v.b, int64_t{-5});
    EXPECT_EQ(buf.overruns(), uint64_t{4});
}

TEST(itc_atb_typed, spsc_stress_no_torn_reads_and_monotonic)
{
    struct Quad
    {
        int64_t a;  // sequence number
        int64_t b;  // ~a (bitwise complement of a)
        int64_t c;  // a + 1
        int64_t d;  // ~(a + 1)
    };

    AtomicTripleBuffer<Quad> buf;
    std::atomic<bool> stop{false};

    // Producer publishes a strictly increasing sequence as fast as it
    // can. Consumer loops, picks up each visible state, and checks
    // both the field-redundancy invariants and monotonicity.
    std::thread producer{[&]() noexcept {
        int64_t n = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            buf.publish(
                Quad{
                    .a = n,
                    .b = ~n,
                    .c = n + 1,
                    .d = ~(n + 1),
                });
            ++n;
        }
    }};

    int64_t last_seen = -1;
    int64_t observations = 0;
    int64_t const observations_target = 100'000;
    int64_t const time_budget_ms = 500;
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{time_budget_ms};
    while (observations < observations_target && std::chrono::steady_clock::now() < deadline) {
        if (!buf.can_consume()) {
            continue;
        }
        Quad const q = buf.consume();
        EXPECT_EQ(q.a + q.b, int64_t{-1});  // a == ~b
        EXPECT_EQ(q.c + q.d, int64_t{-1});  // c == ~d
        EXPECT_EQ(q.c, q.a + 1);            // c == a + 1
        EXPECT_TRUE(q.a > last_seen);       // strictly newer
        last_seen = q.a;
        ++observations;
    }

    stop.store(true, std::memory_order_relaxed);
    producer.join();

    EXPECT_TRUE(observations >= int64_t{100});  // we observed *something* in the budget
}

TEST_MAIN(statusbar_itc, itc_atomic_triple_buffer_test)
