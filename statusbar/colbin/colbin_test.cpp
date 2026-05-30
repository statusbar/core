// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/colbin/colbin.hpp"

#include "statusbar/colbin/colbin_reader.hpp"
#include "statusbar/colbin/colbin_writer.hpp"
#include "statusbar/test/test.hpp"

#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace cb = statusbar::colbin;

namespace {

struct ScratchPath
{
    std::filesystem::path path;
    ScratchPath()
        : path(std::filesystem::temp_directory_path() / make_name())
    {}
    ~ScratchPath() noexcept
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    ScratchPath(ScratchPath const&) = delete;
    auto operator=(ScratchPath const&) -> ScratchPath& = delete;

    static auto make_name() -> std::string
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "colbin_test_%d_%d.colbin", ::getpid(), std::rand());
        return buf;
    }
};

auto sample_schema() -> std::vector<cb::ColumnSpec>
{
    return {
        {"rx_gptp_ns", cb::TypeCode::i64},
        {"presentation_time_ns", cb::TypeCode::i64},
        {"latency_ns", cb::TypeCode::i64},
        {"sender_eui64", cb::TypeCode::u64},
        {"sequence", cb::TypeCode::u32},
        {"interval_us", cb::TypeCode::u32},
        {"role", cb::TypeCode::u8},
    };
}

// Matches the alignment-padded row layout the sample schema
// resolves to. Kept in lock-step with `sample_schema()`.
#pragma pack(push, 1)
struct SampleRow
{
    int64_t rx_gptp_ns;
    int64_t presentation_time_ns;
    int64_t latency_ns;
    uint64_t sender_eui64;
    uint32_t sequence;
    uint32_t interval_us;
    uint8_t role;
    uint8_t pad[7];
};
#pragma pack(pop)
static_assert(sizeof(SampleRow) == 48);

}  // namespace

TEST(colbin_schema, resolve_sample_schema)
{
    auto cols = sample_schema();
    auto r = cb::resolve_schema(cols);
    EXPECT_EQ(r.columns.size(), size_t{7});
    EXPECT_EQ(r.columns[0].offset, uint32_t{0});
    EXPECT_EQ(r.columns[3].offset, uint32_t{24});  // u64 at offset 24
    EXPECT_EQ(r.columns[4].offset, uint32_t{32});  // u32 sequence
    EXPECT_EQ(r.columns[5].offset, uint32_t{36});  // u32 interval_us
    EXPECT_EQ(r.columns[6].offset, uint32_t{40});  // u8 role
    EXPECT_EQ(r.row_size, uint32_t{48});
    EXPECT_EQ(r.row_align, uint32_t{8});
}

TEST(colbin_schema, mixed_alignment_inserts_padding)
{
    std::vector<cb::ColumnSpec> cols = {
        {"a", cb::TypeCode::u8},
        {"b", cb::TypeCode::u32},  // expect offset 4 (3 bytes of pad after a)
        {"c", cb::TypeCode::u16},  // offset 8
        {"d", cb::TypeCode::u64},  // bumped to 16
    };
    auto r = cb::resolve_schema(cols);
    EXPECT_EQ(r.columns[0].offset, uint32_t{0});
    EXPECT_EQ(r.columns[1].offset, uint32_t{4});
    EXPECT_EQ(r.columns[2].offset, uint32_t{8});
    EXPECT_EQ(r.columns[3].offset, uint32_t{16});
    EXPECT_EQ(r.row_size, uint32_t{24});  // 16 + 8 = 24, already 8-aligned
}

TEST(colbin_schema, serialize_parse_round_trip)
{
    auto cols = sample_schema();
    auto text = cb::serialize_schema(cols);
    EXPECT_EQ(
        text,
        std::string{"rx_gptp_ns:i64,presentation_time_ns:i64,latency_ns:i64,"
                    "sender_eui64:u64,sequence:u32,interval_us:u32,role:u8"});
    auto parsed = cb::parse_schema(text);
    EXPECT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->size(), cols.size());
    for (size_t i = 0; i < cols.size(); ++i) {
        EXPECT_EQ((*parsed)[i].name, cols[i].name);
        EXPECT_EQ(static_cast<int>((*parsed)[i].type), static_cast<int>(cols[i].type));
    }
}

TEST(colbin_schema, parse_rejects_malformed)
{
    EXPECT_FALSE(cb::parse_schema("noColon").has_value());
    EXPECT_FALSE(cb::parse_schema("name:bogus_type").has_value());
    EXPECT_FALSE(cb::parse_schema(":i64").has_value());
    EXPECT_FALSE(cb::parse_schema("1bad:i64").has_value());  // name starts with digit
}

