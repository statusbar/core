// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/status/status.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <system_error>

using namespace statusbar::ieee;
using namespace statusbar::protocol;
using statusbar::BufferError;
using statusbar::failure;
using statusbar::Status;
using statusbar::StatusValue;
using statusbar::success;

//
// Example Protocol Header - Pure Data Structure
//

struct ProtocolHeader
{
    octet_t type;
    doublet_t flags;
    doublet_t length;
    octlet_t timestamp;

    static constexpr size_t LENGTH = 13U;
};

//
// Free Functions for Protocol Serialization
//

template <>
struct statusbar::traits::is_serializable_fixed_struct<ProtocolHeader> : std::true_type
{};

// Implement store_unchecked free function
[[nodiscard]] auto store_unchecked(std::span<uint8_t> const buf, ProtocolHeader const& header) noexcept -> size_t
{
    statusbar::BufferSerializerBuilderWithBuffer{buf}
        .append_unchecked(header.type)
        .append_unchecked(header.flags)
        .append_unchecked(header.length)
        .append_unchecked(header.timestamp);
    return wire_size(header);
}

// Implement load_unchecked free function
[[nodiscard]] auto load_unchecked(std::span<uint8_t const> const buf, ProtocolHeader* const header) noexcept -> size_t
{
    statusbar::BufferDeserializerBuilder{buf}
        .parse_unchecked(&header->type)
        .parse_unchecked(&header->flags)
        .parse_unchecked(&header->length)
        .parse_unchecked(&header->timestamp);
    return wire_size(*header);
}

//
// Protocol Header Tests
//

TEST(ieee_protocol_example, wire_size_is_correct)
{
    ProtocolHeader header{};
    EXPECT_EQ(wire_size(header), 13U);
}

TEST(ieee_protocol_example, struct_may_have_padding)
{
    // The struct in memory might have padding for alignment
    // but wire format is always 13 bytes (no padding)
    EXPECT_TRUE(sizeof(ProtocolHeader) >= 13U);
}

TEST(ieee_protocol_example, load_valid_packet)
{
    // Network packet with big-endian bytes
    uint8_t const packet[13] = {
        0x01,  // type
        0x00,
        0x20,  // flags = 0x0020
        0x01,
        0x00,  // length = 0x0100 (256)
        0x12,
        0x34,
        0x56,
        0x78,
        0x9A,
        0xBC,
        0xDE,
        0xF0  // timestamp
    };
    std::span<uint8_t const> buf{packet, 13};

    ProtocolHeader header;
    auto result = load(buf, &header);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 13U);
    EXPECT_EQ(header.type, 0x01U);
    EXPECT_EQ(header.flags, 0x0020U);
    EXPECT_EQ(header.length, 0x0100U);
    EXPECT_EQ(header.timestamp, 0x123456789ABCDEf0ull);
}

TEST(ieee_protocol_example, load_insufficient_buffer)
{
    uint8_t const packet[10] = {};  // Too small
    std::span<uint8_t const> buf{packet, 10};

    ProtocolHeader header;
    auto result = load(buf, &header);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), BufferError::insufficient_data);
}

TEST(ieee_protocol_example, store_valid_header)
{
    uint8_t packet[13] = {};
    std::span<uint8_t> buf{packet, 13};

    ProtocolHeader const header{
        .type = 0x42,
        .flags = 0x1234,
        .length = 500,
        .timestamp = 0xAABBCCDDEEFF0011ull,
    };

    auto result = store(buf, header);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 13U);

    // Verify network byte order
    EXPECT_EQ(packet[0], 0x42);  // type
    EXPECT_EQ(packet[1], 0x12);  // flags high byte
    EXPECT_EQ(packet[2], 0x34);  // flags low byte
    EXPECT_EQ(packet[3], 0x01);  // length high byte
    EXPECT_EQ(packet[4], 0xF4);  // length low byte (500 = 0x01F4)
    EXPECT_EQ(packet[5], 0xAA);  // timestamp bytes
    EXPECT_EQ(packet[6], 0xBB);
    EXPECT_EQ(packet[7], 0xCC);
    EXPECT_EQ(packet[8], 0xDD);
    EXPECT_EQ(packet[9], 0xEE);
    EXPECT_EQ(packet[10], 0xFF);
    EXPECT_EQ(packet[11], 0x00);
    EXPECT_EQ(packet[12], 0x11);
}

