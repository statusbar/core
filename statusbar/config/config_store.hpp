#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Config Store - Configuration class with TOML file loading and CLI overrides
///
/// Features:
/// - Load multiple TOML files in cascade (later files override earlier)
/// - CLI argument overrides (`--key=value`, `--key value`)
/// - Generic ConfigParseable support for custom types stored as strings
/// - Write current configuration to TOML file
/// - Special `--config` argument to load additional config files

#include "statusbar/args/args_spec.hpp"
#include "statusbar/status/status.hpp"
#include "statusbar/toml/toml_error.hpp"
#include "statusbar/toml/toml_parser.hpp"
#include "statusbar/toml/toml_value.hpp"

#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace statusbar::config {

/// Configuration class that combines TOML files with CLI overrides
///
/// Usage:
///   Config config;
///   // Load cascade of config files (later overrides earlier)
///   config.load_file("/etc/myapp/config.toml");      // System defaults
///   config.load_file("~/.config/myapp/config.toml"); // User overrides
///
///   // Apply CLI with support for --config to load additional files
///   auto remaining = config.apply_cli_overrides(argc, argv);
///
///   // Access values
///   auto host = config.get_string("server.host");
///
///   // Write current config to file
///   config.write_file("output.toml");
///
/// CLI overrides use the format: `--key=value` or `--key value`
/// Dotted keys work: `--server.port=8080`
/// Special: `--config=file.toml` loads additional config file
class Config
{
  public:
    Config() = default;

    /// Load configuration from a TOML file (merges with existing)
    /// @param path Path to the TOML file
    /// @return Success or error (file_not_found is not an error if file doesn't exist)
    [[nodiscard]] auto load_file(std::string_view path) -> Status;

    /// Load configuration from a TOML file if it exists (silent on missing file)
    /// @param path Path to the TOML file
    /// @return Success (even if file doesn't exist) or parse error
    [[nodiscard]] auto load_file_if_exists(std::string_view path) -> Status;

    /// Load configuration from a TOML string (merges with existing)
    /// @param toml TOML content
    /// @return Success or error
    [[nodiscard]] auto load_string(std::string_view toml) -> Status;

    /// Apply command-line overrides to the configuration
    /// Recognizes: `--key=value`, `--key value`, `-key=value`, `-key value`
    /// Special: `--config=file.toml` or `--config file.toml` loads additional config
    /// @param argc Argument count
    /// @param argv Argument values
    /// @return Vector of remaining (non-override) arguments
    [[nodiscard]] auto apply_cli_overrides(int argc, char const* const* argv) -> std::vector<std::string>;

    /// Apply command-line overrides from a span
    /// @param args Span of command-line argument strings
    [[nodiscard]] auto apply_cli_overrides(std::span<char const* const> args) -> std::vector<std::string>;

    /// Apply command-line overrides, consulting `specs` to decide per-arg
    /// whether the value is a typed scalar (Integer / Float / Flag — gets
    /// auto-parsed via TOML rules) or a string-shaped value (String /
    /// Device / File / Directory / Choice — stored verbatim, no parsing).
    /// This is the right entry point when you have argument specs handy:
    /// it avoids the auto-typing pitfall where pure-octal-digit strings
    /// like "00000000000000000000000000000063" get coerced into integers.
    [[nodiscard]] auto apply_cli_overrides(int argc, char const* const* argv, args::ArgumentSpecs const& specs)
        -> std::vector<std::string>;

    /// Set a value from a string (auto-detects type)
    /// @param key Dotted key path
    /// @param value_str String representation of value
    void set_from_string(std::string_view key, std::string_view value_str);

    /// Set a value as a literal string, bypassing the TOML-style
    /// auto-typing in set_from_string. Use when the destination type is
    /// known to be a string and you do not want strings that happen to
    /// look like integers / floats / booleans to be coerced.
    void set_string(std::string_view key, std::string_view value_str);

    /// Get the root table
    [[nodiscard]] auto root() -> toml::Table& { return root_; }
    [[nodiscard]] auto root() const -> toml::Table const& { return root_; }

    /// Get a value by dotted key path
    /// @param key Dotted key path (e.g., "server.host")
    [[nodiscard]] auto get(std::string_view key) -> toml::Value* { return root_.get_path(key); }
    /// @param key Dotted key path (e.g., "server.host")
    [[nodiscard]] auto get(std::string_view key) const -> toml::Value const* { return root_.get_path(key); }

