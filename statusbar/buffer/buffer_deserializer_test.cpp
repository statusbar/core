// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <span>
#include <system_error>
#include <vector>

using namespace statusbar;

TEST(buffer_deserializer_basic, construct_and_position)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    EXPECT_EQ(deserializer.position(), 0);
    EXPECT_EQ(deserializer.available(), 10);
}

TEST(buffer_deserializer_basic, can_peek)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    EXPECT_TRUE(deserializer.can_peek(5));
    EXPECT_TRUE(deserializer.can_peek(10));
    EXPECT_FALSE(deserializer.can_peek(11));
}

TEST(buffer_deserializer_basic, peek)
{
    std::array<uint8_t, 10> data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    auto const peeked = deserializer.peek(5);
    EXPECT_EQ(peeked.size(), 5);
    EXPECT_EQ(peeked[0], 0);
    EXPECT_EQ(peeked[4], 4);

    // Position unchanged after peek
    EXPECT_EQ(deserializer.position(), 0);
}

TEST(buffer_deserializer_basic, consume)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    auto const status = deserializer.consume(5);

    EXPECT_TRUE(status);
    EXPECT_EQ(deserializer.position(), 5);
    EXPECT_EQ(deserializer.available(), 5);
}

TEST(buffer_deserializer_basic, consume_insufficient_data)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    auto const status = deserializer.consume(15);

    EXPECT_FALSE(status);
    EXPECT_EQ(deserializer.position(), 0);  // Position unchanged
}

TEST(buffer_deserializer_basic, reset)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    (void)deserializer.consume(5);
    EXPECT_EQ(deserializer.position(), 5);

    EXPECT_TRUE(deserializer.reset());
    EXPECT_EQ(deserializer.position(), 0);
    EXPECT_EQ(deserializer.available(), 10);
}

TEST(buffer_deserializer_basic, reset_to_position)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    EXPECT_TRUE(deserializer.reset(3));
    EXPECT_EQ(deserializer.position(), 3);
    EXPECT_EQ(deserializer.available(), 7);
}

//
// std::span<uint8_t const>.cppm tests - BufferDeserializer parse
//

TEST(buffer_deserializer_parse, parse_uint8)
{
    std::array<uint8_t, 10> data{0x42, 0x43, 0x44};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    uint8_t value = 0;
    auto const status = deserializer.parse(&value);

    EXPECT_TRUE(status);
    EXPECT_EQ(value, 0x42);
    EXPECT_EQ(deserializer.position(), 1);
}

TEST(buffer_deserializer_parse, parse_uint32)
{
    std::array<uint8_t, 10> data{0x12, 0x34, 0x56, 0x78};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    uint32_t value = 0;
    auto const status = deserializer.parse(&value);

    EXPECT_TRUE(status);
    EXPECT_EQ(deserializer.position(), 4);
}

TEST(buffer_deserializer_parse, parse_multiple)
{
    std::array<uint8_t, 10> data{0x01, 0x02, 0x03, 0x04, 0x05};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    uint8_t v1 = 0;
    uint16_t v2 = 0;
    uint8_t v3 = 0;

    (void)deserializer.parse(&v1);
    (void)deserializer.parse(&v2);
    (void)deserializer.parse(&v3);

    EXPECT_EQ(v1, 0x01);
    EXPECT_EQ(v3, 0x04);
    EXPECT_EQ(deserializer.position(), 4);
}

TEST(buffer_deserializer_parse, parse_insufficient_data)
{
    std::array<uint8_t, 2> data{0x12, 0x34};
    std::span<uint8_t const> const buf{data};
    BufferDeserializer deserializer{buf};

    uint32_t value = 0;
    auto const status = deserializer.parse(&value);

    EXPECT_FALSE(status);
    EXPECT_EQ(deserializer.position(), 0);  // Position unchanged
}

//
// std::span<uint8_t const>.cppm tests - BufferBuilder
//

//
// Edge cases: every wire-format decoder in the codebase walks one of these
// objects across attacker-controlled byte counts. The boundary behavior
// pinned down here is what those decoders rely on.
//

TEST(buffer_deserializer_safety, reset_to_eof_position)
{
    std::array<uint8_t, 10> data{};
    BufferDeserializer d{std::span<uint8_t const>{data}};
    EXPECT_FALSE(d.reset(10));    // position == size is treated as out-of-bounds
    EXPECT_EQ(d.available(), 0);  // and leaves the deserializer at EOF
}

TEST(buffer_deserializer_safety, reset_to_huge_position_is_eof)
{
    std::array<uint8_t, 10> data{};
    BufferDeserializer d{std::span<uint8_t const>{data}};
    EXPECT_FALSE(d.reset(std::numeric_limits<size_t>::max()));
    EXPECT_EQ(d.available(), 0);
}

TEST(buffer_deserializer_safety, parse_after_full_consume_fails)
{
    std::array<uint8_t, 4> data{1, 2, 3, 4};
    BufferDeserializer d{std::span<uint8_t const>{data}};
    EXPECT_TRUE(d.consume(4));
    EXPECT_EQ(d.available(), 0);
    uint8_t v = 0;
    EXPECT_FALSE(d.parse(&v));
}

TEST(buffer_deserializer_safety, empty_buffer_can_reset_to_zero)
{
    std::span<uint8_t const> empty{};
    BufferDeserializer d{empty};
    EXPECT_TRUE(d.reset(0));   // valid: empty buffer at position 0 is fine
    EXPECT_FALSE(d.reset(1));  // any non-zero position into empty is EOF
    EXPECT_EQ(d.available(), 0);
}

TEST(buffer_deserializer_safety, peek_zero_returns_empty_span)
{
    std::array<uint8_t, 4> data{1, 2, 3, 4};
    BufferDeserializer d{std::span<uint8_t const>{data}};
    auto const span = d.peek(0);
    EXPECT_EQ(span.size(), size_t{0});
    EXPECT_EQ(d.position(), size_t{0});  // peek doesn't advance
}

TEST(buffer_deserializer_safety, consume_huge_count_fails_no_advance)
{
    std::array<uint8_t, 4> data{1, 2, 3, 4};
    BufferDeserializer d{std::span<uint8_t const>{data}};
    EXPECT_FALSE(d.consume(std::numeric_limits<size_t>::max()));
    EXPECT_EQ(d.position(), size_t{0});
}

//
// Main test runner
//

TEST_MAIN(statusbar_buffer, buffer_deserializer_test)