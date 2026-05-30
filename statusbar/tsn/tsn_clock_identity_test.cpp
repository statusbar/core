// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/status/status.hpp"
#include "statusbar/test/test.hpp"
#include "statusbar/tsn/tsn_format.hpp"

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
// Tests: ClockIdentity - Construction and basic accessors
//

TEST(clock_identity_construct, default_constructor_initializes_to_zero)
{
    ClockIdentity id{};

    EXPECT_FALSE(id.is_set());
    EXPECT_EQ(id.to_uint64(), 0ULL);
}

TEST(clock_identity_construct, uint64_constructor_sets_bytes)
{
    ClockIdentity id{0x0102030405060708ULL};

    EXPECT_TRUE(id.is_set());
    auto const bytes = id.span();
    EXPECT_EQ(bytes[0], 0x01);
    EXPECT_EQ(bytes[1], 0x02);
    EXPECT_EQ(bytes[2], 0x03);
    EXPECT_EQ(bytes[3], 0x04);
    EXPECT_EQ(bytes[4], 0x05);
    EXPECT_EQ(bytes[5], 0x06);
    EXPECT_EQ(bytes[6], 0x07);
    EXPECT_EQ(bytes[7], 0x08);
}

TEST(clock_identity_construct, byte_constructor_sets_bytes)
{
    ClockIdentity id{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

    auto const bytes = id.span();
    EXPECT_EQ(bytes[0], 0xAA);
    EXPECT_EQ(bytes[1], 0xBB);
    EXPECT_EQ(bytes[2], 0xCC);
    EXPECT_EQ(bytes[3], 0xDD);
    EXPECT_EQ(bytes[4], 0xEE);
    EXPECT_EQ(bytes[5], 0xFF);
    EXPECT_EQ(bytes[6], 0x11);
    EXPECT_EQ(bytes[7], 0x22);
}

TEST(clock_identity_construct, eui64_constructor_copies_bytes)
{
    Eui64 eui64{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    ClockIdentity id{eui64};

    auto const bytes = id.span();
    EXPECT_EQ(bytes[0], 0x01);
    EXPECT_EQ(bytes[1], 0x02);
    EXPECT_EQ(bytes[2], 0x03);
    EXPECT_EQ(bytes[3], 0x04);
    EXPECT_EQ(bytes[4], 0x05);
    EXPECT_EQ(bytes[5], 0x06);
    EXPECT_EQ(bytes[6], 0x07);
    EXPECT_EQ(bytes[7], 0x08);
}

TEST(clock_identity_construct, size_returns_8)
{
    EXPECT_EQ(ClockIdentity::size(), 8);
}

//
// Tests: ClockIdentity - is_set()
//

TEST(clock_identity_is_set, returns_false_for_all_zeros)
{
    ClockIdentity id{};
    EXPECT_FALSE(id.is_set());
}

TEST(clock_identity_is_set, returns_true_if_any_byte_nonzero)
{
    ClockIdentity id{0, 0, 0, 0, 0, 0, 0, 1};
    EXPECT_TRUE(id.is_set());
}

TEST(clock_identity_is_set, returns_true_for_first_byte_nonzero)
{
    ClockIdentity id{1, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_TRUE(id.is_set());
}

TEST(clock_identity_is_set, returns_false_for_all_ones)
{
    ClockIdentity id{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    EXPECT_FALSE(id.is_set());
}

//
// Tests: ClockIdentity - uint64 conversion
//

TEST(clock_identity_uint64, to_uint64_returns_correct_value)
{
    ClockIdentity id{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

    EXPECT_EQ(id.to_uint64(), 0xAABBCCDDEEFF1122ULL);
}

TEST(clock_identity_uint64, from_uint64_sets_correct_bytes)
{
    ClockIdentity id{};
    id.from_uint64(0x1122334455667788ULL);

    auto const bytes = id.span();
    EXPECT_EQ(bytes[0], 0x11);
    EXPECT_EQ(bytes[1], 0x22);
    EXPECT_EQ(bytes[2], 0x33);
    EXPECT_EQ(bytes[3], 0x44);
    EXPECT_EQ(bytes[4], 0x55);
    EXPECT_EQ(bytes[5], 0x66);
    EXPECT_EQ(bytes[6], 0x77);
    EXPECT_EQ(bytes[7], 0x88);
}

TEST(clock_identity_uint64, round_trip_preserves_value)
{
    uint64_t const original = 0xFEDCBA9876543210ULL;
    ClockIdentity id{original};

    EXPECT_EQ(id.to_uint64(), original);
}

TEST(clock_identity_uint64, from_uint64_returns_self_reference)
{
    ClockIdentity id{};
    ClockIdentity& result = id.from_uint64(0x1234567890ABCDEFULL);

    EXPECT_EQ(&result, &id);
}

//
// Tests: ClockIdentity - EUI-64 conversion
//

TEST(clock_identity_eui64, to_eui64_returns_correct_value)
{
    ClockIdentity id{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    Eui64 eui64 = id.to_eui64();

    EXPECT_EQ(eui64.span()[0], 0x01);
    EXPECT_EQ(eui64.span()[1], 0x02);
    EXPECT_EQ(eui64.span()[2], 0x03);
    EXPECT_EQ(eui64.span()[3], 0x04);
    EXPECT_EQ(eui64.span()[4], 0x05);
    EXPECT_EQ(eui64.span()[5], 0x06);
    EXPECT_EQ(eui64.span()[6], 0x07);
    EXPECT_EQ(eui64.span()[7], 0x08);
}

TEST(clock_identity_eui64, from_eui64_member_sets_bytes)
{
    Eui64 eui64{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    ClockIdentity id{};
    id.from_eui64(eui64);

    auto const bytes = id.span();
    EXPECT_EQ(bytes[0], 0xAA);
    EXPECT_EQ(bytes[7], 0x22);
}

TEST(clock_identity_eui64, from_eui64_returns_self_reference)
{
    Eui64 eui64{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    ClockIdentity id{};
    ClockIdentity& result = id.from_eui64(eui64);

    EXPECT_EQ(&result, &id);
}

//
// Tests: ClockIdentity - from_eui48 (modified EUI-64)
//

TEST(clock_identity_eui48, from_eui48_inserts_fffe)
{
    // EUI-48: AA:BB:CC:DD:EE:FF
    // Modified EUI-64: AA:BB:CC:FF:FE:DD:EE:FF
    Eui48 eui48{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    ClockIdentity id = ClockIdentity::from_eui48(eui48);

    auto const bytes = id.span();
    EXPECT_EQ(bytes[0], 0xAA);
    EXPECT_EQ(bytes[1], 0xBB);
    EXPECT_EQ(bytes[2], 0xCC);
    EXPECT_EQ(bytes[3], 0xFF);  // Inserted
    EXPECT_EQ(bytes[4], 0xFE);  // Inserted
    EXPECT_EQ(bytes[5], 0xDD);
    EXPECT_EQ(bytes[6], 0xEE);
    EXPECT_EQ(bytes[7], 0xFF);
}

TEST(clock_identity_eui48, from_eui48_with_zeros)
{
    Eui48 eui48{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    ClockIdentity id = ClockIdentity::from_eui48(eui48);

    auto const bytes = id.span();
    EXPECT_EQ(bytes[0], 0x00);
    EXPECT_EQ(bytes[1], 0x11);
    EXPECT_EQ(bytes[2], 0x22);
    EXPECT_EQ(bytes[3], 0xFF);
    EXPECT_EQ(bytes[4], 0xFE);
    EXPECT_EQ(bytes[5], 0x33);
    EXPECT_EQ(bytes[6], 0x44);
    EXPECT_EQ(bytes[7], 0x55);
}

//
// Tests: ClockIdentity - Comparison operators
//

TEST(clock_identity_compare, equality_operator)
{
    ClockIdentity id1{0x0102030405060708ULL};
    ClockIdentity id2{0x0102030405060708ULL};

    EXPECT_TRUE(id1 == id2);
}

TEST(clock_identity_compare, inequality_operator)
{
    ClockIdentity id1{0x0102030405060708ULL};
    ClockIdentity id2{0x0102030405060709ULL};

    EXPECT_TRUE(id1 != id2);
}

TEST(clock_identity_compare, less_than_operator)
{
    ClockIdentity id1{0x0102030405060708ULL};
    ClockIdentity id2{0x0102030405060709ULL};

    EXPECT_TRUE(id1 < id2);
}

TEST(clock_identity_compare, greater_than_operator)
{
    ClockIdentity id1{0x0102030405060709ULL};
    ClockIdentity id2{0x0102030405060708ULL};

    EXPECT_TRUE(id1 > id2);
}

TEST(clock_identity_compare, default_constructed_equal)
{
    ClockIdentity id1{};
    ClockIdentity id2{};

    EXPECT_TRUE(id1 == id2);
}

//
// Tests: ClockIdentity - wire_size()
//

TEST(clock_identity_wire, wire_size_returns_8)
{
    ClockIdentity id{0x0102030405060708ULL};
    EXPECT_EQ(wire_size(id), 8);
}

//
// Tests: ClockIdentity - can_load()
//

TEST(clock_identity_load, can_load_sufficient_buffer)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t const> const buf{data};
    ClockIdentity id{};

    auto const status = can_load(buf, &id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
}

TEST(clock_identity_load, can_load_exact_buffer)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t const> const buf{data};
    ClockIdentity id{};

    auto const status = can_load(buf, &id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
}

TEST(clock_identity_load, can_load_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t const> const buf{data};
    ClockIdentity id{};

    auto const status = can_load(buf, &id);

    EXPECT_FALSE(statusbar::is_success(status));
    EXPECT_EQ(status.error().value(), static_cast<int>(statusbar::BufferError::insufficient_data));
}

//
// Tests: ClockIdentity - can_store()
//

TEST(clock_identity_store, can_store_sufficient_buffer)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t> const buf{data};
    ClockIdentity id{0x0102030405060708ULL};

    auto const status = can_store(buf, id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
}

TEST(clock_identity_store, can_store_exact_buffer)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t> const buf{data};
    ClockIdentity id{0x0102030405060708ULL};

    auto const status = can_store(buf, id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
}

TEST(clock_identity_store, can_store_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t> const buf{data};
    ClockIdentity id{0x0102030405060708ULL};

    auto const status = can_store(buf, id);

    EXPECT_FALSE(statusbar::is_success(status));
    EXPECT_EQ(status.error().value(), static_cast<int>(statusbar::BufferError::insufficient_space));
}

//
// Tests: ClockIdentity - load()
//

TEST(clock_identity_load, load_deserializes_correctly)
{
    std::array<uint8_t, 12> data{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x00, 0x00, 0x00, 0x00};
    std::span<uint8_t const> const buf{data};
    ClockIdentity id{};

    auto const status = load(buf, &id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
    EXPECT_EQ(id.to_uint64(), 0xAABBCCDDEEFF1122ULL);
}

TEST(clock_identity_load, load_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t const> const buf{data};
    ClockIdentity id{};

    auto const status = load(buf, &id);

    EXPECT_FALSE(statusbar::is_success(status));
    EXPECT_EQ(status.error().value(), static_cast<int>(statusbar::BufferError::insufficient_data));
}

//
// Tests: ClockIdentity - store()
//

TEST(clock_identity_store, store_serializes_correctly)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t> const buf{data};
    ClockIdentity id{0xAABBCCDDEEFF1122ULL};

    auto const status = store(buf, id);

    EXPECT_TRUE(statusbar::is_success(status));
    EXPECT_EQ(status.value(), 8);
    EXPECT_EQ(data[0], 0xAA);
    EXPECT_EQ(data[1], 0xBB);
    EXPECT_EQ(data[2], 0xCC);
    EXPECT_EQ(data[3], 0xDD);
    EXPECT_EQ(data[4], 0xEE);
    EXPECT_EQ(data[5], 0xFF);
    EXPECT_EQ(data[6], 0x11);
    EXPECT_EQ(data[7], 0x22);
}

TEST(clock_identity_store, store_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t> const buf{data};
    ClockIdentity id{0xAABBCCDDEEFF1122ULL};

    auto const status = store(buf, id);

    EXPECT_FALSE(statusbar::is_success(status));
    EXPECT_EQ(status.error().value(), static_cast<int>(statusbar::BufferError::insufficient_space));
}

//
// Tests: ClockIdentity - load_unchecked() / store_unchecked()
//

TEST(clock_identity_unchecked, load_unchecked_deserializes)
{
    std::array<uint8_t, 8> data{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    std::span<uint8_t const> const buf{data};
    ClockIdentity id{};

    auto const bytes_consumed = load_unchecked(buf, &id);

    EXPECT_EQ(bytes_consumed, 8);
    EXPECT_EQ(id.to_uint64(), 0x1122334455667788ULL);
}

TEST(clock_identity_unchecked, store_unchecked_serializes)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t> const buf{data};
    ClockIdentity id{0xFEDCBA9876543210ULL};

    auto const bytes_written = store_unchecked(buf, id);

    EXPECT_EQ(bytes_written, 8);
    EXPECT_EQ(data[0], 0xFE);
    EXPECT_EQ(data[1], 0xDC);
    EXPECT_EQ(data[7], 0x10);
}

//
// Tests: ClockIdentity - Round-trip serialization
//

TEST(clock_identity_round_trip, preserves_values)
{
    ClockIdentity original{0xAABBCCDDEEFF1122ULL};
    std::array<uint8_t, 8> buffer{};

    auto const store_status = store(std::span<uint8_t>{buffer}, original);
    EXPECT_TRUE(statusbar::is_success(store_status));

    ClockIdentity deserialized{};
    auto const load_status = load(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_TRUE(statusbar::is_success(load_status));

    EXPECT_EQ(deserialized.to_uint64(), original.to_uint64());
}

TEST(clock_identity_round_trip, with_builders)
{
    ClockIdentity original{0x0102030405060708ULL};
    BufferSerializerBuilderWithStorage<8> serializer{};

    serializer.append(original);
    EXPECT_TRUE(serializer);
    EXPECT_EQ(serializer.get_span().size(), 8);

    ClockIdentity deserialized{};
    BufferDeserializerBuilder deserializer{serializer.get_span()};
    deserializer.parse(&deserialized);
    EXPECT_TRUE(deserializer);

    EXPECT_EQ(deserialized.to_uint64(), original.to_uint64());
}

//
// Tests: ClockIdentity - to_string()
//

TEST(clock_identity_string, to_string_formats_correctly)
{
    ClockIdentity id{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    std::string result = to_string(id);

    EXPECT_EQ(result, "01:23:45:67:89:ab:cd:ef");
}

TEST(clock_identity_string, to_string_with_zeros)
{
    ClockIdentity id{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::string result = to_string(id);

    EXPECT_EQ(result, "00:00:00:00:00:00:00:00");
}

//
// Tests: ClockIdentity - from_string()
//

TEST(clock_identity_string, from_string_colon_separated)
{
    auto result = clock_identity_from_string("01:23:45:67:89:ab:cd:ef");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->to_uint64(), 0x0123456789ABCDEFULL);
}

TEST(clock_identity_string, from_string_dash_separated)
{
    auto result = clock_identity_from_string("01-23-45-67-89-AB-CD-EF");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->to_uint64(), 0x0123456789ABCDEFULL);
}

TEST(clock_identity_string, from_string_no_separator)
{
    auto result = clock_identity_from_string("0123456789ABCDEF");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->to_uint64(), 0x0123456789ABCDEFULL);
}

TEST(clock_identity_string, from_string_with_whitespace)
{
    auto result = clock_identity_from_string("  01:23:45:67:89:ab:cd:ef  ");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->to_uint64(), 0x0123456789ABCDEFULL);
}

TEST(clock_identity_string, from_string_invalid_length)
{
    auto result = clock_identity_from_string("01:23:45");

    EXPECT_FALSE(result.has_value());
}

TEST(clock_identity_string, from_string_invalid_chars)
{
    auto result = clock_identity_from_string("01:23:45:67:89:ab:cd:GH");

    EXPECT_FALSE(result.has_value());
}

TEST(clock_identity_string, from_string_mixed_separator)
{
    // Mixed separators should fail
    auto result = clock_identity_from_string("01:23-45:67:89:ab:cd:ef");

    EXPECT_FALSE(result.has_value());
}

TEST(clock_identity_string, round_trip_string_conversion)
{
    ClockIdentity original{0xAABBCCDDEEFF1122ULL};
    std::string str = to_string(original);
    auto parsed = clock_identity_from_string(str);

    EXPECT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->to_uint64(), original.to_uint64());
}

//
// Tests: ClockIdentity - format_to()
//

TEST(clock_identity_format, format_to_output_iterator)
{
    ClockIdentity id{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    std::string result;
    format_to(std::back_inserter(result), id);

    EXPECT_EQ(result, "aa:bb:cc:dd:ee:ff:11:22");
}

//
// Main test runner
//

TEST_MAIN(statusbar_tsn, tsn_clock_identity_test)