TEST(ieee_protocol_example, store_insufficient_buffer)
{
    uint8_t packet[10] = {};  // Too small
    std::span<uint8_t> buf{packet, 10};

    ProtocolHeader const header{
        .type = 0x01,
        .flags = 0x0020,
        .length = 100,
        .timestamp = 0x123456789ABCDEF0ull,
    };

    auto result = store(buf, header);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), BufferError::insufficient_space);
}

TEST(ieee_protocol_example, round_trip)
{
    ProtocolHeader const original{
        .type = 0xFF,
        .flags = 0xABCD,
        .length = 12345,
        .timestamp = 0xFEDCBA9876543210ull,
    };

    // Serialize
    uint8_t packet[13];
    std::span<uint8_t> write_buf{packet, 13};
    auto store_result = store(write_buf, original);
    EXPECT_TRUE(store_result.has_value());

    // Deserialize
    std::span<uint8_t const> read_buf{packet, 13};
    ProtocolHeader loaded;
    auto load_result = load(read_buf, &loaded);
    EXPECT_TRUE(load_result.has_value());

    // Verify
    EXPECT_EQ(loaded.type, original.type);
    EXPECT_EQ(loaded.flags, original.flags);
    EXPECT_EQ(loaded.length, original.length);
    EXPECT_EQ(loaded.timestamp, original.timestamp);
}

TEST(ieee_protocol_example, can_load_checks_size)
{
    uint8_t const packet[13] = {};
    std::span<uint8_t const> buf{packet, 13};

    ProtocolHeader header;
    auto result = can_load(buf, &header);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 13U);
}

TEST(ieee_protocol_example, can_store_checks_size)
{
    uint8_t packet[13] = {};
    std::span<uint8_t> buf{packet, 13};

    ProtocolHeader const header{};
    auto result = can_store(buf, header);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 13U);
}

TEST(ieee_protocol_example, load_unchecked_works)
{
    uint8_t const packet[13] = {
        0x05,  // type
        0x00,
        0x10,  // flags
        0x00,
        0x64,  // length = 100
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01  // timestamp = 1
    };
    std::span<uint8_t const> buf{packet, 13};

    ProtocolHeader header;
    auto bytes_read = load_unchecked(buf, &header);

    EXPECT_EQ(bytes_read, 13U);
    EXPECT_EQ(header.type, 0x05U);
    EXPECT_EQ(header.flags, 0x0010U);
    EXPECT_EQ(header.length, 100U);
    EXPECT_EQ(header.timestamp, 1ull);
}

TEST(ieee_protocol_example, store_unchecked_works)
{
    uint8_t packet[13] = {};
    std::span<uint8_t> buf{packet, 13};

    ProtocolHeader const header{
        .type = 0x99,
        .flags = 0x0001,
        .length = 1000,
        .timestamp = 0x0000000000000042ull,
    };

    auto bytes_written = store_unchecked(buf, header);

    EXPECT_EQ(bytes_written, 13U);
    EXPECT_EQ(packet[0], 0x99);
    EXPECT_EQ(packet[1], 0x00);  // flags high
    EXPECT_EQ(packet[2], 0x01);  // flags low
    EXPECT_EQ(packet[3], 0x03);  // length high (1000 = 0x03E8)
    EXPECT_EQ(packet[4], 0xE8);  // length low
}

TEST(ieee_protocol_example, natural_field_access_after_load)
{
    uint8_t const packet[13] = {
        0x0A,  // type
        0x12,
        0x34,  // flags
        0x56,
        0x78,  // length
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xFF  // timestamp
    };
    std::span<uint8_t const> buf{packet, 13};

    ProtocolHeader header;
    auto load_result = load(buf, &header);
    EXPECT_TRUE(load_result.has_value());

    // Natural access - no .get() needed
    uint8_t type_val = header.type;
    uint16_t flags_val = header.flags;
    uint16_t length_val = header.length;
    uint64_t timestamp_val = header.timestamp;

    EXPECT_EQ(type_val, 0x0Au);
    EXPECT_EQ(flags_val, 0x1234U);
    EXPECT_EQ(length_val, 0x5678U);
    EXPECT_EQ(timestamp_val, 0xFFull);
}

TEST(ieee_protocol_example, can_use_aggregate_initialization)
{
    ProtocolHeader header{
        .type = 1,
        .flags = 2,
        .length = 3,
        .timestamp = 4,
    };

    EXPECT_EQ(header.type, 1U);
    EXPECT_EQ(header.flags, 2U);
    EXPECT_EQ(header.length, 3U);
    EXPECT_EQ(header.timestamp, 4U);
}

