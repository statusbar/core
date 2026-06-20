#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Config CLI - Command-line parsing integration
/// Combines Config + ArgumentSpecs for complete CLI handling

#include "statusbar/args/args_spec.hpp"
#include "statusbar/config/config_error.hpp"
#include "statusbar/config/config_store.hpp"
#include "statusbar/status/status.hpp"

#include <cstdlib>
#include <span>
#include <string_view>
#include <type_traits>

namespace statusbar::config {

/// Type alias for help callback function
using HelpCallback = void (*)(char const* program_name, args::ArgumentSpecs const& specs);

/// Default help callback that prints usage and options to stderr
auto default_print_usage(char const* program_name, args::ArgumentSpecs const& specs) -> void;

/// Render usage with an optional one-line/multi-line description and a list of
/// example invocations. Each example is printed as `  {program_name} {example}`
/// (so callers pass only the argument portion). All output goes to stderr.
/// Lets a tool's `print_usage` shrink to a single call instead of re-emitting
/// the Usage/Options/format_help/Examples skeleton.
auto default_print_usage(
    char const* program_name,
    args::ArgumentSpecs const& specs,
    std::string_view description,
    std::span<std::string_view const> examples = {}) -> void;

/// Check for --completion option and output shell completion script if requested
[[nodiscard]] auto handle_completion(Config const& config, args::ArgumentSpecs const& specs, std::string_view program_path) -> bool;

/// Check if a Status indicates a handled built-in command (help, completion, config-save)
[[nodiscard]] auto handled_builtin_command(Status const& status) -> bool;

/// Compute the default user configuration file path for a program.
///
/// Resolution order:
///   1. `$XDG_CONFIG_HOME/$program_name/config.toml` if XDG_CONFIG_HOME is set and non-empty
///   2. `$HOME/.config/$program_name/config.toml` otherwise
///
/// Returns an empty string if program_name is empty, or if neither XDG_CONFIG_HOME
/// nor HOME is set in the environment.
///
/// @param program_name Short program identifier used as the config subdirectory name
///                     (e.g., "my-tool" produces "~/.config/my-tool/config.toml")
[[nodiscard]] auto default_config_path(std::string_view program_name) -> std::string;

/// Parse CLI arguments, handle special options, and apply spec bindings
///
/// This function handles the complete CLI parsing pattern:
/// 1. Load the default user config file (if `program_name` is non-empty and
///    `--no-default-config` is not set). Silent on missing file.
/// 2. Process `--config-load=FILE` options (multiple allowed, loaded in order)
/// 3. Parse remaining CLI arguments into a Config object
/// 4. Check for `--completion` and output shell completion if requested
/// 5. Check for `--help` and call the help callback if provided
/// 6. Check for `--config-save=FILE` and save config if requested
/// 7. Apply argument spec bindings to populate configuration
///
/// Cascade order (later overrides earlier):
///   default user config -> --config-load files -> CLI arguments
///
/// @param argc Argument count from main()
/// @param argv Argument values from main()
/// @param specs The argument specifications with optional bindings
/// @param help_callback Function to call when `--help` is requested (may be nullptr)
/// @param program_name Short program name used to locate the default user config
///                     at `$XDG_CONFIG_HOME/$program_name/config.toml` (or
///                     `$HOME/.config/$program_name/config.toml`). Leave empty
///                     to skip default config loading.
/// @return Status - success or failure with error code
///
/// On failure, check handled_builtin_command() to determine exit code:
/// - If handled_builtin_command() returns true: exit with 0 (help, completion, config-save)
/// - Otherwise: exit with 1 (actual error)
[[nodiscard]] auto parse_cli_args(
    int argc,
    char* argv[],
    args::ArgumentSpecs& specs,
    HelpCallback help_callback = default_print_usage,
    std::string_view program_name = {}) -> Status;

/// Parse configuration or exit the program
/// Calls the provided parser function and either returns the parsed config,
/// or exits with appropriate status code if parsing fails or a built-in
/// command (`--help`, `--completion`, `--config-save`) was handled.
///
/// @tparam ParserFunc Function type that takes (int argc, char** argv) and returns StatusValue<Config>
/// @param parser_func The parser function to call
/// @param argc Argument count from main()
/// @param argv Argument vector from main()
/// @return The parsed configuration (exits on failure)
template <typename ParserFunc>
auto parse_config_or_exit(ParserFunc parser_func, int argc, char** argv)
    -> std::invoke_result_t<ParserFunc, int, char**>::value_type
{
    auto config_result = parser_func(argc, argv);
    if (!config_result) {
        if (config_result.error() == make_error_code(ConfigError::builtin_handled)) {
            std::exit(EXIT_SUCCESS);
        }
        std::exit(EXIT_FAILURE);
    }
    return std::move(*config_result);
}

}  // namespace statusbar::config
