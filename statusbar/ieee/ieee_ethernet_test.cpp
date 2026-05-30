// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee_format.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <system_error>

using namespace statusbar;
using namespace statusbar::ieee;

// Bring protocol functions into scope for ADL
using statusbar::protocol::can_load;
using statusbar::protocol::can_store;
using statusbar::protocol::load;
using statusbar::protocol::store;
using statusbar::protocol::wire_size;

//
// Tests: Eui48 - wire_size()
//

TEST(eui48, wire_size_returns_6)
{
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    auto const size = wire_size(mac);

    EXPECT_EQ(size, 6);
}

//
// Tests: Eui48 - can_load()
//

TEST(eui48, can_load_sufficient_buffer)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t const> const buf{data};
    Eui48 mac{};

    auto const status = can_load(buf, &mac);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 6);
}

TEST(eui48, can_load_exact_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t const> const buf{data};
    Eui48 mac{};

    auto const status = can_load(buf, &mac);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 6);
}

TEST(eui48, can_load_insufficient_buffer)
{
    std::array<uint8_t, 4> data{};
    std::span<uint8_t const> const buf{data};
    Eui48 mac{};

    auto const status = can_load(buf, &mac);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

//
// Tests: Eui48 - can_store()
//

TEST(eui48, can_store_sufficient_buffer)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    auto const status = can_store(buf, mac);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 6);
}

TEST(eui48, can_store_exact_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    auto const status = can_store(buf, mac);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 6);
}

TEST(eui48, can_store_insufficient_buffer)
{
    std::array<uint8_t, 4> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    auto const status = can_store(buf, mac);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

//
// Tests: Eui48 - load()
//

TEST(eui48, load_deserializes_correctly)
{
    std::array<uint8_t, 10> data{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x00, 0x00, 0x00};
    std::span<uint8_t const> const buf{data};
    Eui48 mac{};

    auto const status = load(buf, &mac);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 6);
    EXPECT_EQ(mac.value[0], 0xAA);
    EXPECT_EQ(mac.value[1], 0xBB);
    EXPECT_EQ(mac.value[2], 0xCC);
    EXPECT_EQ(mac.value[3], 0xDD);
    EXPECT_EQ(mac.value[4], 0xEE);
    EXPECT_EQ(mac.value[5], 0xFF);
}

TEST(eui48, load_insufficient_buffer)
{
    std::array<uint8_t, 4> data{};
    std::span<uint8_t const> const buf{data};
    Eui48 mac{};

    auto const status = load(buf, &mac);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

//
// Tests: Eui48 - store()
//

TEST(eui48, store_serializes_correctly)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    auto const status = store(buf, mac);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 6);
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x11);
    EXPECT_EQ(data[2], 0x22);
    EXPECT_EQ(data[3], 0x33);
    EXPECT_EQ(data[4], 0x44);
    EXPECT_EQ(data[5], 0x55);
}

TEST(eui48, store_insufficient_buffer)
{
    std::array<uint8_t, 4> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    auto const status = store(buf, mac);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

//
// Tests: Eui48 - load_unchecked()
//

TEST(eui48, load_unchecked_deserializes_without_validation)
{
    std::array<uint8_t, 10> data{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0xFF, 0xFF};
    std::span<uint8_t const> const buf{data};
    Eui48 mac{};

    auto const bytes_consumed = load_unchecked(buf, &mac);

    EXPECT_EQ(bytes_consumed, 6);
    EXPECT_EQ(mac.value[0], 0x12);
    EXPECT_EQ(mac.value[1], 0x34);
    EXPECT_EQ(mac.value[2], 0x56);
    EXPECT_EQ(mac.value[3], 0x78);
    EXPECT_EQ(mac.value[4], 0x9A);
    EXPECT_EQ(mac.value[5], 0xBC);
}

//
// Tests: Eui48 - store_unchecked()
//

TEST(eui48, store_unchecked_serializes_without_validation)
{
    std::array<uint8_t, 10> data{};
    std::span<uint8_t> const buf{data};
    Eui48 mac{0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54};

    auto const bytes_written = store_unchecked(buf, mac);

    EXPECT_EQ(bytes_written, 6);
    EXPECT_EQ(data[0], 0xFE);
    EXPECT_EQ(data[1], 0xDC);
    EXPECT_EQ(data[2], 0xBA);
    EXPECT_EQ(data[3], 0x98);
    EXPECT_EQ(data[4], 0x76);
    EXPECT_EQ(data[5], 0x54);
}

//
// Tests: Eui48 - Round-trip serialization/deserialization
//

TEST(eui48, round_trip_preserves_values)
{
    Eui48 original{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    std::array<uint8_t, 6> buffer{};

    // Serialize
    auto const store_status = store(std::span<uint8_t>{buffer}, original);
    EXPECT_TRUE(store_status);

    // Deserialize
    Eui48 deserialized{};
    auto const load_status = load(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_TRUE(load_status);

    // Verify
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(deserialized.value[i], original.value[i]);
    }
}

TEST(eui48, round_trip_unchecked_preserves_values)
{
    Eui48 original{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    std::array<uint8_t, 6> buffer{};

    // Serialize unchecked
    auto const bytes_written = store_unchecked(std::span<uint8_t>{buffer}, original);
    EXPECT_EQ(bytes_written, 6);

    // Deserialize unchecked
    Eui48 deserialized{};
    auto const bytes_read = load_unchecked(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_EQ(bytes_read, 6);

    // Verify
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(deserialized.value[i], original.value[i]);
    }
}

TEST(eui48, round_trip_with_builders_preserves_values)
{
    Eui48 original{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    BufferSerializerBuilderWithStorage<6> serializer{};

    // Serialize using builder
    serializer.append(original);
    EXPECT_TRUE(serializer);
    EXPECT_EQ(serializer.get_span().size(), 6);

    // Deserialize using builder
    Eui48 deserialized{};
    BufferDeserializerBuilder deserializer{serializer.get_span()};
    deserializer.parse(&deserialized);
    EXPECT_TRUE(deserializer);

    // Verify
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(deserialized.value[i], original.value[i]);
    }
}

//
// Tests: Eui48 - to_uint64() / from_uint64()
//

TEST(eui48, to_uint64_returns_correct_value)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    auto const value = mac.to_uint64();

    EXPECT_EQ(value, 0x0000AABBCCDDEEFFULL);
}

TEST(eui48, to_uint64_zero_mac)
{
    Eui48 mac{0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    auto const value = mac.to_uint64();

    EXPECT_EQ(value, 0x0000000000000000ULL);
}

TEST(eui48, from_uint64_sets_correct_bytes)
{
    Eui48 mac{};

    mac.from_uint64(0x0000112233445566ULL);

    EXPECT_EQ(mac.value[0], 0x11);
    EXPECT_EQ(mac.value[1], 0x22);
    EXPECT_EQ(mac.value[2], 0x33);
    EXPECT_EQ(mac.value[3], 0x44);
    EXPECT_EQ(mac.value[4], 0x55);
    EXPECT_EQ(mac.value[5], 0x66);
}

TEST(eui48, from_uint64_ignores_upper_16_bits)
{
    Eui48 mac{};

    mac.from_uint64(0xFFFFAABBCCDDEEFFULL);

    // Upper 16 bits should be ignored
    EXPECT_EQ(mac.value[0], 0xAA);
    EXPECT_EQ(mac.value[1], 0xBB);
    EXPECT_EQ(mac.value[2], 0xCC);
    EXPECT_EQ(mac.value[3], 0xDD);
    EXPECT_EQ(mac.value[4], 0xEE);
    EXPECT_EQ(mac.value[5], 0xFF);
}

TEST(eui48, uint64_round_trip_preserves_value)
{
    Eui48 original{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};

    auto const value = original.to_uint64();
    Eui48 restored{};
    restored.from_uint64(value);

    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(restored.value[i], original.value[i]);
    }
}

// Compile-time verification that Eui48::is_set() is constexpr (issue f8bd704)
static_assert(!Eui48(0, 0, 0, 0, 0, 0).is_set(), "all-zeros should not be set");
static_assert(!Eui48(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF).is_set(), "broadcast should not be set");
static_assert(Eui48(0x00, 0x11, 0x22, 0x33, 0x44, 0x55).is_set(), "valid address should be set");

//
// Tests: Eui48 - is_set()
//

TEST(eui48, is_set_returns_false_for_zero_address)
{
    Eui48 mac{0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(mac.is_set());
}

TEST(eui48, is_set_returns_false_for_broadcast_address)
{
    Eui48 mac{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    EXPECT_FALSE(mac.is_set());
}

TEST(eui48, is_set_returns_true_for_valid_address)
{
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    EXPECT_TRUE(mac.is_set());
}

TEST(eui48, is_set_returns_true_for_partial_address)
{
    Eui48 mac{0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    EXPECT_TRUE(mac.is_set());
}

//
// Tests: Eui48 - is_locally_administered()
//

TEST(eui48, is_locally_administered_returns_false_for_global)
{
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};  // Bit 1 clear

    EXPECT_FALSE(mac.is_locally_administered());
}

TEST(eui48, is_locally_administered_returns_true_for_local)
{
    Eui48 mac{0x02, 0x11, 0x22, 0x33, 0x44, 0x55};  // Bit 1 set

    EXPECT_TRUE(mac.is_locally_administered());
}

TEST(eui48, is_locally_administered_only_checks_bit_1)
{
    Eui48 mac{0xFD, 0x11, 0x22, 0x33, 0x44, 0x55};  // Many bits set, bit 1 clear

    EXPECT_FALSE(mac.is_locally_administered());
}

//
// Tests: Eui48 - is_multicast()
//

TEST(eui48, is_multicast_returns_false_for_unicast)
{
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};  // Bit 0 clear

    EXPECT_FALSE(mac.is_multicast());
}

TEST(eui48, is_multicast_returns_true_for_multicast)
{
    Eui48 mac{0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};  // Bit 0 set (IPv4 multicast)

    EXPECT_TRUE(mac.is_multicast());
}

TEST(eui48, is_multicast_returns_true_for_broadcast)
{
    Eui48 mac{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Broadcast (bit 0 set)

    EXPECT_TRUE(mac.is_multicast());
}

//
// Tests: Eui48 - to_modified_eui64()
//

TEST(eui48, to_modified_eui64_inserts_fffe)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    auto const eui64 = mac.to_modified_eui64();

    EXPECT_EQ(eui64.span()[0], 0xAA);
    EXPECT_EQ(eui64.span()[1], 0xBB);
    EXPECT_EQ(eui64.span()[2], 0xCC);
    EXPECT_EQ(eui64.span()[3], 0xFF);  // Inserted
    EXPECT_EQ(eui64.span()[4], 0xFE);  // Inserted
    EXPECT_EQ(eui64.span()[5], 0xDD);
    EXPECT_EQ(eui64.span()[6], 0xEE);
    EXPECT_EQ(eui64.span()[7], 0xFF);
}

TEST(eui48, to_modified_eui64_preserves_mac_bytes)
{
    Eui48 mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    auto const eui64 = mac.to_modified_eui64();

    // First 3 bytes
    EXPECT_EQ(eui64.span()[0], mac.value[0]);
    EXPECT_EQ(eui64.span()[1], mac.value[1]);
    EXPECT_EQ(eui64.span()[2], mac.value[2]);
    // Last 3 bytes
    EXPECT_EQ(eui64.span()[5], mac.value[3]);
    EXPECT_EQ(eui64.span()[6], mac.value[4]);
    EXPECT_EQ(eui64.span()[7], mac.value[5]);
}

//
// Tests: Eui48 - span()
//

TEST(eui48, span_returns_correct_size)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    auto const span = mac.span();

    EXPECT_EQ(span.size(), 6);
}

TEST(eui48, span_provides_access_to_bytes)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    auto const span = mac.span();

    EXPECT_EQ(span[0], 0xAA);
    EXPECT_EQ(span[5], 0xFF);
}

TEST(eui48, mutable_span_allows_modification)
{
    Eui48 mac{0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    auto span = mac.span();
    span[0] = 0xAA;
    span[5] = 0xFF;

    EXPECT_EQ(mac.value[0], 0xAA);
    EXPECT_EQ(mac.value[5], 0xFF);
}

//
// Tests: Eui64 - wire_size()
//

TEST(eui64, wire_size_returns_8)
{
    Eui64 addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    auto const size = wire_size(addr);

    EXPECT_EQ(size, 8);
}

//
// Tests: Eui64 - can_load()
//

TEST(eui64, can_load_sufficient_buffer)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t const> const buf{data};
    Eui64 addr{};

    auto const status = can_load(buf, &addr);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
}

TEST(eui64, can_load_exact_buffer)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t const> const buf{data};
    Eui64 addr{};

    auto const status = can_load(buf, &addr);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
}