TEST(ieee_protocol_example, fields_support_arithmetic)
{
    ProtocolHeader header{
        .type = 1,
        .flags = 0x0001,
        .length = 100,
        .timestamp = 1000,
    };

    // Natural arithmetic operations
    header.type = header.type + 1;
    header.flags = header.flags | 0x0002;
    header.length = header.length * 2;
    header.timestamp = header.timestamp + 500;

    EXPECT_EQ(header.type, 2U);
    EXPECT_EQ(header.flags, 0x0003U);
    EXPECT_EQ(header.length, 200U);
    EXPECT_EQ(header.timestamp, 1500U);
}

TEST(ieee_protocol_example, multiple_headers_in_sequence)
{
    // Simulate multiple headers in a packet
    uint8_t packet[26] = {};
    std::span<uint8_t> buf{packet};

    ProtocolHeader const header1{.type = 1, .flags = 0x0001, .length = 10, .timestamp = 100};
    ProtocolHeader const header2{.type = 2, .flags = 0x0002, .length = 20, .timestamp = 200};

    // Store both headers
    auto result1 = store(buf, header1);
    EXPECT_TRUE(result1.has_value());

    auto result2 = store(buf.subspan(*result1), header2);
    EXPECT_TRUE(result2.has_value());

    // Load both headers back
    std::span<uint8_t const> read_buf{packet};
    ProtocolHeader loaded1, loaded2;

    auto load1 = load(read_buf, &loaded1);
    EXPECT_TRUE(load1.has_value());

    auto load2 = load(read_buf.subspan(*load1), &loaded2);
    EXPECT_TRUE(load2.has_value());

    EXPECT_EQ(loaded1.type, 1U);
    EXPECT_EQ(loaded2.type, 2U);
}

//
// BufferSerializerBuilder tests
//

TEST(ieee_protocol_example, buffer_builder_append_fields)
{
    ProtocolHeader const header{
        .type = 0x42,
        .flags = 0x1234,
        .length = 500,
        .timestamp = 0xAABBCCDDEEFF0011ull,
    };

    // Create a buffer builder
    statusbar::BufferSerializerBuilderWithStorage<13> builder{};

    // Append each field using the builder
    builder.append(header.type.network_value())
        .append(header.flags.network_value())
        .append(header.length.network_value())
        .append(header.timestamp.network_value());

    // Check that the builder succeeded
    EXPECT_TRUE(builder);
    EXPECT_EQ(builder.get_span().size(), 13U);

    // Verify the data
    auto span = builder.get_span();
    EXPECT_EQ(span[0], 0x42);
    EXPECT_EQ(span[1], 0x12);
    EXPECT_EQ(span[2], 0x34);
}

TEST(ieee_protocol_example, buffer_builder_with_load_verification)
{
    ProtocolHeader const header{
        .type = 0x01,
        .flags = 0x0020,
        .length = 256,
        .timestamp = 0x123456789ABCDEF0ull,
    };

    // Create buffer builder with storage
    statusbar::BufferSerializerBuilderWithStorage<13> builder{};

    // Serialize field by field using append with chaining
    builder.append(header.type.network_value())
        .append(header.flags.network_value())
        .append(header.length.network_value())
        .append(header.timestamp.network_value());

    // Verify the builder succeeded
    EXPECT_TRUE(builder);

    // Verify total size
    EXPECT_EQ(builder.get_span().size(), 13U);

    // Deserialize and verify
    auto span = builder.get_span();
    ProtocolHeader loaded;
    auto load_result = load(span, &loaded);

    EXPECT_TRUE(load_result.has_value());
    EXPECT_EQ(loaded.type, header.type);
    EXPECT_EQ(loaded.flags, header.flags);
    EXPECT_EQ(loaded.length, header.length);
    EXPECT_EQ(loaded.timestamp, header.timestamp);
}

TEST(ieee_protocol_example, buffer_builder_round_trip)
{
    // Create a header
    ProtocolHeader const original{
        .type = 0xFF,
        .flags = 0xABCD,
        .length = 1024,
        .timestamp = 0xFEDCBA9876543210ull,
    };

    // Use BufferSerializerBuilder to serialize
    statusbar::BufferSerializerBuilderWithStorage<13> builder{};

    // Append fields
    builder.append(original.type.network_value())
        .append(original.flags.network_value())
        .append(original.length.network_value())
        .append(original.timestamp.network_value());

    EXPECT_TRUE(builder);

    // Load it back
    auto span = builder.get_span();
    ProtocolHeader loaded;
    auto load_result = load(span, &loaded);

    EXPECT_TRUE(load_result.has_value());
    EXPECT_EQ(loaded.type, original.type);
    EXPECT_EQ(loaded.flags, original.flags);
    EXPECT_EQ(loaded.length, original.length);
    EXPECT_EQ(loaded.timestamp, original.timestamp);
}

