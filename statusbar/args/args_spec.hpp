#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Args Spec - Argument specification, type traits, and shell completion
/// Provides utilities to generate bash and zsh completion scripts from argument definitions
///
/// Usage:
///   ArgumentSpecs specs;
///   specs.add("timecode", ArgType::String, "Start timecode in HH:MM:SS:FF format", "00:00:00:00");
///   specs.add("fps", ArgType::Choice, "Frame rate", "30ndf", {"23.976", "24", "25", "29.97df", "30ndf"});
///   specs.add("help", ArgType::Flag, "Show help message");
///
///   // Generate and print bash completion
///   if (config.contains("completion")) {
///       auto shell = config.get_string("completion");
///       if (shell == "bash") std::print("{}", specs.generate_bash("ltc_gen_tool"));
///       if (shell == "zsh") std::print("{}", specs.generate_zsh("ltc_gen_tool"));
///       return 0;
///   }

#include "statusbar/sg14/inplace_function.h"
#include "statusbar/status/status.hpp"
#include "statusbar/toml/toml_value.hpp"

#include <algorithm>
#include <concepts>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace statusbar::args {

/// Argument type for completion hints
enum class ArgType
{
    String,     ///< Generic string value
    Integer,    ///< Integer value
    Float,      ///< Floating point value
    Flag,       ///< Boolean flag (no value required)
    Choice,     ///< One of a set of choices
    File,       ///< File path (enables file completion)
    Directory,  ///< Directory path (enables directory completion)
    Device,     ///< Device name (network interface, audio device, etc.)
};

/// True for types whose CLI value should be stored verbatim as a string,
/// not run through TOML auto-typing. Used by Config::apply_cli_overrides
/// when it has access to the argument specs — without this distinction,
/// values like "00000000000000000000000000000063" (a valid hex session id
/// that also happens to be a valid octal integer literal) get coerced
/// into integers and lose their original form.
[[nodiscard]] constexpr auto is_string_shaped(ArgType type) noexcept -> bool
{
    switch (type) {
        case ArgType::String:
        case ArgType::Choice:
        case ArgType::File:
        case ArgType::Directory:
        case ArgType::Device:
            return true;
        case ArgType::Integer:
        case ArgType::Float:
        case ArgType::Flag:
            return false;
    }
    return false;
}

/// Get the placeholder name for an argument type (for help text)
[[nodiscard]] constexpr auto arg_type_placeholder(ArgType type) noexcept -> std::string_view
{
    switch (type) {
        case ArgType::String:
            return "VALUE";
        case ArgType::Integer:
            return "N";
        case ArgType::Float:
            return "NUM";
        case ArgType::Choice:
            return "CHOICE";
        case ArgType::File:
            return "FILE";
        case ArgType::Directory:
            return "DIR";
        case ArgType::Device:
            return "DEVICE";
        case ArgType::Flag:
            return "";
    }
    return "";
}

/// Concept for types that can be parsed from/formatted to config strings via ADL
/// Types opt in by defining free functions in their own namespace:
///   auto config_parse(std::type_identity<T>, std::string_view) -> ::statusbar::StatusValue<T>
///   auto config_format(T const&) -> std::string
/// The status-typed return lets the parser surface its own error code
/// (e.g. an EUI-48 vs EUI-64 mismatch, malformed hex, out-of-range value)
/// instead of collapsing to a bare nullopt.
template <typename T>
concept ConfigParseable = requires(std::string_view sv, T const& t) {
    { config_parse(std::type_identity<T>{}, sv) } -> std::same_as<::statusbar::StatusValue<T>>;
    { config_format(t) } -> std::convertible_to<std::string>;
};

//
// Type Traits for Template-Based Argument Addition
//

/// Traits to map C++ types to ArgType and provide type-appropriate value retrieval
template <typename T>
struct ArgTypeTraits;

// String types -> ArgType::String
template <>
struct ArgTypeTraits<std::string_view>
{
    static constexpr ArgType arg_type = ArgType::String;
    static auto get_value(toml::Table const& root, std::string_view name, std::string_view def) -> std::string
    {
        auto const* v = root.get_path(name);
        if (v == nullptr) {
            return std::string{def};
        }
        if (auto s = v->as_string(); s.has_value()) {
            return std::string{*s};
        }
        if (auto i = v->as_integer(); i.has_value()) {
            return std::to_string(*i);
        }
        if (auto f = v->as_float(); f.has_value()) {
            return std::to_string(*f);
        }
        if (auto b = v->as_boolean(); b.has_value()) {
            return *b ? "true" : "false";
        }
        return std::string{def};
    }
};