TEST(eui64, can_load_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t const> const buf{data};
    Eui64 addr{};

    auto const status = can_load(buf, &addr);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

//
// Tests: Eui64 - can_store()
//

TEST(eui64, can_store_sufficient_buffer)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t> const buf{data};
    Eui64 addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    auto const status = can_store(buf, addr);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
}

TEST(eui64, can_store_exact_buffer)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t> const buf{data};
    Eui64 addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    auto const status = can_store(buf, addr);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
}

TEST(eui64, can_store_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t> const buf{data};
    Eui64 addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    auto const status = can_store(buf, addr);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

//
// Tests: Eui64 - load()
//

TEST(eui64, load_deserializes_correctly)
{
    std::array<uint8_t, 12> data{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x00, 0x00, 0x00, 0x00};
    std::span<uint8_t const> const buf{data};
    Eui64 addr{};

    auto const status = load(buf, &addr);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
    EXPECT_EQ(addr.span()[0], 0xAA);
    EXPECT_EQ(addr.span()[1], 0xBB);
    EXPECT_EQ(addr.span()[2], 0xCC);
    EXPECT_EQ(addr.span()[3], 0xDD);
    EXPECT_EQ(addr.span()[4], 0xEE);
    EXPECT_EQ(addr.span()[5], 0xFF);
    EXPECT_EQ(addr.span()[6], 0x11);
    EXPECT_EQ(addr.span()[7], 0x22);
}

TEST(eui64, load_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t const> const buf{data};
    Eui64 addr{};

    auto const status = load(buf, &addr);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

//
// Tests: Eui64 - store()
//

TEST(eui64, store_serializes_correctly)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t> const buf{data};
    Eui64 addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    auto const status = store(buf, addr);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 8);
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x11);
    EXPECT_EQ(data[2], 0x22);
    EXPECT_EQ(data[3], 0x33);
    EXPECT_EQ(data[4], 0x44);
    EXPECT_EQ(data[5], 0x55);
    EXPECT_EQ(data[6], 0x66);
    EXPECT_EQ(data[7], 0x77);
}

