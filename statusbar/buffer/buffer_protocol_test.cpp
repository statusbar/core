// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/status/status.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <system_error>

using namespace statusbar;

using namespace statusbar::protocol;

//
// Test Protocol Type
//

struct TestFixedProtocol
{
    static constexpr size_t LENGTH = 8;
    uint32_t field1;
    uint32_t field2;
};

template <>
struct statusbar::traits::is_serializable_fixed_struct<TestFixedProtocol> : std::true_type
{};

[[nodiscard]] auto load_unchecked(std::span<uint8_t const> const buf, TestFixedProtocol* const item) noexcept
{
    BufferDeserializerBuilder{buf}.parse_unchecked(&item->field1).parse_unchecked(&item->field2);
    return item->LENGTH;
}

[[nodiscard]] auto store_unchecked(std::span<uint8_t> const buf, TestFixedProtocol const& item) noexcept
{
    BufferSerializerBuilderWithBuffer{buf}.append_unchecked(item.field1).append_unchecked(item.field2);
    return item.LENGTH;
}

//
// Tests: wire_size()
//

TEST(fixed_size_protocol, wire_size_returns_length)
{
    TestFixedProtocol proto{.field1 = 0x12345678, .field2 = 0x9ABCDEF0};

    auto const size = wire_size(proto);

    EXPECT_EQ(size, 8);
}

//
// Tests: can_load()
//

TEST(fixed_size_protocol, can_load_sufficient_buffer)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t const> const buf{data};
    TestFixedProtocol proto{};

    auto const status = can_load(buf, &proto);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
}

TEST(fixed_size_protocol, can_load_exact_buffer)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t const> const buf{data};
    TestFixedProtocol proto{};

    auto const status = can_load(buf, &proto);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
}

TEST(fixed_size_protocol, can_load_insufficient_buffer)
{
    std::array<uint8_t, 5> data{};
    std::span<uint8_t const> const buf{data};
    TestFixedProtocol proto{};

    auto const status = can_load(buf, &proto);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

//
// Tests: can_store()
//

TEST(fixed_size_protocol, can_store_sufficient_buffer)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t> const buf{data};
    TestFixedProtocol proto{.field1 = 0x12345678, .field2 = 0x9ABCDEF0};

    auto const status = can_store(buf, proto);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
}

TEST(fixed_size_protocol, can_store_exact_buffer)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t> const buf{data};
    TestFixedProtocol proto{.field1 = 0x12345678, .field2 = 0x9ABCDEF0};

    auto const status = can_store(buf, proto);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
}