template <>
struct ArgTypeTraits<std::string>
{
    static constexpr ArgType arg_type = ArgType::String;
    static auto get_value(toml::Table const& root, std::string_view name, std::string_view def) -> std::string
    {
        auto const* v = root.get_path(name);
        if (v == nullptr) {
            return std::string{def};
        }
        if (auto s = v->as_string(); s.has_value()) {
            return std::string{*s};
        }
        if (auto i = v->as_integer(); i.has_value()) {
            return std::to_string(*i);
        }
        if (auto f = v->as_float(); f.has_value()) {
            return std::to_string(*f);
        }
        if (auto b = v->as_boolean(); b.has_value()) {
            return *b ? "true" : "false";
        }
        return std::string{def};
    }
};

// Integer types -> ArgType::Integer (concept-constrained to cover all integral types)
template <std::integral T>
    requires(!std::same_as<T, bool>)
struct ArgTypeTraits<T>
{
    static constexpr ArgType arg_type = ArgType::Integer;
    static auto get_value(toml::Table const& root, std::string_view name, T def) -> T
    {
        auto const* v = root.get_path(name);
        if (v == nullptr) {
            return def;
        }
        return static_cast<T>(v->as_integer().value_or(static_cast<int64_t>(def)));
    }
};

// Float types -> ArgType::Float (concept-constrained)
template <std::floating_point T>
struct ArgTypeTraits<T>
{
    static constexpr ArgType arg_type = ArgType::Float;
    static auto get_value(toml::Table const& root, std::string_view name, T def) -> T
    {
        auto const* v = root.get_path(name);
        if (v == nullptr) {
            return def;
        }
        return static_cast<T>(v->as_float().value_or(static_cast<double>(def)));
    }
};

// Boolean -> ArgType::Flag
template <>
struct ArgTypeTraits<bool>
{
    static constexpr ArgType arg_type = ArgType::Flag;
    static auto get_value(toml::Table const& root, std::string_view name, bool def) -> bool
    {
        auto const* v = root.get_path(name);
        if (v == nullptr) {
            return def;
        }
        return v->as_boolean().value_or(def);
    }
};

// Generic ConfigParseable fallback
template <ConfigParseable T>
struct ArgTypeTraits<T>
{
    static constexpr ArgType arg_type = ArgType::String;
    static auto get_value(toml::Table const& root, std::string_view name, T const& def) -> T
    {
        auto const* v = root.get_path(name);
        if (v == nullptr) {
            return def;
        }
        auto s = v->as_string();
        if (!s.has_value()) {
            return def;
        }
        // StatusValue<T> exposes std::expected's value_or(), so the falls-back-
        // to-default-on-parse-error behaviour reads the same as before.
        return config_parse(std::type_identity<T>{}, *s).value_or(def);
    }
};

/// Helper to convert default value to string for storage
/// @param value The default value to convert
template <typename T>
auto default_to_string(T const& value) -> std::string
{
    // NOLINTBEGIN(bugprone-branch-clone) - same operation for different type categories
    if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string>) {
        return std::string{value};
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_floating_point_v<T>) {
        return std::to_string(value);
    } else if constexpr (std::is_integral_v<T>) {
        return std::to_string(value);
    } else if constexpr (ConfigParseable<T>) {
        return config_format(value);
    } else {
        return {};
    }
    // NOLINTEND(bugprone-branch-clone)
}

/// Helper to parse default value from string
/// @param str The string representation to parse into type T
template <typename T>
auto parse_default(std::string const& str) -> T
{
    if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string>) {
        return str;
    } else if constexpr (std::is_same_v<T, bool>) {
        return str == "true" || str == "1";
    } else if constexpr (std::is_floating_point_v<T>) {
        return str.empty() ? T{0} : static_cast<T>(std::stod(str));
    } else if constexpr (std::is_integral_v<T>) {
        return str.empty() ? T{0} : static_cast<T>(std::stoll(str));
    } else {
        return T{};
    }
}

/// Specification for a single command-line argument
struct ArgumentSpec
{
    std::string name;                  ///< Argument name (without --)
    ArgType type{ArgType::String};     ///< Argument type
    std::string description;           ///< Help text
    std::string default_value;         ///< Default value (empty if none)
    std::vector<std::string> choices;  ///< Valid choices (for ArgType::Choice)
    bool required{false};              ///< Whether argument is required
    bool is_list{false};               ///< Whether this is a comma-separated list argument

