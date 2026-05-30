// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/colbin/colbin_writer.hpp"

#include "statusbar/colbin/colbin.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>

#include <sys/mman.h>
#include <sys/stat.h>

namespace statusbar::colbin {

namespace {

// Header field offsets — see colbin.hpp for the layout.
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

auto page_size_bytes() noexcept -> uint64_t
{
    long const ps = ::sysconf(_SC_PAGESIZE);
    return ps > 0 ? static_cast<uint64_t>(ps) : 4096;
}

auto round_up(uint64_t x, uint64_t a) noexcept -> uint64_t
{
    return (x + a - 1) & ~(a - 1);
}

void store_u32_le(uint8_t* p, uint32_t v) noexcept
{
    std::memcpy(p, &v, sizeof(v));
}

void store_u64_le_atomic(uint8_t* p, uint64_t v) noexcept
{
    // x86_64 / aarch64 guarantee atomicity of aligned 8-byte stores;
    // atomic_ref makes that contract explicit for thread-sanitizer.
    // Reinterpreting memory-mapped file bytes as uint64_t for atomic store — the mapping is 8-byte aligned at this offset by construction.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::atomic_ref<uint64_t> const ref{*reinterpret_cast<uint64_t*>(p)};
    ref.store(v, std::memory_order_release);
}

auto write_full(int fd, void const* buf, size_t n) noexcept -> Status
{
    auto const* p = static_cast<uint8_t const*>(buf);
    while (n > 0) {
        ssize_t const w = ::write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return failure(io_failure());
        }
        p += w;
        n -= static_cast<size_t>(w);
    }
    return success();
}

}  // namespace