TEST(ieee_protocol_example, buffer_builder_multiple_headers)
{
    ProtocolHeader const header1{.type = 1, .flags = 0x0001, .length = 100, .timestamp = 1000};
    ProtocolHeader const header2{.type = 2, .flags = 0x0002, .length = 200, .timestamp = 2000};

    // Create buffer large enough for two headers
    statusbar::BufferSerializerBuilderWithStorage<26> builder{};

    // Append first header
    builder.append(header1.type.network_value())
        .append(header1.flags.network_value())
        .append(header1.length.network_value())
        .append(header1.timestamp.network_value());

    // Append second header
    builder.append(header2.type.network_value())
        .append(header2.flags.network_value())
        .append(header2.length.network_value())
        .append(header2.timestamp.network_value());

    EXPECT_TRUE(builder);

    // Verify total size
    EXPECT_EQ(builder.get_span().size(), 26U);

    // Load both headers back
    auto span = builder.get_span();
    ProtocolHeader loaded1, loaded2;

    auto load1 = load(span, &loaded1);
    EXPECT_TRUE(load1.has_value());

    auto load2 = load(span.subspan(*load1), &loaded2);
    EXPECT_TRUE(load2.has_value());

    EXPECT_EQ(loaded1.type, header1.type);
    EXPECT_EQ(loaded2.type, header2.type);
}

TEST(ieee_protocol_example, buffer_builder_insufficient_space)
{
    ProtocolHeader const header{
        .type = 0x01,
        .flags = 0x0020,
        .length = 100,
        .timestamp = 0x123456789ABCDEF0ull,
    };

    // Create buffer too small
    statusbar::BufferSerializerBuilderWithStorage<10> builder{};

    // Try to append all fields - should fail partway through
    builder.append(header.type.network_value())
        .append(header.flags.network_value())
        .append(header.length.network_value())
        .append(header.timestamp.network_value());

    // Builder should have an error
    EXPECT_FALSE(builder);
    EXPECT_EQ(builder.error(), BufferError::insufficient_space);
}

TEST(ieee_protocol_example, buffer_builder_chaining)
{
    ProtocolHeader const header{
        .type = 0xAA,
        .flags = 0xBBCC,
        .length = 0xDDEE,
        .timestamp = 0x1122334455667788ull,
    };

    // Create buffer builder
    statusbar::BufferSerializerBuilderWithStorage<13> builder{};

    // Use method chaining for fluent API
    builder.append(header.type.network_value())
        .append(header.flags.network_value())
        .append(header.length.network_value())
        .append(header.timestamp.network_value());

    EXPECT_TRUE(builder);
    EXPECT_EQ(builder.get_span().size(), 13U);

    // Verify the data
    auto span = builder.get_span();
    EXPECT_EQ(span[0], 0xAA);
    EXPECT_EQ(span[1], 0xBB);
    EXPECT_EQ(span[2], 0xCC);
}

TEST(ieee_protocol_example, buffer_builder_error_stops_further_appends)
{
    // Create buffer that's too small
    statusbar::BufferSerializerBuilderWithStorage<5> builder{};

    // Try to append fields - will fail after a few
    builder
        .append(static_cast<uint8_t>(0x01))     // 1 byte - succeeds
        .append(static_cast<uint16_t>(0x0203))  // 2 bytes - succeeds
        .append(static_cast<uint16_t>(0x0405))  // 2 bytes - succeeds (5 total)
        .append(static_cast<uint64_t>(0x06));   // 8 bytes - fails (would need 13 total)

    // Builder should have error
    EXPECT_FALSE(builder);
    EXPECT_EQ(builder.error(), BufferError::insufficient_space);

    // Should have written first 5 bytes before failing
    EXPECT_EQ(builder.get_span().size(), 5U);
}