TEST(eui64, store_insufficient_buffer)
{
    std::array<uint8_t, 6> data{};
    std::span<uint8_t> const buf{data};
    Eui64 addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    auto const status = store(buf, addr);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

//
// Tests: Eui64 - load_unchecked()
//

TEST(eui64, load_unchecked_deserializes_without_validation)
{
    std::array<uint8_t, 12> data{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF};
    std::span<uint8_t const> const buf{data};
    Eui64 addr{};

    auto const bytes_consumed = load_unchecked(buf, &addr);

    EXPECT_EQ(bytes_consumed, 8);
    EXPECT_EQ(addr.span()[0], 0x12);
    EXPECT_EQ(addr.span()[1], 0x34);
    EXPECT_EQ(addr.span()[2], 0x56);
    EXPECT_EQ(addr.span()[3], 0x78);
    EXPECT_EQ(addr.span()[4], 0x9A);
    EXPECT_EQ(addr.span()[5], 0xBC);
    EXPECT_EQ(addr.span()[6], 0xDE);
    EXPECT_EQ(addr.span()[7], 0xF0);
}

//
// Tests: Eui64 - store_unchecked()
//

TEST(eui64, store_unchecked_serializes_without_validation)
{
    std::array<uint8_t, 12> data{};
    std::span<uint8_t> const buf{data};
    Eui64 addr{0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};

    auto const bytes_written = store_unchecked(buf, addr);

    EXPECT_EQ(bytes_written, 8);
    EXPECT_EQ(data[0], 0xFE);
    EXPECT_EQ(data[1], 0xDC);
    EXPECT_EQ(data[2], 0xBA);
    EXPECT_EQ(data[3], 0x98);
    EXPECT_EQ(data[4], 0x76);
    EXPECT_EQ(data[5], 0x54);
    EXPECT_EQ(data[6], 0x32);
    EXPECT_EQ(data[7], 0x10);
}

//
// Tests: Eui64 - Round-trip serialization/deserialization
//

TEST(eui64, round_trip_preserves_values)
{
    Eui64 original{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    std::array<uint8_t, 8> buffer{};

    // Serialize
    auto const store_status = store(std::span<uint8_t>{buffer}, original);
    EXPECT_TRUE(store_status);

    // Deserialize
    Eui64 deserialized{};
    auto const load_status = load(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_TRUE(load_status);

    // Verify
    for (size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(deserialized.span()[i], original.span()[i]);
    }
}

TEST(eui64, round_trip_unchecked_preserves_values)
{
    Eui64 original{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    std::array<uint8_t, 8> buffer{};

    // Serialize unchecked
    auto const bytes_written = store_unchecked(std::span<uint8_t>{buffer}, original);
    EXPECT_EQ(bytes_written, 8);

    // Deserialize unchecked
    Eui64 deserialized{};
    auto const bytes_read = load_unchecked(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_EQ(bytes_read, 8);

    // Verify
    for (size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(deserialized.span()[i], original.span()[i]);
    }
}

//
// Tests: Eui64 - to_uint64() / from_uint64()
//

TEST(eui64, to_uint64_returns_correct_value)
{
    Eui64 addr{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

    auto const value = addr.to_uint64();

    EXPECT_EQ(value, 0xAABBCCDDEEFF1122ULL);
}

TEST(eui64, to_uint64_zero_address)
{
    Eui64 addr{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    auto const value = addr.to_uint64();

    EXPECT_EQ(value, 0x0000000000000000ULL);
}

TEST(eui64, from_uint64_sets_correct_bytes)
{
    Eui64 addr{};

    addr.from_uint64(0x1122334455667788ULL);

    EXPECT_EQ(addr.span()[0], 0x11);
    EXPECT_EQ(addr.span()[1], 0x22);
    EXPECT_EQ(addr.span()[2], 0x33);
    EXPECT_EQ(addr.span()[3], 0x44);
    EXPECT_EQ(addr.span()[4], 0x55);
    EXPECT_EQ(addr.span()[5], 0x66);
    EXPECT_EQ(addr.span()[6], 0x77);
    EXPECT_EQ(addr.span()[7], 0x88);
}

TEST(eui64, uint64_round_trip_preserves_value)
{
    Eui64 original{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

    auto const value = original.to_uint64();
    Eui64 restored{};
    restored.from_uint64(value);

    for (size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(restored.span()[i], original.span()[i]);
    }
}

//
// Tests: Eui64 - is_set()
//

TEST(eui64, is_set_returns_false_for_zero_address)
{
    Eui64 addr{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(addr.is_set());
}

TEST(eui64, is_set_returns_false_for_broadcast_address)
{
    Eui64 addr{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    EXPECT_FALSE(addr.is_set());
}

TEST(eui64, is_set_returns_true_for_valid_address)
{
    Eui64 addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    EXPECT_TRUE(addr.is_set());
}

TEST(eui64, is_set_returns_true_for_partial_address)
{
    Eui64 addr{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    EXPECT_TRUE(addr.is_set());
}

//
// Tests: Eui64 - is_locally_administered()
//

TEST(eui64, is_locally_administered_returns_false_for_global)
{
    Eui64 addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};  // Bit 1 clear

    EXPECT_FALSE(addr.is_locally_administered());
}

TEST(eui64, is_locally_administered_returns_true_for_local)
{
    Eui64 addr{0x02, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};  // Bit 1 set

    EXPECT_TRUE(addr.is_locally_administered());
}

TEST(eui64, is_locally_administered_only_checks_bit_1)
{
    Eui64 addr{0xFD, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};  // Many bits set, bit 1 clear

    EXPECT_FALSE(addr.is_locally_administered());
}

//
// Tests: Eui64 - is_multicast()
//

TEST(eui64, is_multicast_returns_false_for_unicast)
{
    Eui64 addr{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};  // Bit 0 clear

    EXPECT_FALSE(addr.is_multicast());
}

TEST(eui64, is_multicast_returns_true_for_multicast)
{
    Eui64 addr{0x01, 0x00, 0x5E, 0x00, 0x00, 0x00, 0x00, 0x01};  // Bit 0 set

    EXPECT_TRUE(addr.is_multicast());
}

TEST(eui64, is_multicast_returns_true_for_broadcast)
{
    Eui64 addr{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Broadcast (bit 0 set)

    EXPECT_TRUE(addr.is_multicast());
}

//
// Tests: Eui64 - span()
//

TEST(eui64, span_returns_correct_size)
{
    Eui64 addr{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

    auto const span = addr.span();

    EXPECT_EQ(span.size(), 8);
}

TEST(eui64, span_provides_access_to_bytes)
{
    Eui64 addr{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

    auto const span = addr.span();

    EXPECT_EQ(span[0], 0xAA);
    EXPECT_EQ(span[7], 0x22);
}

//
// Tests: VlanTag - wire_size()
//

TEST(vlan_tag, wire_size_returns_4)
{
    VlanTag vlan{0x042, false, 0};  // VID=66, DEI=0, PCP=0

    auto const size = wire_size(vlan);

    EXPECT_EQ(size, 4);
}

//
// Tests: VlanTag - can_load()
//

TEST(vlan_tag, can_load_sufficient_buffer)
{
    // Test with VLAN tag present (tpid = 0x8100)
    std::array<uint8_t, 8> data{0x81, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x00};
    std::span<uint8_t const> const buf{data};
    VlanTag vlan{};

    auto const status = can_load(buf, &vlan);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 4);
}

TEST(vlan_tag, can_load_no_tag_present)
{
    // Test with no VLAN tag present (tpid != 0x8100)
    std::array<uint8_t, 8> data{0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::span<uint8_t const> const buf{data};
    VlanTag vlan{};

    auto const status = can_load(buf, &vlan);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 0);
}

TEST(vlan_tag, can_load_exact_buffer)
{
    // Test with exact 4 bytes for VLAN tag
    std::array<uint8_t, 4> data{0x81, 0x00, 0x12, 0x34};
    std::span<uint8_t const> const buf{data};
    VlanTag vlan{};

    auto const status = can_load(buf, &vlan);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 4);
}

TEST(vlan_tag, can_load_insufficient_buffer)
{
    // Test with 2 bytes starting with 0x8100 but not enough for full tag
    std::array<uint8_t, 2> data{0x81, 0x00};
    std::span<uint8_t const> const buf{data};
    VlanTag vlan{};

    auto const status = can_load(buf, &vlan);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

//
// Tests: VlanTag - can_store()
//

TEST(vlan_tag, can_store_sufficient_buffer)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t> const buf{data};
    VlanTag vlan{0x042, false, 0};  // VID=66, DEI=0, PCP=0

    auto const status = can_store(buf, vlan);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 4);
}

TEST(vlan_tag, can_store_exact_buffer)
{
    std::array<uint8_t, 4> data{};
    std::span<uint8_t> const buf{data};
    VlanTag vlan{0x042, false, 0};  // VID=66, DEI=0, PCP=0

    auto const status = can_store(buf, vlan);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 4);
}

TEST(vlan_tag, can_store_insufficient_buffer)
{
    std::array<uint8_t, 2> data{};
    std::span<uint8_t> const buf{data};
    VlanTag vlan{0x042, false, 0};  // VID=66, DEI=0, PCP=0

    auto const status = can_store(buf, vlan);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

//
// Tests: VlanTag - load()
//

TEST(vlan_tag, load_deserializes_correctly)
{
    // Test with VLAN tag present (tpid = 0x8100)
    std::array<uint8_t, 8> data{0x81, 0x00, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00};
    std::span<uint8_t const> const buf{data};
    VlanTag vlan{};

    auto const status = load(buf, &vlan);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 4);
    // doublet_t uses network byte order (big-endian)
    EXPECT_EQ(vlan.tpid, VlanTag::ETHERTYPE);
    EXPECT_EQ(vlan.tci, 0x5678);
}

TEST(vlan_tag, load_deserializes_no_tag)
{
    // Test with no VLAN tag present (tpid != 0x8100)
    std::array<uint8_t, 8> data{0x08, 0x00, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00};
    std::span<uint8_t const> const buf{data};
    VlanTag vlan{};

    auto const status = load(buf, &vlan);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 0);
    // Should be reset to 0
    EXPECT_EQ(vlan.tpid, 0);
    EXPECT_EQ(vlan.tci, 0);
}

TEST(vlan_tag, load_insufficient_buffer)
{
    // Test with 2 bytes starting with 0x8100 but not enough data
    std::array<uint8_t, 2> data{0x81, 0x00};
    std::span<uint8_t const> const buf{data};
    VlanTag vlan{};

    auto const status = load(buf, &vlan);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

//
// Tests: VlanTag - store()
//

TEST(vlan_tag, store_serializes_correctly)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t> const buf{data};
    VlanTag vlan{0x042, false, 0};  // VID=66, DEI=0, PCP=0

    auto const status = store(buf, vlan);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 4);
    // doublet_t uses network byte order (big-endian)
    EXPECT_EQ(data[0], 0x81);
    EXPECT_EQ(data[1], 0x00);
    EXPECT_EQ(data[2], 0x00);
    EXPECT_EQ(data[3], 0x42);
}

TEST(vlan_tag, store_insufficient_buffer)
{
    std::array<uint8_t, 2> data{};
    std::span<uint8_t> const buf{data};
    VlanTag vlan{0x042, false, 0};  // VID=66, DEI=0, PCP=0

    auto const status = store(buf, vlan);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

//
// Tests: VlanTag - load_unchecked()
//

TEST(vlan_tag, load_unchecked_deserializes_without_validation)
{
    // Test with VLAN tag present (tpid = 0x8100)
    std::array<uint8_t, 8> data{0x81, 0x00, 0x33, 0x44, 0xFF, 0xFF, 0xFF, 0xFF};
    std::span<uint8_t const> const buf{data};
    VlanTag vlan{};

    auto const bytes_consumed = load_unchecked(buf, &vlan);

    EXPECT_EQ(bytes_consumed, 4);
    // doublet_t uses network byte order (big-endian)
    EXPECT_EQ(vlan.tpid, VlanTag::ETHERTYPE);
    EXPECT_EQ(vlan.tci, 0x3344);
}

TEST(vlan_tag, load_unchecked_no_tag_present)
{
    // Test with no VLAN tag present (tpid != 0x8100)
    std::array<uint8_t, 8> data{0x08, 0x00, 0x33, 0x44, 0xFF, 0xFF, 0xFF, 0xFF};
    std::span<uint8_t const> const buf{data};
    VlanTag vlan{};

    auto const bytes_consumed = load_unchecked(buf, &vlan);

    EXPECT_EQ(bytes_consumed, 0);
    // Should be reset to 0
    EXPECT_EQ(vlan.tpid, 0);
    EXPECT_EQ(vlan.tci, 0);
}

//
// Tests: VlanTag - store_unchecked()
//

TEST(vlan_tag, store_unchecked_serializes_without_validation)
{
    std::array<uint8_t, 8> data{};
    std::span<uint8_t> const buf{data};
    VlanTag vlan = VlanTag::from_raw(VlanTag::ETHERTYPE, 0x1234);

    auto const bytes_written = store_unchecked(buf, vlan);

    EXPECT_EQ(bytes_written, 4);
    // doublet_t uses network byte order (big-endian)
    EXPECT_EQ(data[0], 0x81);
    EXPECT_EQ(data[1], 0x00);
    EXPECT_EQ(data[2], 0x12);
    EXPECT_EQ(data[3], 0x34);
}

TEST(vlan_tag, store_unchecked_no_tag)
{
    std::array<uint8_t, 8> data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    std::span<uint8_t> const buf{data};
    VlanTag vlan = VlanTag::from_raw(0x0800, 0x1234);  // Not a VLAN tag

    auto const bytes_written = store_unchecked(buf, vlan);

    EXPECT_EQ(bytes_written, 0);
    // Buffer should be unchanged
    EXPECT_EQ(data[0], 0xFF);
    EXPECT_EQ(data[1], 0xFF);
}

//
// Tests: VlanTag - Round-trip serialization/deserialization
//

TEST(vlan_tag, round_trip_preserves_values)
{
    VlanTag original{0x042, false, 0};  // VID=66, DEI=0, PCP=0
    std::array<uint8_t, 4> buffer{};

    // Serialize
    auto const store_status = store(std::span<uint8_t>{buffer}, original);
    EXPECT_TRUE(store_status);

    // Deserialize
    VlanTag deserialized{};
    auto const load_status = load(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_TRUE(load_status);

    // Verify
    EXPECT_EQ(deserialized.tpid, original.tpid);
    EXPECT_EQ(deserialized.tci, original.tci);
}

TEST(vlan_tag, round_trip_unchecked_preserves_values)
{
    VlanTag original{0xFFF, false, 0};  // VID=4095, DEI=0, PCP=0
    std::array<uint8_t, 4> buffer{};

    // Serialize unchecked
    auto const bytes_written = store_unchecked(std::span<uint8_t>{buffer}, original);
    EXPECT_EQ(bytes_written, 4);

    // Deserialize unchecked
    VlanTag deserialized{};
    auto const bytes_read = load_unchecked(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_EQ(bytes_read, 4);

    // Verify
    EXPECT_EQ(deserialized.tpid, original.tpid);
    EXPECT_EQ(deserialized.tci, original.tci);
}

TEST(vlan_tag, round_trip_with_builders_preserves_values)
{
    VlanTag original{0xABC, false, 0};  // VID=2748, DEI=0, PCP=0
    BufferSerializerBuilderWithStorage<4> serializer{};

    // Serialize using builder
    serializer.append(original);
    EXPECT_TRUE(serializer);
    EXPECT_EQ(serializer.get_span().size(), 4);

    // Deserialize using builder
    VlanTag deserialized{};
    BufferDeserializerBuilder deserializer{serializer.get_span()};
    deserializer.parse(&deserialized);
    EXPECT_TRUE(deserializer);

    // Verify
    EXPECT_EQ(deserialized.tpid, original.tpid);
    EXPECT_EQ(deserialized.tci, original.tci);
}

//
// Tests: VlanTag - get_vid(), get_pcp(), get_dei()
//

TEST(vlan_tag, get_vid_returns_correct_value)
{
    VlanTag vlan{0x042, false, 0};  // VID=66, DEI=0, PCP=0

    auto const vid = vlan.get_vid();

    EXPECT_EQ(vid, 0x042);
}

TEST(vlan_tag, get_vid_extracts_only_12_bits)
{
    VlanTag vlan = VlanTag::from_raw(VlanTag::ETHERTYPE, 0xFFFF);  // All bits set

    auto const vid = vlan.get_vid();

    EXPECT_EQ(vid, 0x0FFF);  // Only 12 bits for VID
}

TEST(vlan_tag, get_vid_returns_zero_when_not_set)
{
    VlanTag vlan = VlanTag::from_raw(0x0800, 0x0042);  // Not a VLAN tag

    auto const vid = vlan.get_vid();

    EXPECT_EQ(vid, 0);
}

TEST(vlan_tag, get_pcp_returns_correct_value)
{
    VlanTag vlan{0, false, 7};  // VID=0, DEI=0, PCP=7

    auto const pcp = vlan.get_pcp();

    EXPECT_EQ(pcp, 7);
}

TEST(vlan_tag, get_pcp_extracts_only_3_bits)
{
    VlanTag vlan{0, false, 5};  // VID=0, DEI=0, PCP=5

    auto const pcp = vlan.get_pcp();

    EXPECT_EQ(pcp, 5);
}

TEST(vlan_tag, get_pcp_returns_zero_when_not_set)
{
    VlanTag vlan = VlanTag::from_raw(0x0800, 0xE000);  // Not a VLAN tag

    auto const pcp = vlan.get_pcp();

    EXPECT_EQ(pcp, 0);
}

TEST(vlan_tag, get_dei_returns_true_when_set)
{
    VlanTag vlan{0, true, 0};  // VID=0, DEI=1, PCP=0

    auto const dei = vlan.get_dei();

    EXPECT_TRUE(dei);
}

TEST(vlan_tag, get_dei_returns_false_when_clear)
{
    VlanTag vlan{0xFFF, false, 0};  // VID=4095, DEI=0, PCP=0

    auto const dei = vlan.get_dei();

    EXPECT_FALSE(dei);
}

TEST(vlan_tag, get_dei_returns_false_when_not_set)
{
    VlanTag vlan = VlanTag::from_raw(0x0800, 0x1000);  // Not a VLAN tag

    auto const dei = vlan.get_dei();

    EXPECT_FALSE(dei);
}

TEST(vlan_tag, getters_work_together)
{
    // VID=0x5BC, DEI=0, PCP=5
    VlanTag vlan{0x5BC, false, 5};

    EXPECT_EQ(vlan.get_pcp(), 5);
    EXPECT_FALSE(vlan.get_dei());
    EXPECT_EQ(vlan.get_vid(), 0x5BC);
}

//
// Tests: VlanTag - set_vid(), set_pcp(), set_dei()
//

TEST(vlan_tag, set_vid_updates_vid_field)
{
    VlanTag vlan{0, false, 0};  // VID=0, DEI=0, PCP=0

    vlan.set_vid(0x123);

    EXPECT_EQ(vlan.get_vid(), 0x123);
    EXPECT_EQ(vlan.tci, 0x0123);
}

TEST(vlan_tag, set_vid_preserves_pcp_and_dei)
{
    // VID=0x5BC, DEI=0, PCP=7
    VlanTag vlan{0x5BC, false, 7};

    vlan.set_vid(0x042);

    EXPECT_EQ(vlan.get_vid(), 0x042);
    EXPECT_EQ(vlan.get_pcp(), 7);
    EXPECT_FALSE(vlan.get_dei());
    EXPECT_EQ(vlan.tci, 0xE042);
}

TEST(vlan_tag, set_vid_masks_to_12_bits)
{
    VlanTag vlan{0, false, 0};  // VID=0, DEI=0, PCP=0

    vlan.set_vid(0xFFFF);  // Try to set all bits

    EXPECT_EQ(vlan.get_vid(), 0x0FFF);  // Only 12 bits should be set
}

TEST(vlan_tag, set_vid_sets_tpid_if_not_set)
{
    VlanTag vlan{};  // Not a VLAN tag (default constructor, TPID=0)

    vlan.set_vid(0x100);

    EXPECT_TRUE(vlan.is_set());
    EXPECT_EQ(vlan.tpid, VlanTag::ETHERTYPE);
    EXPECT_EQ(vlan.get_vid(), 0x100);
}

TEST(vlan_tag, set_pcp_updates_pcp_field)
{
    VlanTag vlan{0, false, 0};  // VID=0, DEI=0, PCP=0

    vlan.set_pcp(5);

    EXPECT_EQ(vlan.get_pcp(), 5);
    EXPECT_EQ(vlan.tci, 0xA000);  // PCP=5 in bits 13-15
}

TEST(vlan_tag, set_pcp_preserves_dei_and_vid)
{
    // VID=0x5BC, DEI=0, PCP=7
    VlanTag vlan{0x5BC, false, 7};

    vlan.set_pcp(3);

    EXPECT_EQ(vlan.get_pcp(), 3);
    EXPECT_FALSE(vlan.get_dei());
    EXPECT_EQ(vlan.get_vid(), 0x5BC);
    EXPECT_EQ(vlan.tci, 0x65BC);
}

TEST(vlan_tag, set_pcp_masks_to_3_bits)
{
    VlanTag vlan{0, false, 0};  // VID=0, DEI=0, PCP=0

    vlan.set_pcp(0xFF);  // Try to set all bits

    EXPECT_EQ(vlan.get_pcp(), 7);  // Only 3 bits should be set
}

TEST(vlan_tag, set_pcp_sets_tpid_if_not_set)
{
    VlanTag vlan{};  // Not a VLAN tag (default constructor, TPID=0)

    vlan.set_pcp(4);

    EXPECT_TRUE(vlan.is_set());
    EXPECT_EQ(vlan.tpid, VlanTag::ETHERTYPE);
    EXPECT_EQ(vlan.get_pcp(), 4);
}

TEST(vlan_tag, set_dei_sets_dei_bit)
{
    VlanTag vlan{0, false, 0};  // VID=0, DEI=0, PCP=0

    vlan.set_dei(true);

    EXPECT_TRUE(vlan.get_dei());
    EXPECT_EQ(vlan.tci, 0x1000);  // DEI bit 12 set
}

TEST(vlan_tag, set_dei_clears_dei_bit)
{
    VlanTag vlan = VlanTag::from_raw(VlanTag::ETHERTYPE, 0xFFFF);

    vlan.set_dei(false);

    EXPECT_FALSE(vlan.get_dei());
    EXPECT_EQ(vlan.tci, 0xEFFF);  // DEI bit 12 cleared
}

TEST(vlan_tag, set_dei_preserves_pcp_and_vid)
{
    // VID=0x5BC, DEI=0, PCP=7
    VlanTag vlan{0x5BC, false, 7};

    vlan.set_dei(true);

    EXPECT_TRUE(vlan.get_dei());
    EXPECT_EQ(vlan.get_pcp(), 7);
    EXPECT_EQ(vlan.get_vid(), 0x5BC);
    EXPECT_EQ(vlan.tci, 0xF5BC);
}

TEST(vlan_tag, set_dei_sets_tpid_if_not_set)
{
    VlanTag vlan{};  // Not a VLAN tag (default constructor, TPID=0)

    vlan.set_dei(true);

    EXPECT_TRUE(vlan.is_set());
    EXPECT_EQ(vlan.tpid, VlanTag::ETHERTYPE);
    EXPECT_TRUE(vlan.get_dei());
}

TEST(vlan_tag, setters_work_together)
{
    VlanTag vlan{0, false, 0};  // VID=0, DEI=0, PCP=0

    vlan.set_pcp(5);
    vlan.set_dei(true);
    vlan.set_vid(0x5BC);

    EXPECT_EQ(vlan.get_pcp(), 5);
    EXPECT_TRUE(vlan.get_dei());
    EXPECT_EQ(vlan.get_vid(), 0x5BC);
    EXPECT_EQ(vlan.tci, 0xB5BC);  // PCP=5, DEI=1, VID=0x5BC
}

TEST(vlan_tag, setters_can_modify_existing_values)
{
    // VID=0x042, DEI=0, PCP=5
    VlanTag vlan{0x042, false, 5};

    vlan.set_vid(0x123);
    vlan.set_pcp(7);
    vlan.set_dei(true);

    EXPECT_EQ(vlan.get_vid(), 0x123);
    EXPECT_EQ(vlan.get_pcp(), 7);
    EXPECT_TRUE(vlan.get_dei());
    EXPECT_EQ(vlan.tci, 0xF123);  // PCP=7, DEI=1, VID=0x123
}

TEST(vlan_tag, setters_can_individually_update_fields)
{
    // VID=0x5BC, DEI=0, PCP=7
    VlanTag vlan{0x5BC, false, 7};

    // Update only VID
    vlan.set_vid(0xFFF);
    EXPECT_EQ(vlan.get_pcp(), 7);
    EXPECT_FALSE(vlan.get_dei());
    EXPECT_EQ(vlan.get_vid(), 0xFFF);

    // Update only DEI
    vlan.set_dei(true);
    EXPECT_EQ(vlan.get_pcp(), 7);
    EXPECT_TRUE(vlan.get_dei());
    EXPECT_EQ(vlan.get_vid(), 0xFFF);

    // Update only PCP
    vlan.set_pcp(0);
    EXPECT_EQ(vlan.get_pcp(), 0);
    EXPECT_TRUE(vlan.get_dei());
    EXPECT_EQ(vlan.get_vid(), 0xFFF);
}

//
// Tests: VlanTag - Round-trip with setters
//

TEST(vlan_tag, round_trip_using_setters)
{
    VlanTag vlan{};  // Start with unset VLAN tag

    // Build tag using setters
    vlan.set_vid(0x042);
    vlan.set_pcp(5);
    vlan.set_dei(false);

    // Serialize
    std::array<uint8_t, 4> buffer{};
    auto const store_status = store(std::span<uint8_t>{buffer}, vlan);
    EXPECT_TRUE(store_status);

    // Deserialize
    VlanTag deserialized{};
    auto const load_status = load(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_TRUE(load_status);

    // Verify using getters
    EXPECT_EQ(deserialized.get_vid(), 0x042);
    EXPECT_EQ(deserialized.get_pcp(), 5);
    EXPECT_FALSE(deserialized.get_dei());
}

//
// Tests: EthernetFrame - can_load()
//

TEST(ethernet_frame, can_load_untagged_frame_sufficient_buffer)
{
    // Untagged frame buffer with 20 bytes (more than needed)
    std::array<uint8_t, 20> data{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,  // dest_mac
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,  // src_mac
        0x08,
        0x00  // ethertype = 0x0800 (not VLAN)
    };
    EthernetFrame frame{};

    auto const status = can_load(std::span<uint8_t const>{data}, &frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 14);  // Untagged frame needs 14 bytes
}

TEST(ethernet_frame, can_load_untagged_frame_exact_buffer)
{
    // Untagged frame buffer with exactly 14 bytes
    std::array<uint8_t, 14> data{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,  // dest_mac
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,  // src_mac
        0x08,
        0x00  // ethertype = 0x0800 (not VLAN)
    };
    EthernetFrame frame{};

    auto const status = can_load(std::span<uint8_t const>{data}, &frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 14);
}

TEST(ethernet_frame, can_load_vlan_tagged_frame_sufficient_buffer)
{
    // VLAN tagged frame buffer with 20 bytes (more than needed)
    std::array<uint8_t, 20> data{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,  // dest_mac
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,  // src_mac
        0x81,
        0x00,  // tpid = 0x8100 (VLAN tag indicator)
        0x0A,
        0xBC,  // tci
        0x08,
        0x00  // actual ethertype
    };
    EthernetFrame frame{};

    auto const status = can_load(std::span<uint8_t const>{data}, &frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 18);  // VLAN tagged frame needs 18 bytes
}

TEST(ethernet_frame, can_load_vlan_tagged_frame_exact_buffer)
{
    // VLAN tagged frame buffer with exactly 18 bytes
    std::array<uint8_t, 18> data{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,  // dest_mac
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,  // src_mac
        0x81,
        0x00,  // tpid = 0x8100 (VLAN tag indicator)
        0x0A,
        0xBC,  // tci
        0x08,
        0x00  // actual ethertype
    };
    EthernetFrame frame{};

    auto const status = can_load(std::span<uint8_t const>{data}, &frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 18);
}

TEST(ethernet_frame, can_load_insufficient_buffer_less_than_14)
{
    // Buffer with only 10 bytes (less than minimum)
    std::array<uint8_t, 10> data{};
    EthernetFrame frame{};

    auto const status = can_load(std::span<uint8_t const>{data}, &frame);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

TEST(ethernet_frame, can_load_vlan_tagged_insufficient_buffer)
{
    // Buffer with 16 bytes, but VLAN tag detected (needs 18)
    std::array<uint8_t, 16> data{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,  // dest_mac
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,  // src_mac
        0x81,
        0x00,  // tpid = 0x8100 (VLAN tag indicator)
        0x0A,
        0xBC  // only tci, missing ethertype
    };
    EthernetFrame frame{};

    auto const status = can_load(std::span<uint8_t const>{data}, &frame);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

//
// Tests: EthernetFrame - can_store()
//

TEST(ethernet_frame, can_store_untagged_frame_sufficient_buffer)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800};
    std::array<uint8_t, 20> buffer{};

    auto const status = can_store(std::span<uint8_t>{buffer}, frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 14);  // Untagged frame needs 14 bytes
}

TEST(ethernet_frame, can_store_untagged_frame_exact_buffer)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800};
    std::array<uint8_t, 14> buffer{};

    auto const status = can_store(std::span<uint8_t>{buffer}, frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 14);
}

TEST(ethernet_frame, can_store_vlan_tagged_frame_sufficient_buffer)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = VlanTag{0xABC, false, 0},  // VID=0xABC, DEI=0, PCP=0
        .ethertype = 0x0800};
    std::array<uint8_t, 20> buffer{};

    auto const status = can_store(std::span<uint8_t>{buffer}, frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 18);  // VLAN tagged frame needs 18 bytes
}

TEST(ethernet_frame, can_store_vlan_tagged_frame_exact_buffer)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = VlanTag{0xABC, false, 0},  // VID=0xABC, DEI=0, PCP=0
        .ethertype = 0x0800};
    std::array<uint8_t, 18> buffer{};

    auto const status = can_store(std::span<uint8_t>{buffer}, frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 18);
}

TEST(ethernet_frame, can_store_untagged_frame_insufficient_buffer)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800};
    std::array<uint8_t, 10> buffer{};

    auto const status = can_store(std::span<uint8_t>{buffer}, frame);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

TEST(ethernet_frame, can_store_vlan_tagged_frame_insufficient_buffer)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = VlanTag{0xABC, false, 0},  // VID=0xABC, DEI=0, PCP=0
        .ethertype = 0x0800};
    std::array<uint8_t, 16> buffer{};  // Only 16 bytes, needs 18

    auto const status = can_store(std::span<uint8_t>{buffer}, frame);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

//
// Tests: EthernetFrame - load() / store()
//

TEST(ethernet_frame, load_untagged_frame)
{
    std::array<uint8_t, 14> data{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,  // dest_mac
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,  // src_mac
        0x08,
        0x00  // ethertype = 0x0800 (IPv4)
    };

    EthernetFrame frame{};
    auto const status = load(std::span<uint8_t const>{data}, &frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 14);
    EXPECT_EQ(frame.dest_mac.value[0], 0x01);
    EXPECT_EQ(frame.dest_mac.value[5], 0x06);
    EXPECT_EQ(frame.src_mac.value[0], 0x0A);
    EXPECT_EQ(frame.src_mac.value[5], 0x0F);
    EXPECT_EQ(frame.ethertype, 0x0800);
    EXPECT_FALSE(frame.vlan_tag.is_set());
}

TEST(ethernet_frame, load_vlan_tagged_frame)
{
    std::array<uint8_t, 18> data{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,  // dest_mac
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,  // src_mac
        0x81,
        0x00,  // tpid = 0x8100 (VLAN tag)
        0x0A,
        0xBC,  // tci = 0x0ABC
        0x08,
        0x00  // ethertype = 0x0800 (IPv4)
    };

    EthernetFrame frame{};
    auto const status = load(std::span<uint8_t const>{data}, &frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 18);
    EXPECT_EQ(frame.dest_mac.value[0], 0x01);
    EXPECT_EQ(frame.dest_mac.value[5], 0x06);
    EXPECT_EQ(frame.src_mac.value[0], 0x0A);
    EXPECT_EQ(frame.src_mac.value[5], 0x0F);
    EXPECT_TRUE(frame.vlan_tag.is_set());
    EXPECT_EQ(frame.vlan_tag.tpid, 0x8100);
    EXPECT_EQ(frame.vlan_tag.tci, 0x0ABC);
    EXPECT_EQ(frame.ethertype, 0x0800);
}

TEST(ethernet_frame, load_insufficient_buffer)
{
    std::array<uint8_t, 10> data{};
    EthernetFrame frame{};

    auto const status = load(std::span<uint8_t const>{data}, &frame);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_data);
}

TEST(ethernet_frame, store_untagged_frame)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800};

    std::array<uint8_t, 14> buffer{};
    auto const status = store(std::span<uint8_t>{buffer}, frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 14);
    EXPECT_EQ(buffer[0], 0x01);
    EXPECT_EQ(buffer[5], 0x06);
    EXPECT_EQ(buffer[6], 0x0A);
    EXPECT_EQ(buffer[11], 0x0F);
    EXPECT_EQ(buffer[12], 0x08);
    EXPECT_EQ(buffer[13], 0x00);
}