TEST(fixed_size_protocol, can_store_insufficient_buffer)
{
    std::array<uint8_t, 5> data{};
    std::span<uint8_t> const buf{data};
    TestFixedProtocol proto{.field1 = 0x12345678, .field2 = 0x9ABCDEF0};

    auto const status = can_store(buf, proto);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

//
// Tests: load()
//

TEST(fixed_size_protocol, load_deserializes_correctly)
{
    std::array<uint8_t, 10> data{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0xFF, 0xFF};
    std::span<uint8_t const> const buf{data};
    TestFixedProtocol proto{};

    auto const status = load(buf, &proto);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
    // Raw load, no byte-order conversion on little-endian systems
    EXPECT_EQ(proto.field1, 0x78563412);
    EXPECT_EQ(proto.field2, 0xF0DEBC9A);
}

TEST(fixed_size_protocol, load_insufficient_buffer)
{
    std::array<uint8_t, 5> data{};
    std::span<uint8_t const> const buf{data};
    TestFixedProtocol proto{};

    auto const status = load(buf, &proto);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

//
// Tests: store()
//

TEST(fixed_size_protocol, store_serializes_correctly)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t> const buf{data};
    TestFixedProtocol proto{.field1 = 0x12345678, .field2 = 0x9ABCDEF0};

    auto const status = store(buf, proto);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
    // Raw store, no byte-order conversion on little-endian systems
    EXPECT_EQ(data[0], 0x78);
    EXPECT_EQ(data[1], 0x56);
    EXPECT_EQ(data[2], 0x34);
    EXPECT_EQ(data[3], 0x12);
    EXPECT_EQ(data[4], 0xF0);
    EXPECT_EQ(data[5], 0xDE);
    EXPECT_EQ(data[6], 0xBC);
    EXPECT_EQ(data[7], 0x9A);
}

TEST(fixed_size_protocol, store_insufficient_buffer)
{
    std::array<uint8_t, 5> data{};
    std::span<uint8_t> const buf{data};
    TestFixedProtocol proto{.field1 = 0x12345678, .field2 = 0x9ABCDEF0};

    auto const status = store(buf, proto);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

//
// Tests: load_unchecked()
//

TEST(fixed_size_protocol, load_unchecked_deserializes_without_validation)
{
    std::array<uint8_t, 10> data{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0xFF, 0xFF};
    std::span<uint8_t const> const buf{data};
    TestFixedProtocol proto{};

    auto const bytes_consumed = load_unchecked(buf, &proto);

    EXPECT_EQ(bytes_consumed, 8);
    EXPECT_EQ(proto.field1, 0x44332211);
    EXPECT_EQ(proto.field2, 0x88776655);
}

//
// Tests: store_unchecked()
//

TEST(fixed_size_protocol, store_unchecked_serializes_without_validation)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t> const buf{data};
    TestFixedProtocol proto{.field1 = 0xAABBCCDD, .field2 = 0x11223344};

    auto const bytes_written = store_unchecked(buf, proto);

    EXPECT_EQ(bytes_written, 8);
    EXPECT_EQ(data[0], 0xDD);
    EXPECT_EQ(data[1], 0xCC);
    EXPECT_EQ(data[2], 0xBB);
    EXPECT_EQ(data[3], 0xAA);
    EXPECT_EQ(data[4], 0x44);
    EXPECT_EQ(data[5], 0x33);
    EXPECT_EQ(data[6], 0x22);
    EXPECT_EQ(data[7], 0x11);
}

//
// Tests: Round-trip serialization/deserialization
//

TEST(fixed_size_protocol, round_trip_preserves_values)
{
    TestFixedProtocol original{.field1 = 0x12345678, .field2 = 0xABCDEF01};
    std::array<uint8_t, 8> buffer{};

    // Serialize
    auto const store_status = store(std::span<uint8_t>{buffer}, original);
    EXPECT_TRUE(store_status);

    // Deserialize
    TestFixedProtocol deserialized{};
    auto const load_status = load(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_TRUE(load_status);

    // Verify
    EXPECT_EQ(deserialized.field1, original.field1);
    EXPECT_EQ(deserialized.field2, original.field2);
}

TEST(fixed_size_protocol, round_trip_unchecked_preserves_values)
{
    TestFixedProtocol original{.field1 = 0xFEDCBA98, .field2 = 0x76543210};
    std::array<uint8_t, 8> buffer{};

    // Serialize unchecked
    auto const bytes_written = store_unchecked(std::span<uint8_t>{buffer}, original);
    EXPECT_EQ(bytes_written, 8);

    // Deserialize unchecked
    TestFixedProtocol deserialized{};
    auto const bytes_read = load_unchecked(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_EQ(bytes_read, 8);

    // Verify
    EXPECT_EQ(deserialized.field1, original.field1);
    EXPECT_EQ(deserialized.field2, original.field2);
}

TEST(fixed_size_protocol, round_trip_preserves_values_with_builder)
{
    TestFixedProtocol original{.field1 = 0x12345678, .field2 = 0xABCDEF01};
    std::array<uint8_t, 16> storage;
    MutableBuffer buffer(storage);
    BufferSerializerBuilder serializer(buffer);
    serializer.append(original);
    EXPECT_TRUE(serializer);

    // Deserialize
    TestFixedProtocol deserialized{};
    BufferDeserializerBuilder deserializer(buffer.get_span());
    deserializer.parse(&deserialized);
    EXPECT_TRUE(deserializer);

    // Verify
    EXPECT_EQ(deserialized.field1, original.field1);
    EXPECT_EQ(deserialized.field2, original.field2);

    if constexpr (std::endian::native == std::endian::big) {
        EXPECT_EQ(storage[0], 0x12U);
        EXPECT_EQ(storage[1], 0x34U);
    } else {
        EXPECT_EQ(storage[0], 0x78U);
        EXPECT_EQ(storage[1], 0x56U);
    }
}

//
// Main test runner
//

TEST_MAIN(statusbar_buffer, buffer_protocol_test)