TEST(ieee_protocol_example, buffer_builder_append_whole_header)
{
    ProtocolHeader const header{
        .type = 0x99,
        .flags = 0xABCD,
        .length = 2048,
        .timestamp = 0x0011223344556677ull,
    };

    // Create buffer builder
    statusbar::BufferSerializerBuilderWithStorage<13> builder{};

    // Append fields individually
    builder.append(header.type).append(header.flags).append(header.length).append(header.timestamp);

    EXPECT_TRUE(builder);
    EXPECT_EQ(builder.get_span().size(), 13U);

    // Verify by deserializing
    auto span = builder.get_span();
    ProtocolHeader loaded;
    auto load_result = load(span, &loaded);

    EXPECT_TRUE(load_result.has_value());
    EXPECT_EQ(loaded.type, header.type);
    EXPECT_EQ(loaded.flags, header.flags);
    EXPECT_EQ(loaded.length, header.length);
    EXPECT_EQ(loaded.timestamp, header.timestamp);
}

TEST(ieee_protocol_example, buffer_builder_append_multiple_whole_headers)
{
    ProtocolHeader const header1{.type = 10, .flags = 0x1111, .length = 111, .timestamp = 11111};
    ProtocolHeader const header2{.type = 20, .flags = 0x2222, .length = 222, .timestamp = 22222};

    // Create buffer large enough for two headers
    statusbar::BufferSerializerBuilderWithStorage<26> builder{};

    // Append both headers field by field
    builder.append(header1.type)
        .append(header1.flags)
        .append(header1.length)
        .append(header1.timestamp)
        .append(header2.type)
        .append(header2.flags)
        .append(header2.length)
        .append(header2.timestamp);

    EXPECT_TRUE(builder);
    EXPECT_EQ(builder.get_span().size(), 26U);

    // Load both headers back
    auto span = builder.get_span();
    ProtocolHeader loaded1, loaded2;

    auto load1 = load(span, &loaded1);
    EXPECT_TRUE(load1.has_value());

    auto load2 = load(span.subspan(*load1), &loaded2);
    EXPECT_TRUE(load2.has_value());

    EXPECT_EQ(loaded1.type, header1.type);
    EXPECT_EQ(loaded1.flags, header1.flags);
    EXPECT_EQ(loaded2.type, header2.type);
    EXPECT_EQ(loaded2.flags, header2.flags);
}

//
// BufferDeserializer tests
//

TEST(ieee_protocol_example, buffer_deserializer_parse_whole_header)
{
    // Network packet with big-endian bytes
    uint8_t const packet[13] = {
        0x42,  // type
        0x12,
        0x34,  // flags = 0x1234
        0x01,
        0xF4,  // length = 500 (0x01F4)
        0xAA,
        0xBB,
        0xCC,
        0xDD,
        0xEE,
        0xFF,
        0x00,
        0x11  // timestamp = 0xAABBCCDDEEFF0011
    };
    std::span<uint8_t const> buf{packet, 13};

    // Create a BufferDeserializer
    statusbar::BufferDeserializer deserializer{buf};

    // Parse the header field by field
    ProtocolHeader header;
    auto result1 = deserializer.parse(&header.type);
    auto result2 = deserializer.parse(&header.flags);
    auto result3 = deserializer.parse(&header.length);
    auto result4 = deserializer.parse(&header.timestamp);

    EXPECT_TRUE(result1 && result2 && result3 && result4);
    EXPECT_EQ(header.type, 0x42U);
    EXPECT_EQ(header.flags, 0x1234U);
    EXPECT_EQ(header.length, 500U);
    EXPECT_EQ(header.timestamp, 0xAABBCCDDEEFF0011ull);
}

TEST(ieee_protocol_example, buffer_deserializer_builder_parse_whole_header)
{
    // Network packet with big-endian bytes
    uint8_t const packet[13] = {
        0xFF,  // type
        0xAB,
        0xCD,  // flags = 0xABCD
        0x04,
        0x00,  // length = 1024 (0x0400)
        0xFE,
        0xDC,
        0xBA,
        0x98,
        0x76,
        0x54,
        0x32,
        0x10  // timestamp = 0xFEDCBA9876543210
    };
    std::span<uint8_t const> buf{packet, 13};

    // Create a BufferDeserializerBuilder
    statusbar::BufferDeserializerBuilder builder{buf};

    // Parse the header field by field using fluent API
    ProtocolHeader header;
    builder.parse(&header.type).parse(&header.flags).parse(&header.length).parse(&header.timestamp);

    EXPECT_TRUE(builder);
    EXPECT_EQ(header.type, 0xFFu);
    EXPECT_EQ(header.flags, 0xABCDu);
    EXPECT_EQ(header.length, 1024U);
    EXPECT_EQ(header.timestamp, 0xFEDCBA9876543210ull);
}