TEST(ethernet_frame, store_vlan_tagged_frame)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = VlanTag{0xABC, false, 0},  // VID=0xABC, DEI=0, PCP=0
        .ethertype = 0x0800};

    std::array<uint8_t, 18> buffer{};
    auto const status = store(std::span<uint8_t>{buffer}, frame);

    EXPECT_TRUE(status);
    EXPECT_EQ(status.value(), 18);
    EXPECT_EQ(buffer[0], 0x01);
    EXPECT_EQ(buffer[5], 0x06);
    EXPECT_EQ(buffer[6], 0x0A);
    EXPECT_EQ(buffer[11], 0x0F);
    EXPECT_EQ(buffer[12], 0x81);
    EXPECT_EQ(buffer[13], 0x00);
    EXPECT_EQ(buffer[14], 0x0A);
    EXPECT_EQ(buffer[15], 0xBC);
    EXPECT_EQ(buffer[16], 0x08);
    EXPECT_EQ(buffer[17], 0x00);
}

TEST(ethernet_frame, store_insufficient_buffer)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800};

    std::array<uint8_t, 10> buffer{};
    auto const status = store(std::span<uint8_t>{buffer}, frame);

    EXPECT_FALSE(status);
    EXPECT_EQ(status.error(), BufferError::insufficient_space);
}

