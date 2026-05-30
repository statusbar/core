#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/colbin/colbin.hpp"
#include "statusbar/status/status.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace statusbar::colbin {

/// Read-only mmap view over a .colbin file. Zero-copy: returned spans
/// reference the mapped region and remain valid for the lifetime of
/// the Reader. Movable but not copyable.
class Reader
{
  public:
    [[nodiscard]] static auto open(std::filesystem::path const& path) -> StatusValue<Reader>;

    Reader(Reader const&) = delete;
    auto operator=(Reader const&) -> Reader& = delete;
    Reader(Reader&&) noexcept;
    auto operator=(Reader&&) noexcept -> Reader&;
    ~Reader() noexcept;

    /// Number of committed rows (as published by the writer's last
    /// `commit()`). Reads from the header field, so this reflects the
    /// latest atomic store from the writer.
    [[nodiscard]] auto row_count() const noexcept -> uint64_t;

    [[nodiscard]] auto row_size() const noexcept -> uint32_t { return row_size_; }
    [[nodiscard]] auto header_size() const noexcept -> uint32_t { return header_total_; }
    [[nodiscard]] auto schema() const noexcept -> std::span<ResolvedColumn const> { return schema_; }

    /// Zero-copy view of a single row at `index`. Returns an empty span
    /// if `index >= row_count()`.
    [[nodiscard]] auto row(uint64_t index) const noexcept -> std::span<uint8_t const>;

    /// Zero-copy view of every committed row, contiguous in memory.
    /// Length is `row_count() * row_size()`.
    [[nodiscard]] auto all_rows() const noexcept -> std::span<uint8_t const>;

  private:
    Reader() = default;
    void destroy() noexcept;

    int fd_{-1};
    uint8_t const* base_{nullptr};
    uint64_t mapped_size_{0};
    uint32_t header_total_{0};
    uint32_t row_size_{0};
    // Maximum row count the mmap can physically hold. Used to clamp the
    // file-controlled `committed_rows` field so a hostile file cannot make
    // row_count() * row_size_ exceed the mapped region.
    uint64_t max_rows_{0};
    std::vector<ResolvedColumn> schema_;
};

}  // namespace statusbar::colbin