    /// Optional binding function that applies config value to a destination
    /// Throws Status on validation failure
    statusbar::sg14::inplace_function<void(toml::Table const&), 96> apply_binding;
};

/// Collection of argument specifications with completion script generation
class ArgumentSpecs
{
  public:
    ArgumentSpecs() = default;

    /// Add a flag (boolean) without binding
    /// @param name Argument name (without --)
    /// @param description Help text
    auto add_flag(std::string_view name, std::string_view description) -> void;

    /// Add a file path argument without binding
    /// @param name Argument name (without --)
    /// @param description Help text
    /// @param default_value Default file path (optional)
    auto add_file(std::string_view name, std::string_view description, std::string_view default_value = {}) -> void;

    /// Add a choice argument without binding
    /// @param name Argument name (without --)
    /// @param description Help text
    /// @param choices Valid choice values
    /// @param default_value Default value (optional)
    auto add_choice(
        std::string_view name,
        std::string_view description,
        std::initializer_list<char const*> choices,
        std::string_view default_value = {}) -> void;

    /// Add a typed argument without binding
    /// Type T determines the ArgType automatically via ArgTypeTraits
    /// @param name Argument name (without --)
    /// @param description Help text
    /// @param default_value Default value of type T
    template <typename T>
        requires requires { ArgTypeTraits<T>::arg_type; }
    auto add(std::string_view name, std::string_view description, T default_value) -> void
    {
        specs_.push_back(ArgumentSpec{
            .name = std::string{name},
            .type = ArgTypeTraits<T>::arg_type,
            .description = std::string{description},
            .default_value = default_to_string(default_value),
        });
    }

