#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Config module error types
/// Provides ConfigError enum and std::error_category integration

#include <string>
#include <system_error>

namespace statusbar::config {

/// Config error codes
enum class ConfigError
{
    /// A built-in CLI command (--help, --completion, --config-save, --config-dump)
    /// was handled. The caller should exit cleanly (status 0), not treat this as
    /// a real failure. Callers normally check via handled_builtin_command().
    builtin_handled = 1,
    /// One or more unknown options were present on the command line.
    unknown_option,
};

/// Error category for ConfigError
class ConfigErrorCategory : public std::error_category
{
  public:
    [[nodiscard]] auto name() const noexcept -> char const* override { return "statusbar.config"; }

    /// @param ev Error code value to convert to a message string
    [[nodiscard]] auto message(int ev) const -> std::string override;
};

/// Get the singleton ConfigError category instance
[[nodiscard]] auto config_error_category() noexcept -> std::error_category const&;

/// Create an error_code from a ConfigError
/// @param e The ConfigError value to convert
[[nodiscard]] auto make_error_code(ConfigError e) noexcept -> std::error_code;

}  // namespace statusbar::config

/// Register ConfigError as an error code enum
template <>
struct std::is_error_code_enum<statusbar::config::ConfigError> : std::true_type
{};