TEST(ethernet_frame, round_trip_untagged)
{
    EthernetFrame original{
        .dest_mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
        .src_mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800};

    std::array<uint8_t, 14> buffer{};

    // Serialize
    auto const store_status = store(std::span<uint8_t>{buffer}, original);
    EXPECT_TRUE(store_status);
    EXPECT_EQ(store_status.value(), 14);

    // Deserialize
    EthernetFrame deserialized{};
    auto const load_status = load(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_TRUE(load_status);
    EXPECT_EQ(load_status.value(), 14);

    // Verify
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(deserialized.dest_mac.value[i], original.dest_mac.value[i]);
        EXPECT_EQ(deserialized.src_mac.value[i], original.src_mac.value[i]);
    }
    EXPECT_FALSE(deserialized.vlan_tag.is_set());
    EXPECT_EQ(deserialized.ethertype, original.ethertype);
}

TEST(ethernet_frame, round_trip_vlan_tagged)
{
    EthernetFrame original{
        .dest_mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
        .src_mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
        .vlan_tag = VlanTag{0xFFF, false, 0},  // VID=0xFFF, DEI=0, PCP=0
        .ethertype = 0x86DD};

    std::array<uint8_t, 18> buffer{};

    // Serialize
    auto const store_status = store(std::span<uint8_t>{buffer}, original);
    EXPECT_TRUE(store_status);
    EXPECT_EQ(store_status.value(), 18);

    // Deserialize
    EthernetFrame deserialized{};
    auto const load_status = load(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_TRUE(load_status);
    EXPECT_EQ(load_status.value(), 18);

    // Verify
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(deserialized.dest_mac.value[i], original.dest_mac.value[i]);
        EXPECT_EQ(deserialized.src_mac.value[i], original.src_mac.value[i]);
    }
    EXPECT_TRUE(deserialized.vlan_tag.is_set());
    EXPECT_EQ(deserialized.vlan_tag.tpid, original.vlan_tag.tpid);
    EXPECT_EQ(deserialized.vlan_tag.tci, original.vlan_tag.tci);
    EXPECT_EQ(deserialized.ethertype, original.ethertype);
}

