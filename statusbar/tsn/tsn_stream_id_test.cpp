// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/status/status.hpp"
#include "statusbar/test/test.hpp"
#include "statusbar/tsn/tsn.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <system_error>

using namespace statusbar;
using namespace statusbar::tsn;
using namespace statusbar::ieee;

// Bring protocol functions into scope for ADL
using statusbar::protocol::can_load;
using statusbar::protocol::can_store;
using statusbar::protocol::load;
using statusbar::protocol::store;
using statusbar::protocol::wire_size;

//
// Tests: StreamId - Construction and basic accessors
//

TEST(stream_id, default_constructor_initializes_to_zero)
{
    StreamId stream_id{};

    EXPECT_FALSE(stream_id.is_set());
    EXPECT_EQ(stream_id.get_unique_id(), 0);
}

TEST(stream_id, parameterized_constructor_sets_values)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id{mac, 0x1234};

    EXPECT_TRUE(stream_id.is_set());
    EXPECT_EQ(stream_id.get_system_address().value[0], 0xAA);
    EXPECT_EQ(stream_id.get_system_address().value[5], 0xFF);
    EXPECT_EQ(stream_id.get_unique_id(), 0x1234);
}

TEST(stream_id, size_returns_8)
{
    EXPECT_EQ(StreamId::size(), 8);
}