    /// Check if a key exists
    /// @param key Dotted key path to check
    [[nodiscard]] auto contains(std::string_view key) const -> bool { return root_.get_path(key) != nullptr; }

    /// Get string value
    /// @param key Dotted key path
    [[nodiscard]] auto get_string(std::string_view key) const -> std::optional<std::string_view>
    {
        if (auto const* v = get(key); v != nullptr) {
            return v->as_string();
        }
        return std::nullopt;
    }

    /// Get string value with default
    /// @param key Dotted key path
    /// @param default_value Value returned if key is not found
    [[nodiscard]] auto get_string(std::string_view key, std::string_view default_value) const -> std::string_view
    {
        return get_string(key).value_or(default_value);
    }

    /// Get any value converted to string
    /// Unlike get_string(), this converts integers, floats, and booleans to their string representation
    /// Useful for CLI args where `--fps=25` stores as integer but you want "25" as string
    /// @param key Dotted key path
    [[nodiscard]] auto get_value_as_string(std::string_view key) const -> std::optional<std::string>;

    /// Get any value converted to string with default
    /// @param key Dotted key path
    /// @param default_value Value returned if key is not found
    [[nodiscard]] auto get_value_as_string(std::string_view key, std::string_view default_value) const -> std::string;

    /// Get integer value
    /// @param key Dotted key path
    [[nodiscard]] auto get_integer(std::string_view key) const -> std::optional<int64_t>
    {
        if (auto const* v = get(key); v != nullptr) {
            return v->as_integer();
        }
        return std::nullopt;
    }

    /// Get integer value with default
    /// @param key Dotted key path
    /// @param default_value Value returned if key is not found
    [[nodiscard]] auto get_integer(std::string_view key, int64_t default_value) const -> int64_t
    {
        return get_integer(key).value_or(default_value);
    }

    /// Get float value
    /// @param key Dotted key path
    [[nodiscard]] auto get_float(std::string_view key) const -> std::optional<double>
    {
        if (auto const* v = get(key); v != nullptr) {
            return v->as_float();
        }
        return std::nullopt;
    }

    /// Get float value with default
    /// @param key Dotted key path
    /// @param default_value Value returned if key is not found
    [[nodiscard]] auto get_float(std::string_view key, double default_value) const -> double
    {
        return get_float(key).value_or(default_value);
    }

    /// Get boolean value
    /// @param key Dotted key path
    [[nodiscard]] auto get_boolean(std::string_view key) const -> std::optional<bool>
    {
        if (auto const* v = get(key); v != nullptr) {
            return v->as_boolean();
        }
        return std::nullopt;
    }

    /// Get boolean value with default
    /// @param key Dotted key path
    /// @param default_value Value returned if key is not found
    [[nodiscard]] auto get_boolean(std::string_view key, bool default_value) const -> bool
    {
        return get_boolean(key).value_or(default_value);
    }

    /// Get array value
    /// @param key Dotted key path
    [[nodiscard]] auto get_array(std::string_view key) -> toml::Array*
    {
        if (auto* v = get(key); v != nullptr) {
            return v->as_array();
        }
        return nullptr;
    }

    /// @param key Dotted key path
    [[nodiscard]] auto get_array(std::string_view key) const -> toml::Array const*
    {
        if (auto const* v = get(key); v != nullptr) {
            return v->as_array();
        }
        return nullptr;
    }

    /// Get table value
    /// @param key Dotted key path
    [[nodiscard]] auto get_table(std::string_view key) -> toml::Table*
    {
        if (auto* v = get(key); v != nullptr) {
            return v->as_table();
        }
        return nullptr;
    }

    /// @param key Dotted key path
    [[nodiscard]] auto get_table(std::string_view key) const -> toml::Table const*
    {
        if (auto const* v = get(key); v != nullptr) {
            return v->as_table();
        }
        return nullptr;
    }

    /// Get a typed vector from a key
    /// Forwards to statusbar::toml::get_list<T>()
    /// @param key Dotted key path
    template <typename T>
    [[nodiscard]] auto get_list(std::string_view key) const -> std::vector<T>
    {
        return toml::get_list<T>(root_, key);
    }

