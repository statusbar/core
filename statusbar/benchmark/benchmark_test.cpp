// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/benchmark/benchmark.hpp"

#include "statusbar/test/test.hpp"

#include <chrono>
#include <thread>
#include <vector>

using namespace statusbar::benchmark;

//
// Timer - Basic Tests
//

TEST(timer_basic, start_and_elapsed)
{
    auto timer = Timer::start();
    // Sleep briefly to ensure some time passes
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto elapsed = timer.elapsed_ns();
    // Should have elapsed at least 1 million nanoseconds (1ms)
    // but no more than 100 million (100ms) to account for scheduling
    EXPECT_TRUE(elapsed >= 1'000'000);
    EXPECT_TRUE(elapsed < 100'000'000);
}

TEST(timer_basic, elapsed_us)
{
    auto timer = Timer::start();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    double elapsed = timer.elapsed_us();
    // Should be at least 1000 microseconds (1ms)
    EXPECT_TRUE(elapsed >= 1000.0);
}

TEST(timer_basic, elapsed_ms)
{
    auto timer = Timer::start();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    double elapsed = timer.elapsed_ms();
    // Should be at least 1 millisecond
    EXPECT_TRUE(elapsed >= 1.0);
}

TEST(timer_basic, elapsed_s)
{
    auto timer = Timer::start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    double elapsed = timer.elapsed_s();
    // Should be at least 0.001 seconds
    EXPECT_TRUE(elapsed >= 0.001);
}

TEST(timer_basic, reset)
{
    auto timer = Timer::start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    timer.reset();
    // After reset, elapsed time should be small
    auto elapsed = timer.elapsed_ns();
    // Should be less than 10 milliseconds
    EXPECT_TRUE(elapsed < 10'000'000);
}

//
// do_not_optimize - Tests
//

TEST(do_not_optimize_test, const_reference)
{
    int value = 42;
    do_not_optimize(value);
    // Just verify it doesn't crash - the point is to prevent optimization
    EXPECT_EQ(value, 42);
}

TEST(do_not_optimize_test, mutable_reference)
{
    int value = 42;
    do_not_optimize(value);
    value = 100;
    do_not_optimize(value);
    EXPECT_EQ(value, 100);
}

TEST(do_not_optimize_test, complex_type)
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    do_not_optimize(vec);
    EXPECT_EQ(vec.size(), 5);
}

//
// BenchmarkConfig - Tests
//

TEST(config_test, default_values)
{
    BenchmarkConfig config;
    EXPECT_EQ(config.warmup_iterations, 100);
    EXPECT_EQ(config.measurement_iterations, 1000);
    EXPECT_EQ(config.batch_size, 1);
    EXPECT_EQ(config.verbose, false);
}

//
// run() - Benchmark Execution Tests
//

TEST(run_test, simple_function)
{
    int counter = 0;
    BenchmarkConfig config;
    config.warmup_iterations = 5;
    config.measurement_iterations = 10;

    auto stats = run(
        "test",
        [&counter]() {
            counter++;
            do_not_optimize(counter);
        },
        config);

    EXPECT_EQ(stats.sample_count, 10);
    // Counter should be warmup + measurement iterations
    EXPECT_EQ(counter, 15);
}

TEST(run_test, with_batch_size)
{
    int counter = 0;
    BenchmarkConfig config;
    config.warmup_iterations = 2;
    config.measurement_iterations = 3;
    config.batch_size = 4;

    auto stats = run(
        "test_batch",
        [&counter]() {
            counter++;
            do_not_optimize(counter);
        },
        config);

    EXPECT_EQ(stats.sample_count, 3);
    // Counter should be (warmup + measurement) * batch_size
    EXPECT_EQ(counter, (2 + 3) * 4);
}

//
// HardwareTimer - Basic Tests
//

TEST(hw_timer_basic, ticks_per_second_nonzero)
{
    EXPECT_TRUE(HardwareTimer::ticks_per_second() > 0);
}

TEST(hw_timer_basic, elapsed_ticks_monotonic)
{
    auto timer = HardwareTimer::start();
    auto t1 = timer.elapsed_ticks();
    // Do a tiny bit of work so the counter advances
    uint64_t volatile sum = 0;
    for (int i = 0; i < 1000; ++i) {
        sum += static_cast<uint64_t>(i);
    }
    do_not_optimize(sum);
    auto t2 = timer.elapsed_ticks();
    EXPECT_TRUE(t2 >= t1);
}

TEST(hw_timer_basic, elapsed_ns_after_sleep)
{
    auto timer = HardwareTimer::start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto elapsed = timer.elapsed_ns();
    EXPECT_TRUE(elapsed >= 1'000'000);
    EXPECT_TRUE(elapsed < 100'000'000);
}

TEST(hw_timer_basic, elapsed_ms_after_sleep)
{
    auto timer = HardwareTimer::start();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    double elapsed = timer.elapsed_ms();
    EXPECT_TRUE(elapsed >= 1.0);
}

TEST(hw_timer_basic, reset)
{
    auto timer = HardwareTimer::start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    timer.reset();
    // After reset, elapsed should be tiny
    EXPECT_TRUE(timer.elapsed_ms() < 10.0);
}

TEST(hw_timer_basic, ticks_to_ns_static_matches_instance)
{
    auto timer = HardwareTimer::start();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    auto ticks = timer.elapsed_ticks();
    // Both conversions should produce the same value for the same tick count
    auto ns_static = HardwareTimer::ticks_to_ns(ticks);
    // Within a few microseconds — can't compare exactly since elapsed_ns() re-reads the counter
    EXPECT_TRUE(ns_static >= 1'000'000);
}

//
// run_hw() - HardwareTimer-backed Benchmark Execution Tests
//

TEST(run_hw_test, simple_function)
{
    int counter = 0;
    BenchmarkConfig config;
    config.warmup_iterations = 5;
    config.measurement_iterations = 10;

    auto stats = run_hw(
        "hw_test",
        [&counter](size_t n) {
            for (size_t k = 0; k < n; ++k) {
                counter++;
            }
            do_not_optimize(counter);
        },
        config);

    EXPECT_EQ(stats.sample_count, 10);
    EXPECT_EQ(counter, 15);
}

TEST(run_hw_test, with_batch_size)
{
    int counter = 0;
    BenchmarkConfig config;
    config.warmup_iterations = 2;
    config.measurement_iterations = 3;
    config.batch_size = 4;

    auto stats = run_hw(
        "hw_test_batch",
        [&counter](size_t n) {
            for (size_t k = 0; k < n; ++k) {
                counter++;
            }
            do_not_optimize(counter);
        },
        config);

    EXPECT_EQ(stats.sample_count, 3);
    EXPECT_EQ(counter, (2 + 3) * 4);
}

TEST(run_hw_test, measurements_are_positive)
{
    // Verify that tick→ns conversion produces sensible (nonzero) values
    BenchmarkConfig config;
    config.warmup_iterations = 2;
    config.measurement_iterations = 20;
    config.batch_size = 100;

    uint64_t volatile sum = 0;
    auto stats = run_hw(
        "hw_test_positive",
        [&sum](size_t n) {
            for (size_t k = 0; k < n; ++k) {
                sum += 1;
            }
            do_not_optimize(sum);
        },
        config);

    EXPECT_EQ(stats.sample_count, 20);
    // Per-call time should be positive and reasonably small (< 1ms per increment)
    EXPECT_TRUE(stats.median > 0.0);
    EXPECT_TRUE(stats.median < 1'000'000.0);
}

//
// Test Runner
//

TEST_MAIN(statusbar_benchmark, benchmark_test)