TEST(colbin_writer_reader, append_and_read_back)
{
    ScratchPath sp;
    auto cols = sample_schema();
    {
        auto w_or = cb::Writer::create(sp.path, cols);
        EXPECT_TRUE(w_or.has_value());
        auto w = std::move(*w_or);
        EXPECT_EQ(w.row_size(), uint32_t{48});

        for (uint32_t i = 0; i < 100; ++i) {
            SampleRow row{};
            row.rx_gptp_ns = 1'000'000'000LL + i * 1'000'000LL;
            row.presentation_time_ns = row.rx_gptp_ns - 20;
            row.latency_ns = 1'000'000LL + i;
            row.sender_eui64 = 0x123456789abcdef0ULL;
            row.sequence = i;
            row.interval_us = 1000;
            row.role = 2;  // remote_primary
            auto s = w.write_row(std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(&row), sizeof(row)});
            EXPECT_TRUE(s.has_value());
        }
        auto cs = w.commit();
        EXPECT_TRUE(cs.has_value());
        // writer goes out of scope -> destructor truncates + closes
    }

    auto r_or = cb::Reader::open(sp.path);
    EXPECT_TRUE(r_or.has_value());
    auto const& r = *r_or;
    EXPECT_EQ(r.row_count(), uint64_t{100});
    EXPECT_EQ(r.row_size(), uint32_t{48});
    EXPECT_EQ(r.schema().size(), size_t{7});
    EXPECT_EQ(r.schema()[0].name, std::string{"rx_gptp_ns"});

    for (uint64_t i = 0; i < 100; ++i) {
        auto bytes = r.row(i);
        EXPECT_EQ(bytes.size(), size_t{48});
        SampleRow row{};
        std::memcpy(&row, bytes.data(), sizeof(row));
        EXPECT_EQ(row.sequence, static_cast<uint32_t>(i));
        EXPECT_EQ(row.rx_gptp_ns, 1'000'000'000LL + static_cast<int64_t>(i) * 1'000'000LL);
        EXPECT_EQ(row.latency_ns, 1'000'000LL + static_cast<int64_t>(i));
    }
}

TEST(colbin_writer_reader, grow_across_mremap_boundary)
{
    // Force the writer to grow at least once by setting a tiny initial
    // capacity. We push enough rows to overflow the initial mmap region
    // by more than 2x.
    ScratchPath sp;
    auto cols = sample_schema();
    cb::WriterConfig cfg;
    cfg.initial_capacity_bytes = 8192;  // 8 KiB; ~170 rows of 48 B fit
    cfg.max_capacity_bytes = 4ULL * 1024 * 1024;

    uint32_t const n_rows = 1000;  // exceeds initial capacity multiple times
    {
        auto w_or = cb::Writer::create(sp.path, cols, cfg);
        EXPECT_TRUE(w_or.has_value());
        auto w = std::move(*w_or);
        for (uint32_t i = 0; i < n_rows; ++i) {
            SampleRow row{};
            row.sequence = i;
            row.role = 2;
            auto s = w.write_row(std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(&row), sizeof(row)});
            EXPECT_TRUE(s.has_value());
        }
        (void)w.commit();
    }

    auto r_or = cb::Reader::open(sp.path);
    EXPECT_TRUE(r_or.has_value());
    EXPECT_EQ(r_or->row_count(), static_cast<uint64_t>(n_rows));
    SampleRow last{};
    std::memcpy(&last, r_or->row(n_rows - 1).data(), sizeof(last));
    EXPECT_EQ(last.sequence, n_rows - 1);
}

TEST(colbin_writer_reader, reopen_appends_after_commit)
{
    ScratchPath sp;
    auto cols = sample_schema();
    {
        auto w_or = cb::Writer::create(sp.path, cols);
        EXPECT_TRUE(w_or.has_value());
        auto w = std::move(*w_or);
        for (uint32_t i = 0; i < 10; ++i) {
            SampleRow row{};
            row.sequence = i;
            (void)w.write_row(std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(&row), sizeof(row)});
        }
        (void)w.commit();
    }
    {
        auto w_or = cb::Writer::create(sp.path, cols);
        EXPECT_TRUE(w_or.has_value());
        auto w = std::move(*w_or);
        EXPECT_EQ(w.rows_written(), uint64_t{10});
        for (uint32_t i = 10; i < 20; ++i) {
            SampleRow row{};
            row.sequence = i;
            (void)w.write_row(std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(&row), sizeof(row)});
        }
        (void)w.commit();
    }
    auto r_or = cb::Reader::open(sp.path);
    EXPECT_TRUE(r_or.has_value());
    EXPECT_EQ(r_or->row_count(), uint64_t{20});
    SampleRow last{};
    std::memcpy(&last, r_or->row(19).data(), sizeof(last));
    EXPECT_EQ(last.sequence, uint32_t{19});
}

TEST(colbin_writer_reader, reject_schema_mismatch_on_reopen)
{
    ScratchPath sp;
    {
        auto cols = sample_schema();
        auto w_or = cb::Writer::create(sp.path, cols);
        EXPECT_TRUE(w_or.has_value());
    }
    // Try to reopen with a different schema → schema_row_size_mismatch
    std::vector<cb::ColumnSpec> different = {
        {"x", cb::TypeCode::u32},
        {"y", cb::TypeCode::u32},
    };
    auto w_or = cb::Writer::create(sp.path, different);
    EXPECT_FALSE(w_or.has_value());
}

TEST(colbin_reader, reject_bad_magic)
{
    ScratchPath sp;
    // Write a file that's the right size but with junk magic.
    std::ofstream f(sp.path, std::ios::binary);
    std::array<char, 256> junk{};
    f.write(junk.data(), junk.size());
    f.close();
    auto r_or = cb::Reader::open(sp.path);
    EXPECT_FALSE(r_or.has_value());
}

TEST_MAIN(statusbar_colbin, colbin_test)