    //
    // ConfigParseable Type Accessors - stored as strings, parsed on access
    //

    /// Get a ConfigParseable value (stored as string, parsed on access).
    /// Returns nullopt if the key is missing OR the parser failed; callers
    /// that care about the difference can reach for the underlying
    /// `config_parse(...)` ADL hook directly.
    template <args::ConfigParseable T>
    [[nodiscard]] auto get(std::string_view key) const -> std::optional<T>
    {
        auto str = get_string(key);
        if (!str.has_value()) {
            return std::nullopt;
        }
        auto parsed = config_parse(std::type_identity<T>{}, *str);
        if (!parsed) {
            return std::nullopt;
        }
        return *parsed;
    }

    /// Get a ConfigParseable value with default
    template <args::ConfigParseable T>
    [[nodiscard]] auto get(std::string_view key, T const& default_value) const -> T
    {
        return get<T>(key).value_or(default_value);
    }

    /// Set a ConfigParseable value (stored as string)
    template <args::ConfigParseable T>
    void set(std::string_view key, T const& value)
    {
        set_value(key, toml::Value{config_format(value)});
    }

    //
    // Setters
    //

    /// Set a string value (const char*)
    /// @param key Dotted key path
    /// @param value String value to set
    void set(std::string_view key, char const* value) { set_value(key, toml::Value{std::string{value}}); }

    /// Set a string value (string_view)
    /// @param key Dotted key path
    /// @param value String value to set
    void set(std::string_view key, std::string_view value) { set_value(key, toml::Value{std::string{value}}); }

    /// Set a string value (string)
    /// @param key Dotted key path
    /// @param value String value to set
    void set(std::string_view key, std::string value) { set_value(key, toml::Value{std::move(value)}); }

    /// Set an integer value
    /// @param key Dotted key path
    /// @param value Integer value to set
    void set(std::string_view key, int64_t value) { set_value(key, toml::Value{value}); }

    /// Set a float value
    /// @param key Dotted key path
    /// @param value Float value to set
    void set(std::string_view key, double value) { set_value(key, toml::Value{value}); }

    /// Set a boolean value
    /// @param key Dotted key path
    /// @param value Boolean value to set
    void set(std::string_view key, bool value) { set_value(key, toml::Value{value}); }

    //
    // Serialization
    //

    /// Write configuration to a TOML file
    /// @param path Path to output file
    /// @return Success or error
    [[nodiscard]] auto write_file(std::string_view path) const -> Status;

    /// Write configuration to a TOML file, annotating each value with
    /// the matching ArgumentSpec's description as `#`-prefixed comments.
    [[nodiscard]] auto write_file(std::string_view path, args::ArgumentSpecs const& specs) const -> Status;

    /// Serialize configuration to a string
    [[nodiscard]] auto to_toml_string() const -> std::string;

    /// Serialize configuration to a string, annotating each value with
    /// the matching ArgumentSpec's description as `#`-prefixed comments.
    [[nodiscard]] auto to_toml_string(args::ArgumentSpecs const& specs) const -> std::string;

    /// Get list of loaded configuration files
    [[nodiscard]] auto loaded_files() const -> std::vector<std::string> const& { return loaded_files_; }

    /// Parse a CLI value string into a typed Value
    /// Forwards to statusbar::toml::parse_value_string()
    /// @param s String representation to parse
    [[nodiscard]] static auto parse_value_string(std::string_view s) -> toml::Value { return toml::parse_value_string(s); }

  private:
    void set_value(std::string_view key, toml::Value value);

    /// Merge source table into dest table (source values override dest)
    static void merge_table(toml::Table& dest, toml::Table const& source);

    /// Write a value to output stream
    static void write_value(std::ostream& out, toml::Value const& value);

    /// Write a table to output stream with header prefix. When `specs`
    /// is non-null, each non-table value is preceded by the matching
    /// ArgumentSpec's description rendered as `#`-prefixed comment lines.
    static void write_table(
        std::ostream& out, toml::Table const& table, std::string_view prefix, args::ArgumentSpecs const* specs = nullptr);

    toml::Table root_;
    std::vector<std::string> loaded_files_;
};

}  // namespace statusbar::config
