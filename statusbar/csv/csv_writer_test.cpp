// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/csv/csv_writer.hpp"

#include "statusbar/test/test.hpp"

#include <unistd.h>

#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

using namespace statusbar;

namespace {

[[nodiscard]] auto read_file(std::string const& path) -> std::string
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

[[nodiscard]] auto temp_path(std::string_view tag) -> std::string
{
    std::string p = "/tmp/csv_writer_test_";
    p += tag;
    p += "_";
    p += std::to_string(::getpid());
    p += ".csv";
    return p;
}

}  // namespace

TEST(csv_writer_basic, writes_header_and_row)
{
    auto const path = temp_path("basic");
    {
        std::array<std::string_view, 3> const header{"a", "b", "c"};
        csv::CsvWriter w{path, header};
        std::array<std::string_view, 3> const row{"1", "2", "3"};
        auto const st = w.write_row(row);
        EXPECT_TRUE(st.has_value());
        EXPECT_EQ(w.row_count(), uint64_t{1});
    }
    EXPECT_EQ(read_file(path), std::string{"a,b,c\r\n1,2,3\r\n"});
    std::remove(path.c_str());
}

TEST(csv_writer_escape, comma_triggers_quoting)
{
    auto const path = temp_path("comma");
    {
        std::array<std::string_view, 2> const header{"a", "b"};
        csv::CsvWriter w{path, header};
        std::array<std::string_view, 2> const row{"hi,there", "ok"};
        (void)w.write_row(row);
    }
    EXPECT_EQ(read_file(path), std::string{"a,b\r\n\"hi,there\",ok\r\n"});
    std::remove(path.c_str());
}

TEST(csv_writer_escape, double_quote_is_doubled)
{
    auto const path = temp_path("quote");
    {
        std::array<std::string_view, 1> const header{"x"};
        csv::CsvWriter w{path, header};
        std::string_view const f{"he said \"hi\""};
        std::array<std::string_view, 1> const row{f};
        (void)w.write_row(row);
    }
    EXPECT_EQ(read_file(path), std::string{"x\r\n\"he said \"\"hi\"\"\"\r\n"});
    std::remove(path.c_str());
}

TEST(csv_writer_escape, newline_triggers_quoting)
{
    auto const path = temp_path("lf");
    {
        std::array<std::string_view, 1> const header{"x"};
        csv::CsvWriter w{path, header};
        std::string_view const f{"line1\nline2"};
        std::array<std::string_view, 1> const row{f};
        (void)w.write_row(row);
    }
    EXPECT_EQ(read_file(path), std::string{"x\r\n\"line1\nline2\"\r\n"});
    std::remove(path.c_str());
}

TEST(csv_writer_escape, carriage_return_triggers_quoting)
{
    auto const path = temp_path("cr");
    {
        std::array<std::string_view, 1> const header{"x"};
        csv::CsvWriter w{path, header};
        std::string_view const f{"a\rb"};
        std::array<std::string_view, 1> const row{f};
        (void)w.write_row(row);
    }
    EXPECT_EQ(read_file(path), std::string{"x\r\n\"a\rb\"\r\n"});
    std::remove(path.c_str());
}

TEST(csv_writer_basic, empty_field_is_empty_not_quoted)
{
    auto const path = temp_path("empty");
    {
        std::array<std::string_view, 2> const header{"a", "b"};
        csv::CsvWriter w{path, header};
        std::array<std::string_view, 2> const row{"", "ok"};
        (void)w.write_row(row);
    }
    EXPECT_EQ(read_file(path), std::string{"a,b\r\n,ok\r\n"});
    std::remove(path.c_str());
}

TEST(csv_writer_escape, formula_injection_is_prefixed_with_apostrophe)
{
    auto const path = temp_path("formula");
    {
        std::array<std::string_view, 1> const header{"cmd"};
        csv::CsvWriter w{path, header};
        std::array<std::string_view, 5> const rows{
            "=1+1",        // = trigger
            "+sum(A1)",    // + trigger
            "-2",          // - trigger
            "@import",     // @ trigger
            "\tDEFAULT"};  // \t trigger
        for (auto f : rows) {
            std::array<std::string_view, 1> const row{f};
            (void)w.write_row(row);
        }
    }
    // Each formula-trigger row is quoted AND prefixed with a single
    // apostrophe inside the quotes.
    EXPECT_EQ(
        read_file(path),
        std::string{"cmd\r\n"
                    "\"'=1+1\"\r\n"
                    "\"'+sum(A1)\"\r\n"
                    "\"'-2\"\r\n"
                    "\"'@import\"\r\n"
                    "\"'\tDEFAULT\"\r\n"});
    std::remove(path.c_str());
}

TEST(csv_writer_escape, safe_leading_chars_are_unquoted)
{
    // A leading letter / digit / punctuation that ISN'T a formula
    // trigger is written without quoting (status quo, regression guard).
    auto const path = temp_path("safe");
    {
        std::array<std::string_view, 1> const header{"v"};
        csv::CsvWriter w{path, header};
        std::array<std::string_view, 3> const rows{"hello", "42", "1+1"};
        for (auto f : rows) {
            std::array<std::string_view, 1> const row{f};
            (void)w.write_row(row);
        }
    }
    EXPECT_EQ(read_file(path), std::string{"v\r\nhello\r\n42\r\n1+1\r\n"});
    std::remove(path.c_str());
}

TEST(csv_writer_basic, ctor_throws_on_open_failure)
{
    std::array<std::string_view, 1> const header{"x"};
    bool caught = false;
    try {
        csv::CsvWriter w{"/this/path/does/not/exist/owlm.csv", header};
    } catch (std::system_error const&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

TEST_MAIN(statusbar_csv, csv_writer_test)
