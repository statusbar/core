// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/ieee/ieee.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <system_error>

using namespace statusbar::ieee;

//
// Single value load/store tests
//

TEST(ieee_buffer, load_single_quadlet)
{
    // Network-ordered bytes: 0x12345678
    uint8_t const packet[4] = {0x12, 0x34, 0x56, 0x78};
    std::span<uint8_t const> buf{packet, 4};

    quadlet_t value;
    auto result = load(buf, &value);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 4U);
    EXPECT_EQ(value, 0x12345678U);
}

TEST(ieee_buffer, store_single_quadlet)
{
    uint8_t packet[4] = {0, 0, 0, 0};
    std::span<uint8_t> buf{packet, 4};

    quadlet_t value = 0xAABBCCDD;
    auto result = store(buf, value);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 4U);
    EXPECT_EQ(packet[0], 0xAA);
    EXPECT_EQ(packet[1], 0xBB);
    EXPECT_EQ(packet[2], 0xCC);
    EXPECT_EQ(packet[3], 0xDD);
}

TEST(ieee_buffer, load_single_doublet)
{
    uint8_t const packet[2] = {0xAB, 0xCD};
    std::span<uint8_t const> buf{packet, 2};

    doublet_t value;
    auto result = load(buf, &value);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 2U);
    EXPECT_EQ(value, 0xABCDu);
}

TEST(ieee_buffer, store_single_doublet)
{
    uint8_t packet[2] = {0, 0};
    std::span<uint8_t> buf{packet, 2};

    doublet_t value = 0x1234;
    auto result = store(buf, value);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 2U);
    EXPECT_EQ(packet[0], 0x12);
    EXPECT_EQ(packet[1], 0x34);
}

TEST(ieee_buffer, load_single_octlet)
{
    uint8_t const packet[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    std::span<uint8_t const> buf{packet, 8};

    octlet_t value;
    auto result = load(buf, &value);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 8U);
    EXPECT_EQ(value, 0x1122334455667788ull);
}

TEST(ieee_buffer, store_single_octlet)
{
    uint8_t packet[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    std::span<uint8_t> buf{packet, 8};

    octlet_t value = 0xAABBCCDDEEFF0011ull;
    auto result = store(buf, value);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 8U);
    EXPECT_EQ(packet[0], 0xAA);
    EXPECT_EQ(packet[1], 0xBB);
    EXPECT_EQ(packet[2], 0xCC);
    EXPECT_EQ(packet[3], 0xDD);
    EXPECT_EQ(packet[4], 0xEE);
    EXPECT_EQ(packet[5], 0xFF);
    EXPECT_EQ(packet[6], 0x00);
    EXPECT_EQ(packet[7], 0x11);
}

//
// Array load/store tests
//

TEST(ieee_buffer, load_array_of_quadlets)
{
    uint8_t const packet[12] = {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03};
    std::span<uint8_t const> buf{packet, 12};

    std::array<quadlet_t, 3> values;
    auto result = load(buf, &values);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 12U);
    EXPECT_EQ(values[0], 1U);
    EXPECT_EQ(values[1], 2U);
    EXPECT_EQ(values[2], 3U);
}

TEST(ieee_buffer, store_array_of_quadlets)
{
    uint8_t packet[12] = {};
    std::span<uint8_t> buf{packet, 12};

    std::array<quadlet_t, 3> const values = {100, 200, 300};
    auto result = store(buf, values);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 12U);
    EXPECT_EQ(packet[0], 0x00);
    EXPECT_EQ(packet[1], 0x00);
    EXPECT_EQ(packet[2], 0x00);
    EXPECT_EQ(packet[3], 0x64);  // 100
    EXPECT_EQ(packet[4], 0x00);
    EXPECT_EQ(packet[5], 0x00);
    EXPECT_EQ(packet[6], 0x00);
    EXPECT_EQ(packet[7], 0xC8);  // 200
    EXPECT_EQ(packet[8], 0x00);
    EXPECT_EQ(packet[9], 0x00);
    EXPECT_EQ(packet[10], 0x01);
    EXPECT_EQ(packet[11], 0x2C);  // 300
}

TEST(ieee_buffer, load_array_of_doublets)
{
    uint8_t const packet[6] = {0x00, 0x0A, 0x00, 0x14, 0x00, 0x1E};
    std::span<uint8_t const> buf{packet, 6};

    std::array<doublet_t, 3> values;
    auto result = load(buf, &values);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 6U);
    EXPECT_EQ(values[0], 10U);
    EXPECT_EQ(values[1], 20U);
    EXPECT_EQ(values[2], 30U);
}

TEST(ieee_buffer, store_array_of_doublets)
{
    uint8_t packet[6] = {};
    std::span<uint8_t> buf{packet, 6};

    std::array<doublet_t, 3> const values = {0x1111, 0x2222, 0x3333};
    auto result = store(buf, values);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 6U);
    EXPECT_EQ(packet[0], 0x11);
    EXPECT_EQ(packet[1], 0x11);
    EXPECT_EQ(packet[2], 0x22);
    EXPECT_EQ(packet[3], 0x22);
    EXPECT_EQ(packet[4], 0x33);
    EXPECT_EQ(packet[5], 0x33);
}