auto Writer::create(std::filesystem::path const& path, std::span<ColumnSpec const> schema, WriterConfig const& cfg)
    -> StatusValue<Writer>
{
    auto resolved = resolve_schema(schema);
    if (resolved.row_size == 0) {
        return std::unexpected{make_error_code(ColbinError::invalid_schema)};
    }

    std::string const schema_text = serialize_schema(schema);
    if (schema_text.size() > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected{make_error_code(ColbinError::invalid_schema)};
    }

    uint32_t const min_header_total = HEADER_PREAMBLE_BYTES + static_cast<uint32_t>(schema_text.size());
    // Round header up to the row alignment so the first row is naturally
    // aligned, and to at least 64 B for cache-line friendliness.
    uint32_t const header_align = std::max<uint32_t>(64, resolved.row_align);
    uint32_t const header_total = static_cast<uint32_t>(round_up(min_header_total, header_align));

    // Open / create the file. We use O_RDWR rather than O_WRONLY so we
    // can mmap PROT_READ|PROT_WRITE.
    int const fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        return std::unexpected{io_failure()};
    }

    struct stat st{};
    if (::fstat(fd, &st) < 0) {
        auto const ec = io_failure();
        ::close(fd);
        return std::unexpected{ec};
    }

    bool const fresh = (st.st_size < static_cast<off_t>(header_total));

    if (fresh) {
        // Brand-new file (or one too small to be valid). Truncate to 0,
        // then write the preamble + schema, then ftruncate to capacity.
        if (::ftruncate(fd, 0) < 0) {
            auto const ec = io_failure();
            ::close(fd);
            return std::unexpected{ec};
        }
        // Build the header in a small stack buffer.
        std::array<uint8_t, HEADER_PREAMBLE_BYTES> preamble{};
        std::memcpy(preamble.data() + OFFSET_MAGIC, MAGIC.data(), MAGIC.size());
        store_u32_le(preamble.data() + OFFSET_VERSION, FORMAT_VERSION);
        store_u32_le(preamble.data() + OFFSET_ENDIAN, ENDIAN_MARKER);
        store_u32_le(preamble.data() + OFFSET_HEADER_TOTAL, header_total);
        store_u32_le(preamble.data() + OFFSET_ROW_SIZE, resolved.row_size);
        store_u64_le_atomic(preamble.data() + OFFSET_COMMITTED_ROWS, 0);
        store_u32_le(preamble.data() + OFFSET_SCHEMA_LEN, static_cast<uint32_t>(schema_text.size()));
        if (auto s = write_full(fd, preamble.data(), preamble.size()); !s) {
            ::close(fd);
            return std::unexpected{s.error()};
        }
        if (auto s = write_full(fd, schema_text.data(), schema_text.size()); !s) {
            ::close(fd);
            return std::unexpected{s.error()};
        }
        // Zero-pad to header_total.
        if (uint64_t const pad = header_total - HEADER_PREAMBLE_BYTES - schema_text.size(); pad > 0) {
            std::vector<uint8_t> const zeros(pad, 0);
            if (auto s = write_full(fd, zeros.data(), zeros.size()); !s) {
                ::close(fd);
                return std::unexpected{s.error()};
            }
        }
    } else {
        // Append-mode: validate magic / version / endian / row_size /
        // schema match what we're being asked to write.
        std::array<uint8_t, HEADER_PREAMBLE_BYTES> preamble{};
        if (::pread(fd, preamble.data(), preamble.size(), 0) != static_cast<ssize_t>(preamble.size())) {
            ::close(fd);
            return std::unexpected{make_error_code(ColbinError::truncated_header)};
        }
        if (std::memcmp(preamble.data() + OFFSET_MAGIC, MAGIC.data(), MAGIC.size()) != 0) {
            ::close(fd);
            return std::unexpected{make_error_code(ColbinError::bad_magic)};
        }
        uint32_t version = 0;
        std::memcpy(&version, preamble.data() + OFFSET_VERSION, sizeof(version));
        if (version != FORMAT_VERSION) {
            ::close(fd);
            return std::unexpected{make_error_code(ColbinError::unsupported_version)};
        }
        uint32_t endian = 0;
        std::memcpy(&endian, preamble.data() + OFFSET_ENDIAN, sizeof(endian));
        if (endian != ENDIAN_MARKER) {
            ::close(fd);
            return std::unexpected{make_error_code(ColbinError::endian_mismatch)};
        }
        uint32_t existing_row_size = 0;
        std::memcpy(&existing_row_size, preamble.data() + OFFSET_ROW_SIZE, sizeof(existing_row_size));
        if (existing_row_size != resolved.row_size) {
            ::close(fd);
            return std::unexpected{make_error_code(ColbinError::schema_row_size_mismatch)};
        }
        uint32_t existing_header_total = 0;
        std::memcpy(&existing_header_total, preamble.data() + OFFSET_HEADER_TOTAL, sizeof(existing_header_total));
        if (existing_header_total != header_total) {
            ::close(fd);
            return std::unexpected{make_error_code(ColbinError::invalid_schema)};
        }
    }

    // Compute initial mmap size and ftruncate to it. Make sure we
    // include enough room for at least one row beyond the existing
    // content (so writers that resume an existing file don't have to
    // grow immediately).
    uint64_t const page = page_size_bytes();
    uint64_t const cur_size = static_cast<uint64_t>(::lseek(fd, 0, SEEK_END));
    uint64_t mapped_size = round_up(std::max<uint64_t>(cfg.initial_capacity_bytes, cur_size + resolved.row_size), page);
    if (mapped_size > cfg.max_capacity_bytes) {
        ::close(fd);
        return std::unexpected{make_error_code(ColbinError::capacity_exceeded)};
    }
    if (mapped_size > cur_size) {
        if (::ftruncate(fd, static_cast<off_t>(mapped_size)) < 0) {
            auto const ec = io_failure();
            ::close(fd);
            return std::unexpected{ec};
        }
    } else {
        mapped_size = cur_size;
    }

    void* mapped = ::mmap(nullptr, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        auto const ec = io_failure();
        ::close(fd);
        return std::unexpected{ec};
    }
    auto* base = static_cast<uint8_t*>(mapped);

    // Resume cursor from committed_rows when re-opening; on fresh files
    // committed_rows is zero so cursor starts at header_total.
    uint64_t committed = 0;
    std::memcpy(&committed, base + OFFSET_COMMITTED_ROWS, sizeof(committed));
    uint64_t const cursor = static_cast<uint64_t>(header_total) + (committed * resolved.row_size);
    if (cursor > mapped_size) {
        ::munmap(mapped, mapped_size);
        ::close(fd);
        return std::unexpected{make_error_code(ColbinError::committed_rows_overflow)};
    }

    Writer w;
    w.fd_ = fd;
    w.base_ = base;
    w.mapped_size_ = mapped_size;
    w.cursor_ = cursor;
    w.max_capacity_ = cfg.max_capacity_bytes;
    w.grow_factor_ = cfg.grow_factor < 2 ? 2 : cfg.grow_factor;
    w.header_total_ = header_total;
    w.row_size_ = resolved.row_size;
    w.row_count_ = committed;
    w.path_ = path.string();
    return w;
}

Writer::Writer(Writer&& other) noexcept
    : fd_(other.fd_)
    , base_(other.base_)
    , mapped_size_(other.mapped_size_)
    , cursor_(other.cursor_)
    , max_capacity_(other.max_capacity_)
    , grow_factor_(other.grow_factor_)
    , header_total_(other.header_total_)
    , row_size_(other.row_size_)
    , row_count_(other.row_count_)
    , path_(std::move(other.path_))
{
    other.fd_ = -1;
    other.base_ = nullptr;
    other.mapped_size_ = 0;
}