TEST(ieee_protocol_example, buffer_deserializer_insufficient_data)
{
    // Packet too short (only 10 bytes, need 13)
    uint8_t const packet[10] = {0x01, 0x00, 0x20, 0x01, 0x00, 0x12, 0x34, 0x56, 0x78, 0x9A};
    std::span<uint8_t const> buf{packet, 10};

    statusbar::BufferDeserializer deserializer{buf};

    ProtocolHeader header;
    auto result1 = deserializer.parse(&header.type);
    auto result2 = deserializer.parse(&header.flags);
    auto result3 = deserializer.parse(&header.length);
    auto result4 = deserializer.parse(&header.timestamp);

    // At least one should fail due to insufficient data
    EXPECT_FALSE(result1 && result2 && result3 && result4);
    EXPECT_EQ(result4.error(), BufferError::insufficient_data);
}

TEST(ieee_protocol_example, buffer_deserializer_builder_round_trip)
{
    // Create an original header
    ProtocolHeader const original{
        .type = 0x99,
        .flags = 0x5555,
        .length = 2048,
        .timestamp = 0x1122334455667788ull,
    };

    // Serialize using BufferSerializerBuilder
    statusbar::BufferSerializerBuilderWithStorage<13> builder{};
    builder.append(original.type).append(original.flags).append(original.length).append(original.timestamp);
    EXPECT_TRUE(builder);

    // Deserialize using BufferDeserializerBuilder
    auto span = builder.get_span();
    statusbar::BufferDeserializerBuilder parser{span};

    ProtocolHeader loaded;
    parser.parse(&loaded.type).parse(&loaded.flags).parse(&loaded.length).parse(&loaded.timestamp);

    EXPECT_TRUE(parser);
    EXPECT_EQ(loaded.type, original.type);
    EXPECT_EQ(loaded.flags, original.flags);
    EXPECT_EQ(loaded.length, original.length);
    EXPECT_EQ(loaded.timestamp, original.timestamp);
}

TEST(ieee_protocol_example, buffer_deserializer_can_parse_sufficient_data)
{
    // Network packet with big-endian bytes
    uint8_t const packet[13] = {0x42, 0x12, 0x34, 0x01, 0xF4, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};
    std::span<uint8_t const> buf{packet, 13};

    statusbar::BufferDeserializer deserializer{buf};

    // Check if buffer has enough space for the entire header
    EXPECT_TRUE(deserializer.available() >= ProtocolHeader::LENGTH);
}

TEST(ieee_protocol_example, buffer_deserializer_can_parse_insufficient_data)
{
    // Packet too short (only 10 bytes, need 13)
    uint8_t const packet[10] = {0x01, 0x00, 0x20, 0x01, 0x00, 0x12, 0x34, 0x56, 0x78, 0x9A};
    std::span<uint8_t const> buf{packet, 10};

    statusbar::BufferDeserializer deserializer{buf};

    // Try to parse all fields - should fail on timestamp
    ProtocolHeader header;
    auto result1 = deserializer.parse(&header.type);
    auto result2 = deserializer.parse(&header.flags);
    auto result3 = deserializer.parse(&header.length);
    auto result4 = deserializer.parse(&header.timestamp);

    // First 3 should succeed, last should fail
    EXPECT_TRUE(result1 && result2 && result3);
    EXPECT_FALSE(result4);
    EXPECT_EQ(result4.error(), BufferError::insufficient_data);
}

TEST(ieee_protocol_example, buffer_deserializer_parse_unchecked)
{
    // Network packet with big-endian bytes
    uint8_t const packet[13] = {
        0x77,  // type
        0xAA,
        0xBB,  // flags = 0xAABB
        0xCC,
        0xDD,  // length = 0xCCDD
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88  // timestamp = 0x1122334455667788
    };
    std::span<uint8_t const> buf{packet, 13};

    statusbar::BufferDeserializer deserializer{buf};

    // Check if buffer has enough space for the entire header
    ProtocolHeader header;
    EXPECT_TRUE(deserializer.available() >= ProtocolHeader::LENGTH);

    // Parse unchecked - we've already verified sufficient data
    deserializer.parse_unchecked(&header.type);
    deserializer.parse_unchecked(&header.flags);
    deserializer.parse_unchecked(&header.length);
    deserializer.parse_unchecked(&header.timestamp);

    EXPECT_EQ(header.type, 0x77U);
    EXPECT_EQ(header.flags, 0xAABBu);
    EXPECT_EQ(header.length, 0xCCDDu);
    EXPECT_EQ(header.timestamp, 0x1122334455667788ull);
}