//
// Tests: EthernetFrame - load_unchecked() / store_unchecked()
//

TEST(ethernet_frame, load_unchecked_untagged_frame)
{
    // Untagged frame: dest_mac + src_mac + ethertype (14 bytes)
    std::array<uint8_t, 14> data{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,  // dest_mac
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,  // src_mac
        0x08,
        0x00  // ethertype = 0x0800 (IPv4)
    };

    EthernetFrame frame{};
    auto const bytes_read = load_unchecked(std::span<uint8_t const>{data}, &frame);

    EXPECT_EQ(bytes_read, 14);
    EXPECT_EQ(frame.dest_mac.value[0], 0x01);
    EXPECT_EQ(frame.dest_mac.value[5], 0x06);
    EXPECT_EQ(frame.src_mac.value[0], 0x0A);
    EXPECT_EQ(frame.src_mac.value[5], 0x0F);
    EXPECT_EQ(frame.ethertype, 0x0800);
    EXPECT_FALSE(frame.vlan_tag.is_set());
}

TEST(ethernet_frame, load_unchecked_vlan_tagged_frame)
{
    // VLAN tagged frame: dest_mac + src_mac + vlan_tag + ethertype (18 bytes)
    std::array<uint8_t, 18> data{
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,  // dest_mac
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,  // src_mac
        0x81,
        0x00,  // tpid = 0x8100 (VLAN tag)
        0x0A,
        0xBC,  // tci = 0x0ABC
        0x08,
        0x00  // ethertype = 0x0800 (IPv4)
    };

    EthernetFrame frame{};
    auto const bytes_read = load_unchecked(std::span<uint8_t const>{data}, &frame);

    EXPECT_EQ(bytes_read, 18);
    EXPECT_EQ(frame.dest_mac.value[0], 0x01);
    EXPECT_EQ(frame.dest_mac.value[5], 0x06);
    EXPECT_EQ(frame.src_mac.value[0], 0x0A);
    EXPECT_EQ(frame.src_mac.value[5], 0x0F);
    EXPECT_TRUE(frame.vlan_tag.is_set());
    EXPECT_EQ(frame.vlan_tag.tpid, 0x8100);
    EXPECT_EQ(frame.vlan_tag.tci, 0x0ABC);
    EXPECT_EQ(frame.ethertype, 0x0800);
}