TEST(stream_id, is_set_returns_false_for_zero_mac)
{
    Eui48 zero_mac{0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    StreamId stream_id{zero_mac, 0x1234};

    EXPECT_FALSE(stream_id.is_set());
}

TEST(stream_id, is_set_returns_false_for_broadcast_mac)
{
    Eui48 broadcast_mac{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    StreamId stream_id{broadcast_mac, 0x1234};

    EXPECT_FALSE(stream_id.is_set());
}

TEST(stream_id, is_set_returns_true_for_valid_mac)
{
    Eui48 valid_mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    StreamId stream_id{valid_mac, 0x1234};

    EXPECT_TRUE(stream_id.is_set());
}

//
// Tests: StreamId - Setters and getters
//

TEST(stream_id, set_system_address_updates_address)
{
    StreamId stream_id{};
    Eui48 mac{0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    stream_id.set_system_address(mac);

    auto const result = stream_id.get_system_address();
    EXPECT_EQ(result.value[0], 0x11);
    EXPECT_EQ(result.value[5], 0x66);
}

TEST(stream_id, set_unique_id_updates_id)
{
    StreamId stream_id{};

    stream_id.set_unique_id(0xABCD);

    EXPECT_EQ(stream_id.get_unique_id(), 0xABCD);
}

TEST(stream_id, increment_unique_id_increments_by_one)
{
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    StreamId stream_id{mac, 0x1000};

    stream_id.increment_unique_id();

    EXPECT_EQ(stream_id.get_unique_id(), 0x1001);
}

TEST(stream_id, increment_unique_id_wraps_at_max)
{
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    StreamId stream_id{mac, 0xFFFF};

    stream_id.increment_unique_id();

    EXPECT_EQ(stream_id.get_unique_id(), 0x0000);
}

//
// Tests: StreamId - uint64 conversion
//

TEST(stream_id, to_uint64_combines_address_and_id)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id{mac, 0x1234};

    auto const value = stream_id.to_uint64();

    // Expected: 0xAABBCCDDEEFF1234
    EXPECT_EQ(value, 0xAABBCCDDEEFF1234ULL);
}

TEST(stream_id, from_uint64_splits_to_address_and_id)
{
    StreamId stream_id{};

    stream_id.from_uint64(0x112233445566ABCDULL);

    auto const mac = stream_id.get_system_address();
    EXPECT_EQ(mac.value[0], 0x11);
    EXPECT_EQ(mac.value[1], 0x22);
    EXPECT_EQ(mac.value[2], 0x33);
    EXPECT_EQ(mac.value[3], 0x44);
    EXPECT_EQ(mac.value[4], 0x55);
    EXPECT_EQ(mac.value[5], 0x66);
    EXPECT_EQ(stream_id.get_unique_id(), 0xABCD);
}

TEST(stream_id, uint64_round_trip_preserves_value)
{
    Eui48 mac{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
    StreamId original{mac, 0xCDEF};

    auto const value = original.to_uint64();
    StreamId restored{};
    restored.from_uint64(value);

    EXPECT_EQ(restored.get_system_address().value[0], mac.value[0]);
    EXPECT_EQ(restored.get_system_address().value[5], mac.value[5]);
    EXPECT_EQ(restored.get_unique_id(), 0xCDEF);
}

//
// Tests: StreamId - Comparison operators
//

TEST(stream_id, equality_operator_compares_correctly)
{
    Eui48 mac1{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    Eui48 mac2{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id1{mac1, 0x1234};
    StreamId stream_id2{mac2, 0x1234};

    EXPECT_TRUE(stream_id1 == stream_id2);
}

TEST(stream_id, inequality_operator_detects_different_mac)
{
    Eui48 mac1{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    Eui48 mac2{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFE};
    StreamId stream_id1{mac1, 0x1234};
    StreamId stream_id2{mac2, 0x1234};

    EXPECT_TRUE(stream_id1 != stream_id2);
}

TEST(stream_id, inequality_operator_detects_different_unique_id)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id1{mac, 0x1234};
    StreamId stream_id2{mac, 0x1235};

    EXPECT_TRUE(stream_id1 != stream_id2);
}

TEST(stream_id, less_than_operator_compares_correctly)
{
    Eui48 mac1{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    Eui48 mac2{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id1{mac1, 0x1234};
    StreamId stream_id2{mac2, 0x1235};

    EXPECT_TRUE(stream_id1 < stream_id2);
}

//
// Tests: StreamId - wire_size()
//

TEST(stream_id, wire_size_returns_8)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id{mac, 0x1234};

    auto const size = wire_size(stream_id);

    EXPECT_EQ(size, 8);
}

//
// Tests: StreamId - can_load()
//

TEST(stream_id, can_load_sufficient_buffer)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t const> const buf{data};
    StreamId stream_id{};

    auto const status = can_load(buf, &stream_id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
}

TEST(stream_id, can_load_exact_buffer)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t const> const buf{data};
    StreamId stream_id{};

    auto const status = can_load(buf, &stream_id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
}

TEST(stream_id, can_load_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t const> const buf{data};
    StreamId stream_id{};

    auto const status = can_load(buf, &stream_id);

    EXPECT_FALSE(statusbar::is_success(status));
    EXPECT_EQ(status.error().value(), static_cast<int>(statusbar::BufferError::insufficient_data));
}

//
// Tests: StreamId - can_store()
//

TEST(stream_id, can_store_sufficient_buffer)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id{mac, 0x1234};

    auto const status = can_store(buf, stream_id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
}

TEST(stream_id, can_store_exact_buffer)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id{mac, 0x1234};

    auto const status = can_store(buf, stream_id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
}

TEST(stream_id, can_store_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id{mac, 0x1234};

    auto const status = can_store(buf, stream_id);

    EXPECT_FALSE(statusbar::is_success(status));
    EXPECT_EQ(status.error().value(), static_cast<int>(statusbar::BufferError::insufficient_space));
}

//
// Tests: StreamId - load()
//

TEST(stream_id, load_deserializes_correctly)
{
    std::array<uint8_t, 12> data{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x12, 0x34, 0x00, 0x00, 0x00, 0x00};
    std::span<uint8_t const> const buf{data};
    StreamId stream_id{};

    auto const status = load(buf, &stream_id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
    auto const mac = stream_id.get_system_address();
    EXPECT_EQ(mac.value[0], 0xAA);
    EXPECT_EQ(mac.value[1], 0xBB);
    EXPECT_EQ(mac.value[2], 0xCC);
    EXPECT_EQ(mac.value[3], 0xDD);
    EXPECT_EQ(mac.value[4], 0xEE);
    EXPECT_EQ(mac.value[5], 0xFF);
    EXPECT_EQ(stream_id.get_unique_id(), 0x1234);
}

TEST(stream_id, load_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t const> const buf{data};
    StreamId stream_id{};

    auto const status = load(buf, &stream_id);

    EXPECT_FALSE(statusbar::is_success(status));
    EXPECT_EQ(status.error().value(), static_cast<int>(statusbar::BufferError::insufficient_data));
}

//
// Tests: StreamId - store()
//

TEST(stream_id, store_serializes_correctly)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id{mac, 0x1234};

    auto const status = store(buf, stream_id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
    EXPECT_EQ(data[0], 0xAA);
    EXPECT_EQ(data[1], 0xBB);
    EXPECT_EQ(data[2], 0xCC);
    EXPECT_EQ(data[3], 0xDD);
    EXPECT_EQ(data[4], 0xEE);
    EXPECT_EQ(data[5], 0xFF);
    EXPECT_EQ(data[6], 0x12);
    EXPECT_EQ(data[7], 0x34);
}

TEST(stream_id, store_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id{mac, 0x1234};

    auto const status = store(buf, stream_id);

    EXPECT_FALSE(statusbar::is_success(status));
    EXPECT_EQ(status.error().value(), static_cast<int>(statusbar::BufferError::insufficient_space));
}

//
// Tests: StreamId - load_unchecked()
//

TEST(stream_id, load_unchecked_deserializes_without_validation)
{
    std::array<uint8_t, 12> data{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0xAB, 0xCD, 0xFF, 0xFF, 0xFF, 0xFF};
    std::span<uint8_t const> const buf{data};
    StreamId stream_id{};

    auto const bytes_consumed = load_unchecked(buf, &stream_id);

    EXPECT_EQ(bytes_consumed, 8);
    auto const mac = stream_id.get_system_address();
    EXPECT_EQ(mac.value[0], 0x11);
    EXPECT_EQ(mac.value[1], 0x22);
    EXPECT_EQ(mac.value[2], 0x33);
    EXPECT_EQ(mac.value[3], 0x44);
    EXPECT_EQ(mac.value[4], 0x55);
    EXPECT_EQ(mac.value[5], 0x66);
    EXPECT_EQ(stream_id.get_unique_id(), 0xABCD);
}

//
// Tests: StreamId - store_unchecked()
//

TEST(stream_id, store_unchecked_serializes_without_validation)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54};
    StreamId stream_id{mac, 0x9876};

    auto const bytes_written = store_unchecked(buf, stream_id);

    EXPECT_EQ(bytes_written, 8);
    EXPECT_EQ(data[0], 0xFE);
    EXPECT_EQ(data[1], 0xDC);
    EXPECT_EQ(data[2], 0xBA);
    EXPECT_EQ(data[3], 0x98);
    EXPECT_EQ(data[4], 0x76);
    EXPECT_EQ(data[5], 0x54);
    EXPECT_EQ(data[6], 0x98);
    EXPECT_EQ(data[7], 0x76);
}

//
// Tests: StreamId - Round-trip serialization/deserialization
//

TEST(stream_id, round_trip_preserves_values)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId original{mac, 0x1234};
    std::array<uint8_t, 8> buffer{};

    // Serialize
    auto const store_status = store(std::span<uint8_t>{buffer}, original);
    EXPECT_TRUE(statusbar::is_success(store_status));

    // Deserialize
    StreamId deserialized{};
    auto const load_status = load(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_TRUE(statusbar::is_success(load_status));

    // Verify
    EXPECT_EQ(deserialized.get_system_address().value[0], mac.value[0]);
    EXPECT_EQ(deserialized.get_system_address().value[5], mac.value[5]);
    EXPECT_EQ(deserialized.get_unique_id(), 0x1234);
}

TEST(stream_id, round_trip_unchecked_preserves_values)
{
    Eui48 mac{0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    StreamId original{mac, 0xABCD};
    std::array<uint8_t, 8> buffer{};

    // Serialize unchecked
    auto const bytes_written = store_unchecked(std::span<uint8_t>{buffer}, original);
    EXPECT_EQ(bytes_written, 8);

    // Deserialize unchecked
    StreamId deserialized{};
    auto const bytes_read = load_unchecked(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_EQ(bytes_read, 8);

    // Verify
    EXPECT_EQ(deserialized.get_system_address().value[0], mac.value[0]);
    EXPECT_EQ(deserialized.get_system_address().value[5], mac.value[5]);
    EXPECT_EQ(deserialized.get_unique_id(), 0xABCD);
}

TEST(stream_id, round_trip_with_builders_preserves_values)
{
    Eui48 mac{0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    StreamId original{mac, 0x7890};
    BufferSerializerBuilderWithStorage<8> serializer{};

    // Serialize using builder
    serializer.append(original);
    EXPECT_TRUE(serializer);
    EXPECT_EQ(serializer.get_span().size(), 8);

    // Deserialize using builder
    StreamId deserialized{};
    BufferDeserializerBuilder deserializer{serializer.get_span()};
    deserializer.parse(&deserialized);
    EXPECT_TRUE(deserializer);

    // Verify
    EXPECT_EQ(deserialized.get_system_address().value[0], mac.value[0]);
    EXPECT_EQ(deserialized.get_system_address().value[5], mac.value[5]);
    EXPECT_EQ(deserialized.get_unique_id(), 0x7890);
}

//
// Tests: StreamId - to_string()
//

TEST(stream_id, to_string_formats_correctly)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    StreamId stream_id{mac, 0x1234};
    std::string result = to_string(stream_id);

    EXPECT_EQ(result, "aa:bb:cc:dd:ee:ff:1234");
}

TEST(stream_id, to_string_with_zero_unique_id)
{
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    StreamId stream_id{mac, 0x0000};
    std::string result = to_string(stream_id);

    EXPECT_EQ(result, "00:11:22:33:44:55:0000");
}

//
// Tests: StreamId - from_string()
//

TEST(stream_id, from_string_colon_separated)
{
    auto result = stream_id_from_string("aa:bb:cc:dd:ee:ff:1234");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->get_system_address().value[0], 0xAA);
    EXPECT_EQ(result->get_system_address().value[5], 0xFF);
    EXPECT_EQ(result->get_unique_id(), 0x1234);
}

TEST(stream_id, from_string_with_whitespace)
{
    auto result = stream_id_from_string("  aa:bb:cc:dd:ee:ff:1234  ");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->get_unique_id(), 0x1234);
}

TEST(stream_id, from_string_invalid_too_short)
{
    auto result = stream_id_from_string("aa:bb");

    EXPECT_FALSE(result.has_value());
}

TEST(stream_id, from_string_invalid_hex)
{
    auto result = stream_id_from_string("aa:bb:cc:dd:ee:ff:ZZZZ");

    EXPECT_FALSE(result.has_value());
}

TEST(stream_id, from_string_round_trip)
{
    Eui48 mac{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
    StreamId original{mac, 0xCDEF};
    std::string str = to_string(original);
    auto parsed = stream_id_from_string(str);

    EXPECT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->get_system_address().value[0], mac.value[0]);
    EXPECT_EQ(parsed->get_system_address().value[5], mac.value[5]);
    EXPECT_EQ(parsed->get_unique_id(), 0xCDEF);
}

//
// Main test runner
//

TEST_MAIN(statusbar_tsn, tsn_stream_id_test)