TEST(ieee_protocol_example, buffer_deserializer_optimization_pattern)
{
    // Network packet with big-endian bytes
    uint8_t const packet[13] = {0x55, 0x11, 0x22, 0x33, 0x44, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};
    std::span<uint8_t const> buf{packet, 13};

    statusbar::BufferDeserializer deserializer{buf};

    ProtocolHeader header;

    // Optimization pattern: check total size first, then parse unchecked
    if (deserializer.available() >= ProtocolHeader::LENGTH) {
        // Fast path - no validation needed for individual fields
        deserializer.parse_unchecked(&header.type);
        deserializer.parse_unchecked(&header.flags);
        deserializer.parse_unchecked(&header.length);
        deserializer.parse_unchecked(&header.timestamp);

        EXPECT_EQ(header.type, 0x55U);
        EXPECT_EQ(header.flags, 0x1122U);
        EXPECT_EQ(header.length, 0x3344U);
        EXPECT_EQ(header.timestamp, 0xAABBCCDDEEFF0011ull);
    }
}

TEST(ieee_protocol_example, buffer_deserializer_builder_can_parse)
{
    // Network packet with big-endian bytes
    uint8_t const packet[13] = {0x42, 0x12, 0x34, 0x01, 0xF4, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};
    std::span<uint8_t const> buf{packet, 13};

    statusbar::BufferDeserializerBuilder builder{buf};

    // Check that buffer size is sufficient
    EXPECT_TRUE(buf.size() >= ProtocolHeader::LENGTH);
}

