#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/status/status.hpp"

#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>

namespace statusbar::csv {

/// RFC 4180 CSV writer. Wraps a stdio FILE*; writes the header row in the
/// constructor. Each `write_row` call emits one data row, escaping any
/// field that contains comma, double-quote, CR, or LF by wrapping in
/// double-quotes and doubling internal double-quotes.
///
/// Construction throws `std::system_error` on open or header-write
/// failure. This is the one exception to the project's `Status`-based
/// error convention: the class is intentionally non-movable (the
/// owned FILE* must stay at a stable address while writes are in
/// progress), which makes a `StatusValue<CsvWriter>` factory pattern
/// awkward. Callers that prefer `Status` should wrap construction in
/// try/catch.
class CsvWriter
{
  public:
    /// Open `path` for writing (binary mode). Throws std::system_error on
    /// open or header-write failure. Header is written immediately.
    CsvWriter(std::string const& path, std::span<std::string_view const> header);
    ~CsvWriter() noexcept;

    CsvWriter(CsvWriter const&) = delete;
    auto operator=(CsvWriter const&) -> CsvWriter& = delete;
    CsvWriter(CsvWriter&&) noexcept = delete;
    auto operator=(CsvWriter&&) noexcept -> CsvWriter& = delete;

    /// Write one data row. Returns failure on write error; leaves the
    /// file in an unspecified state (callers should stop on first error).
    auto write_row(std::span<std::string_view const> fields) -> Status;

    /// Flush stdio's buffer to the OS (does not fsync). Returns failure
    /// on write error. Intended for streaming writers that want recent
    /// rows to survive an unclean process exit.
    auto flush() -> Status;

    /// Number of data rows written (excludes header).
    [[nodiscard]] auto row_count() const noexcept -> uint64_t { return row_count_; }

  private:
    std::FILE* fp_{nullptr};
    uint64_t row_count_{0};
};

}  // namespace statusbar::csv