//
// Span load/store tests
//

TEST(ieee_buffer, load_span_of_quadlets)
{
    uint8_t const packet[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    std::span<uint8_t const> buf{packet, 8};

    std::array<quadlet_t, 2> storage;
    std::span<quadlet_t, 2> values{storage};
    auto result = load(buf, values);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 8U);
    EXPECT_EQ(values[0], 0xDEADBEEFu);
    EXPECT_EQ(values[1], 0xCAFEBABEu);
}

TEST(ieee_buffer, store_span_of_quadlets)
{
    uint8_t packet[8] = {};
    std::span<uint8_t> buf{packet, 8};

    std::array<quadlet_t, 2> const storage = {0x11223344, 0x55667788};
    std::span<quadlet_t const, 2> values{storage};
    auto result = store(buf, values);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 8U);
    EXPECT_EQ(packet[0], 0x11);
    EXPECT_EQ(packet[1], 0x22);
    EXPECT_EQ(packet[2], 0x33);
    EXPECT_EQ(packet[3], 0x44);
    EXPECT_EQ(packet[4], 0x55);
    EXPECT_EQ(packet[5], 0x66);
    EXPECT_EQ(packet[6], 0x77);
    EXPECT_EQ(packet[7], 0x88);
}

TEST(ieee_buffer, load_span_of_octets)
{
    uint8_t const packet[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    std::span<uint8_t const> buf{packet, 4};

    std::array<octet_t, 4> storage;
    std::span<octet_t, 4> values{storage};
    auto result = load(buf, values);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 4U);
    EXPECT_EQ(values[0], 0xAAu);
    EXPECT_EQ(values[1], 0xBBu);
    EXPECT_EQ(values[2], 0xCCu);
    EXPECT_EQ(values[3], 0xDDu);
}

TEST(ieee_buffer, store_span_of_octets)
{
    uint8_t packet[4] = {};
    std::span<uint8_t> buf{packet, 4};

    std::array<octet_t, 4> const storage = {0x12, 0x34, 0x56, 0x78};
    std::span<octet_t const, 4> values{storage};
    auto result = store(buf, values);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 4U);
    EXPECT_EQ(packet[0], 0x12);
    EXPECT_EQ(packet[1], 0x34);
    EXPECT_EQ(packet[2], 0x56);
    EXPECT_EQ(packet[3], 0x78);
}

//
// Error handling tests
//

TEST(ieee_buffer, load_insufficient_buffer)
{
    uint8_t const packet[3] = {0x12, 0x34, 0x56};
    std::span<uint8_t const> buf{packet, 3};

    quadlet_t value;
    auto result = load(buf, &value);

    EXPECT_FALSE(result.has_value());
}

TEST(ieee_buffer, store_insufficient_buffer)
{
    uint8_t packet[3] = {};
    std::span<uint8_t> buf{packet, 3};

    quadlet_t value = 0x12345678;
    auto result = store(buf, value);

    EXPECT_FALSE(result.has_value());
}

TEST(ieee_buffer, load_array_insufficient_buffer)
{
    uint8_t const packet[10] = {};  // Need 12 bytes for 3 quadlets
    std::span<uint8_t const> buf{packet, 10};

    std::array<quadlet_t, 3> values;
    auto result = load(buf, &values);

    EXPECT_FALSE(result.has_value());
}

TEST(ieee_buffer, store_array_insufficient_buffer)
{
    uint8_t packet[10] = {};  // Need 12 bytes for 3 quadlets
    std::span<uint8_t> buf{packet, 10};

    std::array<quadlet_t, 3> const values = {1, 2, 3};
    auto result = store(buf, values);

    EXPECT_FALSE(result.has_value());
}

//
// Round-trip tests
//

TEST(ieee_buffer, round_trip_single_value)
{
    uint8_t packet[4];
    std::span<uint8_t> write_buf{packet, 4};

    quadlet_t const original = 0xFEEDFACE;
    auto store_result = store(write_buf, original);
    EXPECT_TRUE(store_result.has_value());

    std::span<uint8_t const> read_buf{packet, 4};
    quadlet_t loaded;
    auto load_result = load(read_buf, &loaded);
    EXPECT_TRUE(load_result.has_value());

    EXPECT_EQ(loaded, original);
}

TEST(ieee_buffer, round_trip_array)
{
    uint8_t packet[12];
    std::span<uint8_t> write_buf{packet, 12};

    std::array<quadlet_t, 3> const original = {0x11111111, 0x22222222, 0x33333333};
    auto store_result = store(write_buf, original);
    EXPECT_TRUE(store_result.has_value());

    std::span<uint8_t const> read_buf{packet, 12};
    std::array<quadlet_t, 3> loaded;
    auto load_result = load(read_buf, &loaded);
    EXPECT_TRUE(load_result.has_value());

    EXPECT_EQ(loaded[0], original[0]);
    EXPECT_EQ(loaded[1], original[1]);
    EXPECT_EQ(loaded[2], original[2]);
}

//
// Test driver main function
//

TEST_MAIN(statusbar_ieee, ieee_buffer_test)