    /// Add a typed argument with binding lambda
    /// Type T determines the ArgType automatically via ArgTypeTraits
    /// Lambda receives the parsed value as type T, throws Status on validation failure
    /// @param name Argument name (without --)
    /// @param description Help text
    /// @param default_value Default value of type T
    /// @param setter Lambda: void(T) - throws Status on failure
    template <typename T, typename Func>
        requires requires { ArgTypeTraits<T>::arg_type; } && std::invocable<Func, T>
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward,performance-unnecessary-value-param) - std::forward used in lambda capture, default_value copied into lambda
    void add(std::string_view name, std::string_view description, T default_value, Func&& setter)
    {
        specs_.push_back(ArgumentSpec{
            .name = std::string{name},
            .type = ArgTypeTraits<T>::arg_type,
            .description = std::string{description},
            .default_value = default_to_string(default_value),
        });

        // Create binding that retrieves value as type T and calls setter
        auto& spec = specs_.back();
        spec.apply_binding = [setter = std::forward<Func>(setter), name_str = std::string{name}, def = default_value](
                                 toml::Table const& root) -> void { setter(ArgTypeTraits<T>::get_value(root, name_str, def)); };
    }

    /// Add a choice argument with binding lambda
    /// @param name Argument name (without --)
    /// @param description Help text
    /// @param choices Valid choice values
    /// @param default_value Default value
    /// @param setter Lambda: void(std::string_view) - throws Status on failure
    template <typename Func>
        requires std::invocable<Func, std::string_view>
    void add_choice(
        std::string_view name,
        std::string_view description,
        std::vector<std::string> choices,
        std::string_view default_value,
        // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward) - std::forward used in lambda capture
        Func&& setter)
    {
        specs_.push_back(ArgumentSpec{
            .name = std::string{name},
            .type = ArgType::Choice,
            .description = std::string{description},
            .default_value = std::string{default_value},
            .choices = std::move(choices),
        });

        auto& spec = specs_.back();
        spec.apply_binding = [setter = std::forward<Func>(setter), name_str = std::string{name}, def = std::string{default_value}](
                                 toml::Table const& root) -> void {
            auto const* v = root.get_path(name_str);
            if (v == nullptr) {
                setter(def);
                return;
            }
            if (auto s = v->as_string(); s.has_value()) {
                setter(*s);
                return;
            }
            setter(def);
        };
    }

    /// Add a choice argument with binding lambda (initializer_list overload for Linux compatibility)
    /// @param name Argument name (without --)
    /// @param description Help text
    /// @param choices Valid choice values
    /// @param default_value Default value
    /// @param setter Lambda: void(std::string_view) - throws Status on failure
    template <typename Func>
        requires std::invocable<Func, std::string_view>
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward) - forwards to other add_choice overload
    void add_choice(
        std::string_view name,
        std::string_view description,
        std::initializer_list<char const*> choices,
        std::string_view default_value,
        Func&& setter)
    {
        std::vector<std::string> choice_vec;
        choice_vec.reserve(choices.size());
        for (auto const* c : choices) {
            choice_vec.emplace_back(c);
        }
        add_choice(name, description, std::move(choice_vec), default_value, std::forward<Func>(setter));
    }

    /// Add a flag (boolean) with binding lambda
    /// @param name Argument name (without --)
    /// @param description Help text
    /// @param setter Lambda: void(bool) - throws Status on failure
    template <typename Func>
        requires std::invocable<Func, bool>
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward) - std::forward used in lambda capture
    void add_flag(std::string_view name, std::string_view description, Func&& setter)
    {
        specs_.push_back(ArgumentSpec{
            .name = std::string{name},
            .type = ArgType::Flag,
            .description = std::string{description},
        });

        auto& spec = specs_.back();
        spec.apply_binding = [setter = std::forward<Func>(setter), name_str = std::string{name}](toml::Table const& root) -> void {
            auto const* v = root.get_path(name_str);
            if (v == nullptr) {
                setter(false);
                return;
            }
            setter(v->as_boolean().value_or(false));
        };
    }

    /// Add a file path argument with binding lambda
    /// @param name Argument name (without --)
    /// @param description Help text
    /// @param default_value Default file path
    /// @param setter Lambda: void(std::string_view) - throws Status on failure
    template <typename Func>
        requires std::invocable<Func, std::string_view>
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward) - std::forward used in lambda capture
    void add_file(std::string_view name, std::string_view description, std::string_view default_value, Func&& setter)
    {
        specs_.push_back(ArgumentSpec{
            .name = std::string{name},
            .type = ArgType::File,
            .description = std::string{description},
            .default_value = std::string{default_value},
        });

        auto& spec = specs_.back();
        spec.apply_binding = [setter = std::forward<Func>(setter), name_str = std::string{name}, def = std::string{default_value}](
                                 toml::Table const& root) -> void {
            auto const* v = root.get_path(name_str);
            if (v == nullptr) {
                setter(def);
                return;
            }
            if (auto s = v->as_string(); s.has_value()) {
                setter(*s);
                return;
            }
            setter(def);
        };
    }

    /// Add a device argument with binding lambda
    /// @param name Argument name (without --)
    /// @param description Help text
    /// @param default_value Default device name
    /// @param setter Lambda: void(std::string_view) - throws Status on failure
    template <typename Func>
        requires std::invocable<Func, std::string_view>
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward) - std::forward used in lambda capture
    void add_device(std::string_view name, std::string_view description, std::string_view default_value, Func&& setter)
    {
        specs_.push_back(ArgumentSpec{
            .name = std::string{name},
            .type = ArgType::Device,
            .description = std::string{description},
            .default_value = std::string{default_value},
        });

        auto& spec = specs_.back();
        spec.apply_binding = [setter = std::forward<Func>(setter), name_str = std::string{name}, def = std::string{default_value}](
                                 toml::Table const& root) -> void {
            auto const* v = root.get_path(name_str);
            if (v == nullptr) {
                setter(def);
                return;
            }
            if (auto s = v->as_string(); s.has_value()) {
                setter(*s);
                return;
            }
            setter(def);
        };
    }

    /// Add a typed list argument without binding
    /// Comma-separated values on CLI: `--name=V1,V2,V3`
    /// Type T determines the element ArgType via ArgTypeTraits
    /// @param name Argument name (without --)
    /// @param description Help text
    template <typename T>
        requires requires { ArgTypeTraits<T>::arg_type; }
    auto add_list(std::string_view name, std::string_view description) -> void
    {
        specs_.push_back(ArgumentSpec{
            .name = std::string{name},
            .type = ArgTypeTraits<T>::arg_type,
            .description = std::string{description},
        });
        specs_.back().is_list = true;
    }

    /// Add a typed list argument with binding lambda
    /// Comma-separated values on CLI: `--name=V1,V2,V3`
    /// The binding receives std::vector<T> const& with parsed elements.
    /// Elements are split on commas, parsed via parse_value_string(), stored as
    /// an Array in config, then retrieved via get_list<T>().
    /// @param name Argument name (without --)
    /// @param description Help text
    /// @param setter Lambda: void(std::vector<T> const&)
    template <typename T, typename Func>
        requires requires { ArgTypeTraits<T>::arg_type; } && std::invocable<Func, std::vector<T> const&>
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward) - std::forward used in lambda capture
    void add_list(std::string_view name, std::string_view description, Func&& setter)
    {
        specs_.push_back(ArgumentSpec{
            .name = std::string{name},
            .type = ArgTypeTraits<T>::arg_type,
            .description = std::string{description},
        });

        auto& spec = specs_.back();
        spec.is_list = true;
        spec.apply_binding = [setter = std::forward<Func>(setter), name_str = std::string{name}](toml::Table const& root) -> void {
            // If the value is already an array (from TOML file), just use get_list directly
            auto const* v = root.get_path(name_str);
            if (v != nullptr && v->as_array() != nullptr) {
                setter(toml::get_list<T>(root, name_str));
                return;
            }

            // Otherwise, it's a CLI string that needs comma-splitting
            if (v == nullptr) {
                setter(std::vector<T>{});
                return;
            }
            auto raw = v->as_string();
            if (!raw.has_value()) {
                // Non-string scalar (integer, float, bool) — treat as single-element list
                setter(toml::get_list<T>(root, name_str));
                return;
            }
            if (raw->empty()) {
                setter(std::vector<T>{});
                return;
            }

            // Split on commas and parse each element
            toml::Array arr;
            std::string_view remaining{*raw};
            while (!remaining.empty()) {
                auto pos = remaining.find(',');
                std::string_view element;
                if (pos == std::string_view::npos) {
                    element = remaining;
                    remaining = {};
                } else {
                    element = remaining.substr(0, pos);
                    remaining = remaining.substr(pos + 1);
                }
                // Skip empty elements (trailing/double commas)
                if (element.empty()) {
                    continue;
                }
                arr.push_back(toml::parse_value_string(element));
            }

            // Store the parsed array back into a temporary table for get_list
            toml::Table temp;
            temp.set(name_str, toml::Value{std::move(arr)});
            setter(toml::get_list<T>(temp, name_str));
        };
    }

    /// Apply all bindings from config to bound destinations
    /// @param root The parsed configuration table
    /// @return Status - success or failure with error code (from thrown Status exception)
    [[nodiscard]] auto apply(toml::Table const& root) const -> Status;

    /// Get all specs
    [[nodiscard]] auto specs() const -> std::vector<ArgumentSpec> const& { return specs_; }

    /// Check if a key is known (matches any registered argument spec)
    /// @param key The key to check (e.g., "ptp.device", "help")
    /// @return true if the key matches a registered spec
    [[nodiscard]] auto is_known(std::string_view key) const -> bool;

    /// Find unknown keys in a table, checking against registered specs
    /// Recursively checks all keys in the table
    /// @param root The table to check
    /// @return Vector of unknown key names
    [[nodiscard]] auto find_unknown_keys(toml::Table const& root) const -> std::vector<std::string>;

    /// Populate a Table with all default values from the specs
    /// This is useful for dumping the complete configuration including defaults.
    /// @param root The table to populate with defaults
    /// @param include_all When true, includes arguments with empty defaults as empty strings
    auto populate_defaults(toml::Table& root, bool include_all = false) const -> void;

    /// Create a new Table populated with all default values from the specs
    /// @return Table with all defaults set
    [[nodiscard]] auto create_default_config() const -> toml::Table;

    /// Merge another ArgumentSpecs into this one, optionally adding a prefix to all keys
    /// This allows composing specs from multiple sources with namespacing.
    ///
    /// Note: When using a prefix, the original specs should use unprefixed keys (e.g., "period_ns")
    /// and bindings that use unprefixed keys. The merge will:
    /// 1. Add the prefix to the spec name (for CLI/help display)
    /// 2. Wrap the binding to look up the prefixed key in config
    ///
    /// @param other The specs to merge in
    /// @param prefix Optional prefix to add to all keys (e.g., "timer." results in "timer.period_ns")
    auto merge(ArgumentSpecs const& other, std::string_view prefix = {}) -> void;

    /// Generate bash completion script
    /// @param program_name Name of the program for which to generate completions
    [[nodiscard]] auto generate_bash(std::string_view program_name) const -> std::string;

    /// Generate zsh completion script
    /// @param program_name Name of the program for which to generate completions
    [[nodiscard]] auto generate_zsh(std::string_view program_name) const -> std::string;

    /// Generate fish completion script
    /// @param program_name Name of the program for which to generate completions
    [[nodiscard]] auto generate_fish(std::string_view program_name) const -> std::string;

    /// Format help text to output iterator
    /// Each option is formatted as:
    ///   --name=TYPE :
    ///         Description text (default: value)
    ///         Valid values: a, b, c
    ///
    /// @param out Output iterator to write help text to
    /// @param prefix String prepended to each option line (default: two spaces)
    template <typename OutputIt>
    auto format_help_to(OutputIt out, std::string_view prefix = "  ") const -> OutputIt
    {
        for (auto const& spec : specs_) {
            // First line: option name with type placeholder
            out = std::format_to(out, "{}--{}", prefix, spec.name);

            if (spec.type != ArgType::Flag) {
                out = std::format_to(out, "={}", arg_type_placeholder(spec.type));
                if (spec.is_list) {
                    out = std::format_to(out, ",...");
                }
            }

            out = std::format_to(out, " :\n");

            // Second line: description indented 8 spaces
            out = std::format_to(out, "        {}", spec.description);

            if (!spec.default_value.empty()) {
                out = std::format_to(out, " (default: {})", spec.default_value);
            }

            out = std::format_to(out, "\n");

            // Show choices if present (also indented 8 spaces)
            if (spec.type == ArgType::Choice && !spec.choices.empty()) {
                out = std::format_to(out, "        Valid values: ");
                for (size_t i = 0; i < spec.choices.size(); ++i) {
                    if (i > 0) {
                        out = std::format_to(out, ", ");
                    }
                    out = std::format_to(out, "{}", spec.choices[i]);
                }
                out = std::format_to(out, "\n");
            }

            // Blank line between options
            out = std::format_to(out, "\n");
        }
        return out;
    }

  private:
    /// Generate bash completion code for a single argument's value
    template <typename OutputIt>
    static auto format_bash_value_completion(OutputIt out, ArgumentSpec const& spec) -> OutputIt
    {
        switch (spec.type) {
            case ArgType::Choice:
                out = std::format_to(out, "            COMPREPLY=( $(compgen -W \"");
                for (size_t i = 0; i < spec.choices.size(); ++i) {
                    if (i > 0) {
                        out = std::format_to(out, " ");
                    }
                    out = std::format_to(out, "{}", spec.choices[i]);
                }
                out = std::format_to(out, "\" -- \"${{cur}}\") )\n");
                out = std::format_to(out, "            return 0\n");
                break;
            case ArgType::File:
                out = std::format_to(out, "            COMPREPLY=( $(compgen -f -- \"${{cur}}\") )\n");
                out = std::format_to(out, "            return 0\n");
                break;
            case ArgType::Directory:
                out = std::format_to(out, "            COMPREPLY=( $(compgen -d -- \"${{cur}}\") )\n");
                out = std::format_to(out, "            return 0\n");
                break;
            case ArgType::Device:
                out = std::format_to(out, "            # List network interfaces\n");
                out = std::format_to(out, "            if command -v ip &> /dev/null; then\n");
                out = std::format_to(
                    out,
                    "                COMPREPLY=( $(compgen -W \"$(ip -o link show | awk -F': ' '{{print $2}}')\" -- "
                    "\"${{cur}}\") )\n");
                out = std::format_to(out, "            elif command -v ifconfig &> /dev/null; then\n");
                out = std::format_to(
                    out,
                    "                COMPREPLY=( $(compgen -W \"$(ifconfig -l 2>/dev/null || ifconfig | grep -oE "
                    "'^[a-z0-9]+')\" -- \"${{cur}}\") )\n");
                out = std::format_to(out, "            fi\n");
                out = std::format_to(out, "            return 0\n");
                break;
            default:
                out = std::format_to(out, "            return 0\n");
                break;
        }
        return out;
    }

    [[nodiscard]] static auto get_value_description(ArgumentSpec const& spec) -> std::string;

    /// Recursively collect unknown keys from a table
    void collect_unknown_keys(toml::Table const& table, std::string const& prefix, std::vector<std::string>& unknown) const;

    std::vector<ArgumentSpec> specs_;
};

}  // namespace statusbar::args
