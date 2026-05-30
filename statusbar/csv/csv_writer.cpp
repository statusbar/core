// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/csv/csv_writer.hpp"

#include "statusbar/status/throw_or_abort.hpp"

#include <cerrno>
#include <system_error>

namespace statusbar::csv {

namespace {

[[nodiscard]] auto needs_quoting(std::string_view field) noexcept -> bool
{
    for (char const c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            return true;
        }
    }
    return false;
}

/// CSV formula-injection guard. Spreadsheets (Excel / LibreOffice /
/// Google Sheets) treat a cell whose first character is one of these as
/// a formula and evaluate it on open — a serious risk when CSV is
/// rendered from untrusted log data. RFC 4180 says nothing about this;
/// the OWASP-recommended mitigation is to prefix the cell with a single
/// apostrophe so the spreadsheet enters text mode for that cell. The
/// apostrophe DOES land in the field as data when the file is read by
/// non-spreadsheet tools, so this is a deliberate fidelity-for-safety
/// trade.
[[nodiscard]] auto starts_with_formula_trigger(std::string_view field) noexcept -> bool
{
    if (field.empty()) {
        return false;
    }
    char const c = field.front();
    return c == '=' || c == '+' || c == '-' || c == '@' || c == '\t' || c == '\r';
}

[[nodiscard]] auto write_escaped_field(std::FILE* fp, std::string_view field) -> bool
{
    bool const formula_guard = starts_with_formula_trigger(field);
    if (!needs_quoting(field) && !formula_guard) {
        return std::fwrite(field.data(), 1, field.size(), fp) == field.size();
    }
    if (std::fputc('"', fp) == EOF) {
        return false;
    }
    if (formula_guard && std::fputc('\'', fp) == EOF) {
        return false;
    }
    for (char const c : field) {
        if (c == '"' && std::fputc('"', fp) == EOF) {
            return false;
        }
        if (std::fputc(c, fp) == EOF) {
            return false;
        }
    }
    return std::fputc('"', fp) != EOF;
}

[[nodiscard]] auto write_row_to_fp(std::FILE* fp, std::span<std::string_view const> fields) -> bool
{
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0 && std::fputc(',', fp) == EOF) {
            return false;
        }
        if (!write_escaped_field(fp, fields[i])) {
            return false;
        }
    }
    if (std::fputc('\r', fp) == EOF) {
        return false;
    }
    return std::fputc('\n', fp) != EOF;
}

}  // namespace

CsvWriter::CsvWriter(std::string const& path, std::span<std::string_view const> header)
    : fp_{std::fopen(path.c_str(), "wb")}
{
    if (fp_ == nullptr) {
        throw_or_abort(std::error_code{errno, std::generic_category()}, "CsvWriter: fopen failed");
    }
    if (!write_row_to_fp(fp_, header)) {
        int const err = errno;
        std::fclose(fp_);
        fp_ = nullptr;
        throw_or_abort(std::error_code{err, std::generic_category()}, "CsvWriter: header write failed");
    }
}

CsvWriter::~CsvWriter() noexcept
{
    if (fp_ != nullptr) {
        std::fclose(fp_);
    }
}

auto CsvWriter::write_row(std::span<std::string_view const> fields) -> Status
{
    if (!write_row_to_fp(fp_, fields)) {
        return failure(std::error_code{errno, std::generic_category()});
    }
    ++row_count_;
    return success();
}

auto CsvWriter::flush() -> Status
{
    if (std::fflush(fp_) != 0) {
        return failure(std::error_code{errno, std::generic_category()});
    }
    return success();
}

}  // namespace statusbar::csv