TEST(ethernet_frame, store_unchecked_untagged_frame)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800};

    std::array<uint8_t, 14> buffer{};
    auto const bytes_written = store_unchecked(std::span<uint8_t>{buffer}, frame);

    EXPECT_EQ(bytes_written, 14);
    EXPECT_EQ(buffer[0], 0x01);
    EXPECT_EQ(buffer[5], 0x06);
    EXPECT_EQ(buffer[6], 0x0A);
    EXPECT_EQ(buffer[11], 0x0F);
    EXPECT_EQ(buffer[12], 0x08);
    EXPECT_EQ(buffer[13], 0x00);
}

TEST(ethernet_frame, store_unchecked_vlan_tagged_frame)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = VlanTag{0xABC, false, 0},  // VID=0xABC, DEI=0, PCP=0
        .ethertype = 0x0800};

    std::array<uint8_t, 18> buffer{};
    auto const bytes_written = store_unchecked(std::span<uint8_t>{buffer}, frame);

    EXPECT_EQ(bytes_written, 18);
    EXPECT_EQ(buffer[0], 0x01);
    EXPECT_EQ(buffer[5], 0x06);
    EXPECT_EQ(buffer[6], 0x0A);
    EXPECT_EQ(buffer[11], 0x0F);
    EXPECT_EQ(buffer[12], 0x81);
    EXPECT_EQ(buffer[13], 0x00);
    EXPECT_EQ(buffer[14], 0x0A);
    EXPECT_EQ(buffer[15], 0xBC);
    EXPECT_EQ(buffer[16], 0x08);
    EXPECT_EQ(buffer[17], 0x00);
}

TEST(ethernet_frame, round_trip_untagged_preserves_values)
{
    EthernetFrame original{
        .dest_mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
        .src_mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x86DD  // IPv6
    };

    std::array<uint8_t, 14> buffer{};

    // Serialize
    auto const bytes_written = store_unchecked(std::span<uint8_t>{buffer}, original);
    EXPECT_EQ(bytes_written, 14);

    // Deserialize
    EthernetFrame deserialized{};
    auto const bytes_read = load_unchecked(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_EQ(bytes_read, 14);

    // Verify
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(deserialized.dest_mac.value[i], original.dest_mac.value[i]);
        EXPECT_EQ(deserialized.src_mac.value[i], original.src_mac.value[i]);
    }
    EXPECT_FALSE(deserialized.vlan_tag.is_set());
    EXPECT_EQ(deserialized.ethertype, original.ethertype);
}

TEST(ethernet_frame, round_trip_vlan_tagged_preserves_values)
{
    EthernetFrame original{
        .dest_mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
        .src_mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
        .vlan_tag = VlanTag{0xFFF, true, 0},  // VID=0xFFF, DEI=1, PCP=0 (TCI=0x1FFF)
        .ethertype = 0x86DD                   // IPv6
    };

    std::array<uint8_t, 18> buffer{};

    // Serialize
    auto const bytes_written = store_unchecked(std::span<uint8_t>{buffer}, original);
    EXPECT_EQ(bytes_written, 18);

    // Deserialize
    EthernetFrame deserialized{};
    auto const bytes_read = load_unchecked(std::span<uint8_t const>{buffer}, &deserialized);
    EXPECT_EQ(bytes_read, 18);

    // Verify
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(deserialized.dest_mac.value[i], original.dest_mac.value[i]);
        EXPECT_EQ(deserialized.src_mac.value[i], original.src_mac.value[i]);
    }
    EXPECT_TRUE(deserialized.vlan_tag.is_set());
    EXPECT_EQ(deserialized.vlan_tag.tpid, original.vlan_tag.tpid);
    EXPECT_EQ(deserialized.vlan_tag.tci, original.vlan_tag.tci);
    EXPECT_EQ(deserialized.ethertype, original.ethertype);
}

TEST(ethernet_frame, round_trip_with_builders_untagged)
{
    EthernetFrame original{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800  // IPv4
    };

    BufferSerializerBuilderWithStorage<14> serializer{};

    // Serialize using builder
    serializer.append(original);
    EXPECT_TRUE(serializer);
    EXPECT_EQ(serializer.get_span().size(), 14);

    // Deserialize using builder
    EthernetFrame deserialized{};
    BufferDeserializerBuilder deserializer{serializer.get_span()};
    deserializer.parse(&deserialized);
    EXPECT_TRUE(deserializer);

    // Verify
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(deserialized.dest_mac.value[i], original.dest_mac.value[i]);
        EXPECT_EQ(deserialized.src_mac.value[i], original.src_mac.value[i]);
    }
    EXPECT_FALSE(deserialized.vlan_tag.is_set());
    EXPECT_EQ(deserialized.ethertype, original.ethertype);
}

TEST(ethernet_frame, round_trip_with_builders_vlan_tagged)
{
    EthernetFrame original{
        .dest_mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
        .src_mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
        .vlan_tag = VlanTag{0xFFF, true, 0},  // VID=0xFFF, DEI=1, PCP=0 (TCI=0x1FFF)
        .ethertype = 0x86DD                   // IPv6
    };

    BufferSerializerBuilderWithStorage<18> serializer{};

    // Serialize using builder
    serializer.append(original);
    EXPECT_TRUE(serializer);
    EXPECT_EQ(serializer.get_span().size(), 18);

    // Deserialize using builder
    EthernetFrame deserialized{};
    BufferDeserializerBuilder deserializer{serializer.get_span()};
    deserializer.parse(&deserialized);
    EXPECT_TRUE(deserializer);

    // Verify
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(deserialized.dest_mac.value[i], original.dest_mac.value[i]);
        EXPECT_EQ(deserialized.src_mac.value[i], original.src_mac.value[i]);
    }
    EXPECT_TRUE(deserialized.vlan_tag.is_set());
    EXPECT_EQ(deserialized.vlan_tag.tpid, original.vlan_tag.tpid);
    EXPECT_EQ(deserialized.vlan_tag.tci, original.vlan_tag.tci);
    EXPECT_EQ(deserialized.ethertype, original.ethertype);
}

//
// Tests: Eui48 - size() static method
//

TEST(eui48_static, size_returns_6)
{
    EXPECT_EQ(Eui48::size(), 6);
}

//
// Tests: Eui48 - operator==
//

