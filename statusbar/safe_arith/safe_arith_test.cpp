// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/safe_arith/safe_arith.hpp"

#include "statusbar/test/test.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

using namespace statusbar;

TEST(statusbar_safe_arith, can_multiply_small_values_succeeds)
{
    EXPECT_TRUE(can_multiply<size_t>(0, 0));
    EXPECT_TRUE(can_multiply<size_t>(1, 0));
    EXPECT_TRUE(can_multiply<size_t>(0, 1));
    EXPECT_TRUE(can_multiply<size_t>(123, 456));
}

TEST(statusbar_safe_arith, can_multiply_at_max_succeeds)
{
    EXPECT_TRUE(can_multiply<size_t>(std::numeric_limits<size_t>::max(), 0));
    EXPECT_TRUE(can_multiply<size_t>(std::numeric_limits<size_t>::max(), 1));
    EXPECT_TRUE(can_multiply<size_t>(0, std::numeric_limits<size_t>::max()));
    EXPECT_TRUE(can_multiply<size_t>(1, std::numeric_limits<size_t>::max()));
}

TEST(statusbar_safe_arith, can_multiply_overflow_detected)
{
    EXPECT_FALSE(can_multiply<size_t>(std::numeric_limits<size_t>::max(), 2));
    EXPECT_FALSE(can_multiply<size_t>(2, std::numeric_limits<size_t>::max()));
    EXPECT_FALSE(can_multiply<uint32_t>(0x10000U, 0x10000U));  // 2^16 * 2^16 = 2^32
    EXPECT_FALSE(can_multiply<uint16_t>(0x100U, 0x100U));      // 2^8 * 2^8 = 2^16
}

TEST(statusbar_safe_arith, can_multiply_boundary)
{
    constexpr auto max32 = std::numeric_limits<uint32_t>::max();
    EXPECT_TRUE(can_multiply<uint32_t>(max32 / 7, 7));
    EXPECT_FALSE(can_multiply<uint32_t>(max32 / 7 + 1, 7));
}

TEST(statusbar_safe_arith, can_add_small_values_succeeds)
{
    EXPECT_TRUE(can_add<size_t>(0, 0));
    EXPECT_TRUE(can_add<size_t>(1, 2));
    EXPECT_TRUE(can_add<size_t>(123, 456));
}

TEST(statusbar_safe_arith, can_add_at_max_succeeds)
{
    constexpr auto max = std::numeric_limits<size_t>::max();
    EXPECT_TRUE(can_add<size_t>(max, 0));
    EXPECT_TRUE(can_add<size_t>(0, max));
    EXPECT_TRUE(can_add<size_t>(max - 1, 1));
    EXPECT_TRUE(can_add<size_t>(1, max - 1));
}

TEST(statusbar_safe_arith, can_add_overflow_detected)
{
    constexpr auto max = std::numeric_limits<size_t>::max();
    EXPECT_FALSE(can_add<size_t>(max, 1));
    EXPECT_FALSE(can_add<size_t>(1, max));
    EXPECT_FALSE(can_add<size_t>(max, max));
    EXPECT_FALSE(can_add<uint32_t>(std::numeric_limits<uint32_t>::max(), 1));
    EXPECT_FALSE(can_add<uint16_t>(std::numeric_limits<uint16_t>::max(), 1));
}

TEST(statusbar_safe_arith, constexpr_evaluable)
{
    static_assert(can_multiply<size_t>(7, 8));
    static_assert(!can_multiply<uint16_t>(0x100U, 0x100U));
    static_assert(can_add<size_t>(10, 20));
    static_assert(!can_add<uint16_t>(std::numeric_limits<uint16_t>::max(), 1));
}

TEST_MAIN(statusbar_safe_arith, safe_arith_test)