auto Writer::operator=(Writer&& other) noexcept -> Writer&
{
    if (this != &other) {
        destroy();
        fd_ = other.fd_;
        base_ = other.base_;
        mapped_size_ = other.mapped_size_;
        cursor_ = other.cursor_;
        max_capacity_ = other.max_capacity_;
        grow_factor_ = other.grow_factor_;
        header_total_ = other.header_total_;
        row_size_ = other.row_size_;
        row_count_ = other.row_count_;
        path_ = std::move(other.path_);
        other.fd_ = -1;
        other.base_ = nullptr;
        other.mapped_size_ = 0;
    }
    return *this;
}

Writer::~Writer() noexcept
{
    destroy();
}

void Writer::destroy() noexcept
{
    if (base_ != nullptr) {
        // Publish final row count.
        store_u64_le_atomic(base_ + OFFSET_COMMITTED_ROWS, row_count_);
        // Best-effort msync; ignore errors during teardown.
        (void)::msync(base_, mapped_size_, MS_SYNC);
        ::munmap(base_, mapped_size_);
        base_ = nullptr;
    }
    if (fd_ >= 0) {
        // Truncate the file to the actual high-water mark so the
        // tail of the mmap region (which we over-allocated) doesn't
        // appear as zero-filled garbage rows to the reader.
        (void)::ftruncate(fd_, static_cast<off_t>(cursor_));
        ::close(fd_);
        fd_ = -1;
    }
    mapped_size_ = 0;
    cursor_ = 0;
}

auto Writer::grow() noexcept -> Status
{
    uint64_t const page = page_size_bytes();
    uint64_t new_size = round_up(mapped_size_ * grow_factor_, page);
    if (new_size <= mapped_size_) {
        new_size = mapped_size_ + page;  // defensive: monotonic grow
    }
    if (new_size > max_capacity_) {
        if (mapped_size_ >= max_capacity_) {
            return failure(make_error_code(ColbinError::capacity_exceeded));
        }
        new_size = round_up(max_capacity_, page);
    }
    if (::ftruncate(fd_, static_cast<off_t>(new_size)) < 0) {
        return failure(io_failure());
    }
#if defined(__linux__)
    void* p = ::mremap(base_, mapped_size_, new_size, MREMAP_MAYMOVE);
    if (p == MAP_FAILED) {
        return failure(make_error_code(ColbinError::grow_failed));
    }
#else
    // Darwin / BSD have no mremap. The file is the source of truth for
    // a MAP_SHARED mapping, so munmap the old region and mmap the file
    // fresh at the new size. The kernel may pick a different base
    // address; all subsequent row writes go through base_ so this is
    // safe. If the fresh mmap fails after we've already unmapped, the
    // writer is unrecoverable — callers see grow_failed.
    if (::munmap(base_, mapped_size_) != 0) {
        base_ = nullptr;
        mapped_size_ = 0;
        return failure(make_error_code(ColbinError::grow_failed));
    }
    void* p = ::mmap(nullptr, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (p == MAP_FAILED) {
        base_ = nullptr;
        mapped_size_ = 0;
        return failure(make_error_code(ColbinError::grow_failed));
    }
#endif
    base_ = static_cast<uint8_t*>(p);
    mapped_size_ = new_size;
    return success();
}

auto Writer::write_row(std::span<uint8_t const> row) noexcept -> Status
{
    if (row.size() != row_size_) {
        return failure(make_error_code(ColbinError::invalid_schema));
    }
    if (cursor_ + row_size_ > mapped_size_) [[unlikely]] {
        if (auto s = grow(); !s) {
            return s;
        }
    }
    std::memcpy(base_ + cursor_, row.data(), row_size_);
    cursor_ += row_size_;
    ++row_count_;
    return success();
}

auto Writer::commit() noexcept -> Status
{
    if (base_ == nullptr) {
        return failure(make_error_code(ColbinError::io_error));
    }
    store_u64_le_atomic(base_ + OFFSET_COMMITTED_ROWS, row_count_);
    // msync just the header page — cheap and enough to make
    // committed_rows visible after a crash.
    if (::msync(base_, page_size_bytes(), MS_ASYNC) < 0) {
        return failure(io_failure());
    }
    return success();
}

auto Writer::sync() noexcept -> Status
{
    if (base_ == nullptr) {
        return failure(make_error_code(ColbinError::io_error));
    }
    if (::msync(base_, cursor_, MS_ASYNC) < 0) {
        return failure(io_failure());
    }
    return success();
}

}  // namespace statusbar::colbin