TEST(eui48_operators, equality_same_values)
{
    Eui48 a{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    Eui48 b{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(eui48_operators, equality_different_values)
{
    Eui48 a{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    Eui48 b{0xFF, 0x11, 0x22, 0x33, 0x44, 0x55};

    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(eui48_operators, comparison_ordering)
{
    Eui48 a{0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Eui48 b{0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    Eui48 c{0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b < c);
    EXPECT_TRUE(a < c);
}

//
// Tests: Eui48 - to_string / format_to
//

TEST(eui48_format, to_string_formats_correctly)
{
    Eui48 mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    auto const result = to_string(mac);

    EXPECT_EQ(result, "aa:bb:cc:dd:ee:ff");
}

TEST(eui48_format, to_string_zero_address)
{
    Eui48 mac{0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    auto const result = to_string(mac);

    EXPECT_EQ(result, "00:00:00:00:00:00");
}

TEST(eui48_format, format_to_output_iterator)
{
    Eui48 mac{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
    std::string result;

    format_to(std::back_inserter(result), mac);

    EXPECT_EQ(result, "01:23:45:67:89:ab");
}

//
// Tests: Eui48 - from_string parsing
//

TEST(eui48_parse, from_string_colon_separated)
{
    auto result = eui48_from_string("aa:bb:cc:dd:ee:ff");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->value[0], 0xAA);
    EXPECT_EQ(result->value[5], 0xFF);
}

TEST(eui48_parse, from_string_dash_separated)
{
    auto result = eui48_from_string("AA-BB-CC-DD-EE-FF");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->value[0], 0xAA);
    EXPECT_EQ(result->value[5], 0xFF);
}

TEST(eui48_parse, from_string_no_separator)
{
    auto result = eui48_from_string("aabbccddeeff");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->value[0], 0xAA);
    EXPECT_EQ(result->value[5], 0xFF);
}

TEST(eui48_parse, from_string_with_whitespace)
{
    auto result = eui48_from_string("  aa:bb:cc:dd:ee:ff  ");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->value[0], 0xAA);
}

TEST(eui48_parse, from_string_invalid_length)
{
    auto result = eui48_from_string("aa:bb:cc");

    EXPECT_FALSE(result.has_value());
}

TEST(eui48_parse, from_string_invalid_char)
{
    auto result = eui48_from_string("gg:bb:cc:dd:ee:ff");

    EXPECT_FALSE(result.has_value());
}

TEST(eui48_parse, from_string_mixed_separator)
{
    auto result = eui48_from_string("aa:bb-cc:dd:ee:ff");

    EXPECT_FALSE(result.has_value());
}

//
// Tests: Eui64 - size() static method
//

TEST(eui64_static, size_returns_8)
{
    EXPECT_EQ(Eui64::size(), 8);
}

//
// Tests: Eui64 - operator==
//

TEST(eui64_operators, equality_same_values)
{
    Eui64 a{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    Eui64 b{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(eui64_operators, equality_different_values)
{
    Eui64 a{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    Eui64 b{0xFF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(eui64_operators, comparison_ordering)
{
    Eui64 a{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Eui64 b{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    Eui64 c{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b < c);
    EXPECT_TRUE(a < c);
}

//
// Tests: Eui64 - to_string / format_to
//

TEST(eui64_format, to_string_formats_correctly)
{
    Eui64 addr{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

    auto const result = to_string(addr);

    EXPECT_EQ(result, "aa:bb:cc:dd:ee:ff:11:22");
}

TEST(eui64_format, to_string_zero_address)
{
    Eui64 addr{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    auto const result = to_string(addr);

    EXPECT_EQ(result, "00:00:00:00:00:00:00:00");
}

TEST(eui64_format, format_to_output_iterator)
{
    Eui64 addr{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    std::string result;

    format_to(std::back_inserter(result), addr);

    EXPECT_EQ(result, "01:23:45:67:89:ab:cd:ef");
}

//
// Tests: Eui64 - from_string parsing
//

TEST(eui64_parse, from_string_colon_separated)
{
    auto result = eui64_from_string("aa:bb:cc:dd:ee:ff:11:22");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->span()[0], 0xAA);
    EXPECT_EQ(result->span()[7], 0x22);
}

TEST(eui64_parse, from_string_dash_separated)
{
    auto result = eui64_from_string("AA-BB-CC-DD-EE-FF-11-22");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->span()[0], 0xAA);
    EXPECT_EQ(result->span()[7], 0x22);
}

TEST(eui64_parse, from_string_no_separator)
{
    auto result = eui64_from_string("aabbccddeeff1122");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->span()[0], 0xAA);
    EXPECT_EQ(result->span()[7], 0x22);
}

TEST(eui64_parse, from_string_with_whitespace)
{
    auto result = eui64_from_string("  aa:bb:cc:dd:ee:ff:11:22  ");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->span()[0], 0xAA);
}

TEST(eui64_parse, from_string_invalid_length)
{
    auto result = eui64_from_string("aa:bb:cc:dd:ee:ff");

    EXPECT_FALSE(result.has_value());
}

TEST(eui64_parse, from_string_invalid_char)
{
    auto result = eui64_from_string("gg:bb:cc:dd:ee:ff:11:22");

    EXPECT_FALSE(result.has_value());
}

//
// Tests: VlanTag - operator==
//

TEST(vlan_tag_operators, equality_same_values)
{
    VlanTag a{100, false, 3};
    VlanTag b{100, false, 3};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(vlan_tag_operators, equality_different_values)
{
    VlanTag a{100, false, 3};
    VlanTag b{200, false, 3};

    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(vlan_tag_operators, equality_unset_tags)
{
    VlanTag a{};
    VlanTag b{};

    EXPECT_TRUE(a == b);
}

//
// Tests: EthernetFrame - is_valid()
//

TEST(ethernet_frame_valid, valid_untagged_frame)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800  // IPv4
    };

    EXPECT_TRUE(frame.is_valid());
}

TEST(ethernet_frame_valid, valid_tagged_frame)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = VlanTag{100, false, 3},
        .ethertype = 0x86DD  // IPv6
    };

    EXPECT_TRUE(frame.is_valid());
}

TEST(ethernet_frame_valid, invalid_zero_src_mac)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // Invalid
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800};

    EXPECT_FALSE(frame.is_valid());
}

TEST(ethernet_frame_valid, invalid_broadcast_src_mac)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // Invalid
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800};

    EXPECT_FALSE(frame.is_valid());
}

TEST(ethernet_frame_valid, invalid_low_ethertype)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0600  // 802.3 length field, not Ethernet II
    };

    EXPECT_FALSE(frame.is_valid());
}

TEST(ethernet_frame_valid, valid_minimum_ethertype)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0601  // Minimum valid Ethernet II ethertype
    };

    EXPECT_TRUE(frame.is_valid());
}

//
// Tests: EthernetFrame - format_to
//

TEST(ethernet_frame_format, format_untagged_frame)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
        .vlan_tag = make_empty_vlan_tag(),
        .ethertype = 0x0800};
    std::string result;

    format_to(std::back_inserter(result), frame);

    EXPECT_TRUE(result.find("aa:bb:cc:dd:ee:ff") != std::string::npos);
    EXPECT_TRUE(result.find("01:02:03:04:05:06") != std::string::npos);
    EXPECT_TRUE(result.find("0800") != std::string::npos);
}

TEST(ethernet_frame_format, format_tagged_frame)
{
    EthernetFrame frame{
        .dest_mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .src_mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
        .vlan_tag = VlanTag{100, false, 3},
        .ethertype = 0x0800};
    std::string result;

    format_to(std::back_inserter(result), frame);

    EXPECT_TRUE(result.find("vlan=100") != std::string::npos);
    EXPECT_TRUE(result.find("pcp=3") != std::string::npos);
}

//
// Tests: parse_hex_digit
//

TEST(hex_parse, parse_hex_digit_digits)
{
    EXPECT_EQ(parse_hex_digit('0'), 0);
    EXPECT_EQ(parse_hex_digit('5'), 5);
    EXPECT_EQ(parse_hex_digit('9'), 9);
}

TEST(hex_parse, parse_hex_digit_lowercase)
{
    EXPECT_EQ(parse_hex_digit('a'), 10);
    EXPECT_EQ(parse_hex_digit('f'), 15);
}

TEST(hex_parse, parse_hex_digit_uppercase)
{
    EXPECT_EQ(parse_hex_digit('A'), 10);
    EXPECT_EQ(parse_hex_digit('F'), 15);
}

TEST(hex_parse, parse_hex_digit_invalid)
{
    EXPECT_EQ(parse_hex_digit('g'), 255);
    EXPECT_EQ(parse_hex_digit('G'), 255);
    EXPECT_EQ(parse_hex_digit(' '), 255);
    EXPECT_EQ(parse_hex_digit(':'), 255);
}

//
// Tests: VlanTag helper functions
//

TEST(vlan_helpers, calculate_vlan_tci)
{
    // PCP=3 (bits 15-13), DEI=1 (bit 12), VID=100 (bits 11-0)
    auto tci = calculate_vlan_tci(100, true, 3);

    // Expected: 0x3000 (PCP) | 0x1000 (DEI) | 0x0064 (VID) = 0x7064
    EXPECT_EQ(tci, 0x7064);
}

TEST(vlan_helpers, make_vlan_tag_creates_tag)
{
    auto vlan = make_vlan_tag(100, true, 3);

    EXPECT_TRUE(vlan.is_set());
    EXPECT_EQ(vlan.get_vid(), 100);
    EXPECT_TRUE(vlan.get_dei());
    EXPECT_EQ(vlan.get_pcp(), 3);
}

TEST(vlan_helpers, make_empty_vlan_tag)
{
    auto vlan = make_empty_vlan_tag();

    EXPECT_FALSE(vlan.is_set());
}

TEST(vlan_helpers, is_tagged)
{
    VlanTag set_tag{100, false, 0};
    VlanTag empty_tag{};

    EXPECT_TRUE(is_tagged(set_tag));
    EXPECT_FALSE(is_tagged(empty_tag));
}

// ===========================================================================
// Static size() and const span() tests
// ===========================================================================

TEST(ieee_eui48_coverage, static_size_method)
{
    EXPECT_EQ(statusbar::ieee::Eui48::size(), 6U);
}

TEST(ieee_eui48_coverage, const_span_returns_6_bytes)
{
    statusbar::ieee::Eui48 const addr{0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    auto s = addr.span();
    EXPECT_EQ(s.size(), 6U);
    EXPECT_EQ(s[0], 0x11);
    EXPECT_EQ(s[5], 0x66);
}

TEST(ieee_eui64_coverage, static_size_method)
{
    EXPECT_EQ(statusbar::ieee::Eui64::size(), 8U);
}

// ===========================================================================
// ethertype_name tests
// ===========================================================================

TEST(ieee_ethertype_name, known_ethertypes)
{
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x0800), "IPv4");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x0806), "ARP");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x86DD), "IPv6");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x8100), "VLAN");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x88A8), "QinQ");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x88F7), "PTP");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x22F0), "AVTP");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x88CC), "LLDP");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x88E1), "HomePlug");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x8902), "CFM");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x88B8), "GOOSE");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x88BA), "SV");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x22EA), "SRP");
}

TEST(ieee_ethertype_name, unknown_ethertype_returns_hex)
{
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x1234), "0x1234");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0x0000), "0x0000");
    EXPECT_EQ(statusbar::ieee::ethertype_name(0xFFFF), "0xffff");
}

//
// Main test runner
//

TEST_MAIN(statusbar_ieee, ieee_ethernet_test)