TEST(ieee_protocol_example, buffer_deserializer_builder_parse_unchecked)
{
    // Network packet with big-endian bytes
    uint8_t const packet[13] = {0x88, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    std::span<uint8_t const> buf{packet, 13};

    statusbar::BufferDeserializerBuilder builder{buf};

    ProtocolHeader header;

    // Check that buffer size is sufficient
    EXPECT_TRUE(buf.size() >= ProtocolHeader::LENGTH);

    // Parse unchecked
    builder.parse_unchecked(&header.type)
        .parse_unchecked(&header.flags)
        .parse_unchecked(&header.length)
        .parse_unchecked(&header.timestamp);

    EXPECT_TRUE(builder);
    EXPECT_EQ(header.type, 0x88U);
    EXPECT_EQ(header.flags, 0xCCDDu);
    EXPECT_EQ(header.length, 0xEEFFu);
    EXPECT_EQ(header.timestamp, 0x1122334455667788ull);
}

//
// Round-Trip Tests: Serialize then Deserialize
//

TEST(ieee_protocol_example, round_trip_buffer_serialize_deserialize)
{
    // Create an original header with test values
    ProtocolHeader const original{
        .type = octet_t{0x42},
        .flags = doublet_t{0x1234},
        .length = doublet_t{0x5678},
        .timestamp = octlet_t{0x0102030405060708ull},
    };

    // Serialize to buffer field by field
    statusbar::BufferSerializerBuilderWithStorage<13> builder;

    builder.append(original.type).append(original.flags).append(original.length).append(original.timestamp);
    EXPECT_TRUE(builder.status());
    EXPECT_EQ(builder.position(), 13);

    // Deserialize back using BufferDeserializer
    std::span<uint8_t const> const serialized_data = builder.get_span();
    statusbar::BufferDeserializer deserializer{serialized_data};

    ProtocolHeader parsed;
    auto parse_status1 = deserializer.parse(&parsed.type);
    auto parse_status2 = deserializer.parse(&parsed.flags);
    auto parse_status3 = deserializer.parse(&parsed.length);
    auto parse_status4 = deserializer.parse(&parsed.timestamp);

    EXPECT_TRUE(parse_status1 && parse_status2 && parse_status3 && parse_status4);
    EXPECT_EQ(parsed.type, original.type);
    EXPECT_EQ(parsed.flags, original.flags);
    EXPECT_EQ(parsed.length, original.length);
    EXPECT_EQ(parsed.timestamp, original.timestamp);
}

TEST(ieee_protocol_example, round_trip_buffer_builder_serialize_deserialize)
{
    // Create an original header with different test values
    ProtocolHeader const original{
        .type = octet_t{0xFF},
        .flags = doublet_t{0xABCD},
        .length = doublet_t{0x9999},
        .timestamp = octlet_t{0xFEDCBA9876543210ull},
    };

    // Serialize using BufferSerializerBuilder
    statusbar::BufferSerializerBuilderWithStorage<13> builder{};

    builder.append(original.type).append(original.flags).append(original.length).append(original.timestamp);
    EXPECT_TRUE(builder);
    EXPECT_EQ(builder.get_span().size(), 13);

    // Deserialize using BufferDeserializerBuilder
    std::span<uint8_t const> const serialized_data = builder.get_span();
    statusbar::BufferDeserializerBuilder deser_builder{serialized_data};

    ProtocolHeader parsed;
    deser_builder.parse(&parsed.type).parse(&parsed.flags).parse(&parsed.length).parse(&parsed.timestamp);

    EXPECT_TRUE(deser_builder);
    EXPECT_EQ(parsed.type, original.type);
    EXPECT_EQ(parsed.flags, original.flags);
    EXPECT_EQ(parsed.length, original.length);
    EXPECT_EQ(parsed.timestamp, original.timestamp);
}

TEST(ieee_protocol_example, round_trip_multiple_headers)
{
    // Create multiple headers
    ProtocolHeader const headers[3] = {
        {.type = octet_t{0x01},
         .flags = doublet_t{0x1111},
         .length = doublet_t{0x2222},
         .timestamp = octlet_t{0x0000000000000001ull}},
        {.type = octet_t{0x02},
         .flags = doublet_t{0x3333},
         .length = doublet_t{0x4444},
         .timestamp = octlet_t{0x0000000000000002ull}},
        {.type = octet_t{0x03},
         .flags = doublet_t{0x5555},
         .length = doublet_t{0x6666},
         .timestamp = octlet_t{0x0000000000000003ull}},
    };

    // Serialize all headers
    statusbar::BufferSerializerBuilderWithStorage<39> builder{};  // 3 headers * 13 bytes

    for (auto const& h : headers) {
        builder.append(h.type).append(h.flags).append(h.length).append(h.timestamp);
    }

    EXPECT_TRUE(builder);
    EXPECT_EQ(builder.get_span().size(), 39);

    // Deserialize all headers back
    std::span<uint8_t const> const serialized_data = builder.get_span();
    statusbar::BufferDeserializer deserializer{serialized_data};

    ProtocolHeader parsed[3];
    for (auto& p : parsed) {
        auto status1 = deserializer.parse(&p.type);
        auto status2 = deserializer.parse(&p.flags);
        auto status3 = deserializer.parse(&p.length);
        auto status4 = deserializer.parse(&p.timestamp);
        EXPECT_TRUE(status1 && status2 && status3 && status4);
    }

    // Verify all headers match
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(parsed[i].type, headers[i].type);
        EXPECT_EQ(parsed[i].flags, headers[i].flags);
        EXPECT_EQ(parsed[i].length, headers[i].length);
        EXPECT_EQ(parsed[i].timestamp, headers[i].timestamp);
    }
}

TEST(ieee_protocol_example, round_trip_with_unchecked_optimization)
{
    // Network packet with big-endian bytes
    uint8_t const packet[13] = {
        0xAA,  // type
        0xBB,
        0xCC,  // flags = 0xBBCC
        0xDD,
        0xEE,  // length = 0xDDEE
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88  // timestamp = 0x1122334455667788
    };
    std::span<uint8_t const> buf{packet, 13};

    statusbar::BufferDeserializer deserializer{buf};

    // Check if buffer has enough space for the entire header
    ProtocolHeader header;
    EXPECT_TRUE(deserializer.available() >= ProtocolHeader::LENGTH);

    // Parse unchecked - we've already verified sufficient data
    deserializer.parse_unchecked(&header.type);
    deserializer.parse_unchecked(&header.flags);
    deserializer.parse_unchecked(&header.length);
    deserializer.parse_unchecked(&header.timestamp);

    EXPECT_EQ(header.type, 0xAAu);
    EXPECT_EQ(header.flags, 0xBBCCu);
    EXPECT_EQ(header.length, 0xDDEEu);
    EXPECT_EQ(header.timestamp, 0x1122334455667788ull);
}

//
// Test driver main function
//

TEST_MAIN(statusbar_ieee, ieee_protocol_example_test)