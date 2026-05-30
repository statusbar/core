// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/itc/itc_telemetry_counter.hpp"

#include "statusbar/test/test.hpp"

#include <cstdint>
#include <thread>

using statusbar::itc::TelemetryCounter;

TEST(sync_telemetry_counter_init, fresh_counter_is_zero)
{
    TelemetryCounter<uint64_t> c;
    EXPECT_EQ(c.load(), 0U);
}

TEST(sync_telemetry_counter_init, initial_value_ctor)
{
    TelemetryCounter<uint64_t> c{42};
    EXPECT_EQ(c.load(), 42U);
}

TEST(sync_telemetry_counter_add, default_add_increments_by_one)
{
    TelemetryCounter<uint64_t> c;
    c.add();
    c.add();
    EXPECT_EQ(c.load(), 2U);
}

TEST(sync_telemetry_counter_add, add_n)
{
    TelemetryCounter<int64_t> c;
    c.add(5);
    c.add(7);
    EXPECT_EQ(c.load(), 12);
}

TEST(sync_telemetry_counter_reset, reset_zeroes)
{
    TelemetryCounter<uint64_t> c{99};
    c.add(1);
    c.reset();
    EXPECT_EQ(c.load(), 0U);
}

TEST(sync_telemetry_counter_threaded, producer_adds_are_all_counted)
{
    TelemetryCounter<uint64_t> c;
    constexpr uint64_t k_iterations = 100000;
    std::thread producer{[&]() noexcept {
        for (uint64_t i = 0; i < k_iterations; ++i) {
            c.add();
        }
    }};
    producer.join();
    EXPECT_EQ(c.load(), k_iterations);
}

TEST_MAIN(statusbar_itc, itc_telemetry_counter_test)
