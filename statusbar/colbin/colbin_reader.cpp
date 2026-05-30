// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/colbin/colbin_reader.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstring>

#include <sys/mman.h>
#include <sys/stat.h>

namespace statusbar::colbin {

namespace {

constexpr uint32_t OFFSET_MAGIC = 0;
constexpr uint32_t OFFSET_VERSION = 8;
constexpr uint32_t OFFSET_ENDIAN = 12;
constexpr uint32_t OFFSET_HEADER_TOTAL = 16;
constexpr uint32_t OFFSET_ROW_SIZE = 20;
constexpr uint32_t OFFSET_COMMITTED_ROWS = 24;
constexpr uint32_t OFFSET_SCHEMA_LEN = 32;

auto io_failure() noexcept -> std::error_code
{
    return {errno, std::generic_category()};
}

}  // namespace

auto Reader::open(std::filesystem::path const& path) -> StatusValue<Reader>
{
    int const fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return std::unexpected{io_failure()};
    }
    struct stat st{};
    if (::fstat(fd, &st) < 0) {
        auto const ec = io_failure();
        ::close(fd);
        return std::unexpected{ec};
    }
    uint64_t const file_size = static_cast<uint64_t>(st.st_size);
    if (file_size < HEADER_PREAMBLE_BYTES) {
        ::close(fd);
        return std::unexpected{make_error_code(ColbinError::truncated_header)};
    }

    void* mapped = ::mmap(nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        auto const ec = io_failure();
        ::close(fd);
        return std::unexpected{ec};
    }
    auto const* base = static_cast<uint8_t const*>(mapped);

    if (std::memcmp(base + OFFSET_MAGIC, MAGIC.data(), MAGIC.size()) != 0) {
        ::munmap(mapped, file_size);
        ::close(fd);
        return std::unexpected{make_error_code(ColbinError::bad_magic)};
    }
    uint32_t version = 0;
    std::memcpy(&version, base + OFFSET_VERSION, sizeof(version));
    if (version != FORMAT_VERSION) {
        ::munmap(mapped, file_size);
        ::close(fd);
        return std::unexpected{make_error_code(ColbinError::unsupported_version)};
    }
    uint32_t endian = 0;
    std::memcpy(&endian, base + OFFSET_ENDIAN, sizeof(endian));
    if (endian != ENDIAN_MARKER) {
        ::munmap(mapped, file_size);
        ::close(fd);
        return std::unexpected{make_error_code(ColbinError::endian_mismatch)};
    }
    uint32_t header_total = 0;
    std::memcpy(&header_total, base + OFFSET_HEADER_TOTAL, sizeof(header_total));
    uint32_t row_size = 0;
    std::memcpy(&row_size, base + OFFSET_ROW_SIZE, sizeof(row_size));
    uint32_t schema_len = 0;
    std::memcpy(&schema_len, base + OFFSET_SCHEMA_LEN, sizeof(schema_len));
    // Compute the minimum header size in uint64 so a hostile schema_len near
    // UINT32_MAX cannot wrap `HEADER_PREAMBLE_BYTES + schema_len` to 0 and
    // bypass the bound. The widened value is also bounded against file_size.
    uint64_t const header_required = static_cast<uint64_t>(HEADER_PREAMBLE_BYTES) + schema_len;
    if (header_required > file_size || header_total < header_required || header_total > file_size) {
        ::munmap(mapped, file_size);
        ::close(fd);
        return std::unexpected{make_error_code(ColbinError::truncated_header)};
    }
    if (row_size == 0) {
        ::munmap(mapped, file_size);
        ::close(fd);
        return std::unexpected{make_error_code(ColbinError::invalid_schema)};
    }
    // Reinterpreting memory-mapped file bytes as char — mmap returns void*, the cast to char* is well-defined for byte access.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::string_view const schema_text{reinterpret_cast<char const*>(base + HEADER_PREAMBLE_BYTES), schema_len};
    auto cols = parse_schema(schema_text);
    if (!cols) {
        ::munmap(mapped, file_size);
        ::close(fd);
        return std::unexpected{cols.error()};
    }
    auto resolved = resolve_schema(*cols);
    if (resolved.row_size != row_size) {
        ::munmap(mapped, file_size);
        ::close(fd);
        return std::unexpected{make_error_code(ColbinError::schema_row_size_mismatch)};
    }

    Reader r;
    r.fd_ = fd;
    r.base_ = base;
    r.mapped_size_ = file_size;
    r.header_total_ = header_total;
    r.row_size_ = row_size;
    // Physical capacity of the mmap: the file-stored `committed_rows` field
    // is clamped to this in row_count() so multiplications by row_size_
    // cannot exceed the mapped region.
    r.max_rows_ = row_size > 0 ? (file_size - header_total) / row_size : 0;
    r.schema_ = std::move(resolved.columns);
    return r;
}

Reader::Reader(Reader&& other) noexcept
    : fd_(other.fd_)
    , base_(other.base_)
    , mapped_size_(other.mapped_size_)
    , header_total_(other.header_total_)
    , row_size_(other.row_size_)
    , max_rows_(other.max_rows_)
    , schema_(std::move(other.schema_))
{
    other.fd_ = -1;
    other.base_ = nullptr;
    other.mapped_size_ = 0;
    other.max_rows_ = 0;
}

auto Reader::operator=(Reader&& other) noexcept -> Reader&
{
    if (this != &other) {
        destroy();
        fd_ = other.fd_;
        base_ = other.base_;
        mapped_size_ = other.mapped_size_;
        header_total_ = other.header_total_;
        row_size_ = other.row_size_;
        max_rows_ = other.max_rows_;
        schema_ = std::move(other.schema_);
        other.fd_ = -1;
        other.base_ = nullptr;
        other.mapped_size_ = 0;
        other.max_rows_ = 0;
    }
    return *this;
}

Reader::~Reader() noexcept
{
    destroy();
}

void Reader::destroy() noexcept
{
    if (base_ != nullptr) {
        ::munmap(const_cast<uint8_t*>(base_), mapped_size_);
        base_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    mapped_size_ = 0;
}

auto Reader::row_count() const noexcept -> uint64_t
{
    if (base_ == nullptr) {
        return 0;
    }
    // Aligned 8-byte loads are atomic on x86_64 / aarch64; pair the
    // memcpy with an acquire fence so we match the writer's release
    // store of committed_rows.
    uint64_t v = 0;
    std::memcpy(&v, base_ + OFFSET_COMMITTED_ROWS, sizeof(v));
    std::atomic_thread_fence(std::memory_order_acquire);
    // The field is file-controlled and could be set to anything by a hostile
    // writer (or a buggy one that crashed mid-grow). Clamp to the physical
    // capacity of the mmap so row(i) and all_rows() cannot walk past the
    // mapped region.
    return v <= max_rows_ ? v : max_rows_;
}

auto Reader::row(uint64_t index) const noexcept -> std::span<uint8_t const>
{
    if (index >= row_count()) {
        return {};
    }
    return {base_ + header_total_ + (index * row_size_), row_size_};
}

auto Reader::all_rows() const noexcept -> std::span<uint8_t const>
{
    return {base_ + header_total_, row_count() * row_size_};
}

}  // namespace statusbar::colbin
