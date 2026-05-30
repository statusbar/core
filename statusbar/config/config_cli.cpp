// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/config/config_cli.hpp"

#include "statusbar/args/args_spec.hpp"
#include "statusbar/config/config_error.hpp"
#include "statusbar/config/config_store.hpp"
#include "statusbar/status/status.hpp"
#include "statusbar/toml/toml_value.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iterator>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace statusbar::config {

auto handle_completion(Config const& config, args::ArgumentSpecs const& specs, std::string_view program_path) -> bool
{
    auto completion_shell = config.get_string("completion");
    if (!completion_shell.has_value()) {
        return false;
    }

    std::filesystem::path const path{program_path};
    std::string const program_name = path.filename().string();

    if (*completion_shell == "bash") {
        std::print("{}", specs.generate_bash(program_name));
    } else if (*completion_shell == "zsh") {
        std::print("{}", specs.generate_zsh(program_name));
    } else if (*completion_shell == "fish") {
        std::print("{}", specs.generate_fish(program_name));
    } else {
        std::println(stderr, "Error: Unknown shell '{}'. Use: bash, zsh, or fish", *completion_shell);
        return true;  // Still return true so caller exits (with error)
    }
    return true;
}

auto handled_builtin_command(Status const& status) -> bool
{
    return !status && status.error() == make_error_code(ConfigError::builtin_handled);
}

auto default_config_path(std::string_view program_name) -> std::string
{
    if (program_name.empty()) {
        return {};
    }

    if (char const* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::format("{}/{}/config.toml", xdg, program_name);
    }

    char const* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') {
        return {};
    }
    return std::format("{}/.config/{}/config.toml", home, program_name);
}

void default_print_usage(char const* program_name, args::ArgumentSpecs const& specs)
{
    std::print("Usage: {} [options]\n", program_name);
    std::print(stderr, "\nOptions:\n");

    std::string help;
    specs.format_help_to(std::back_inserter(help));
    std::print(stderr, "{}", help);
}

namespace {

struct ConfigFileOptions
{
    bool skip_default_config = false;
    std::vector<std::string> load_files;
    std::string save_file;
    std::string full_save_file;
};

void add_builtin_options(args::ArgumentSpecs& specs)
{
    specs.add_flag("help", "Show this help message");
    specs.add_file("config-load", "Load configuration from TOML file (multiple allowed)");
    specs.add_file("config-save", "Save configuration to TOML file and exit");
    specs.add_file("config-full-save", "Save complete configuration with all defaults to TOML file and exit");
    specs.add_flag("config-dump", "Dump all settings (including defaults) as TOML and exit");
    specs.add_flag("no-default-config", "Skip loading the default user config file");
    specs.add_choice("completion", "Generate shell completion script", {"bash", "zsh", "fish"});
}

// First pass over argv: pick out --config-* options the wrapper itself owns.
// These need to be known before we load any config files.
auto scan_config_file_args(int argc, char* argv[]) -> ConfigFileOptions
{
    ConfigFileOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string_view const arg{argv[i]};
        if (arg == "--no-default-config" || arg == "--no-default-config=true") {
            opts.skip_default_config = true;
        } else if (arg.starts_with("--config-load=")) {
            opts.load_files.emplace_back(arg.substr(14));
        } else if (arg == "--config-load" && i + 1 < argc) {
            opts.load_files.emplace_back(argv[++i]);
        } else if (arg.starts_with("--config-save=")) {
            opts.save_file = std::string{arg.substr(14)};
        } else if (arg == "--config-save" && i + 1 < argc) {
            opts.save_file = argv[++i];
        } else if (arg.starts_with("--config-full-save=")) {
            opts.full_save_file = std::string{arg.substr(19)};
        } else if (arg == "--config-full-save" && i + 1 < argc) {
            opts.full_save_file = argv[++i];
        }
    }
    return opts;
}

auto load_config_files(Config& config, std::string_view program_name, ConfigFileOptions const& opts) -> Status
{
    if (!opts.skip_default_config && !program_name.empty()) {
        auto default_path = default_config_path(program_name);
        if (!default_path.empty()) {
            auto status = config.load_file_if_exists(default_path);
            if (!status) {
                std::println(stderr, "Error: Failed to load default config file '{}': {}", default_path, status.error().message());
                return failure(status.error());
            }
        }
    }
    for (auto const& file : opts.load_files) {
        auto status = config.load_file(file);
        if (!status) {
            std::println(stderr, "Error: Failed to load config file '{}': {}", file, status.error().message());
            return failure(status.error());
        }
    }
    return success();
}

void warn_unknown_file_keys(Config& config, args::ArgumentSpecs const& specs, bool any_file_loaded)
{
    if (!any_file_loaded) {
        return;
    }
    auto unknown_file_keys = specs.find_unknown_keys(config.root());
    for (auto const& key : unknown_file_keys) {
        std::println(stderr, "Warning: Unknown option '{}' in config file (ignored)", key);
    }
}

auto cli_arg_key(std::string_view arg) -> std::string_view
{
    if (arg.starts_with("--")) {
        return arg.substr(2);
    }
    if (arg.starts_with("-") && arg.size() > 1 && std::isdigit(static_cast<unsigned char>(arg[1])) == 0) {
        return arg.substr(1);
    }
    return {};
}

// Second pass: every --key / -k seen in argv must be either a spec or
// one of the wrapper-owned names. Unknown keys are a hard error.
auto validate_cli_args(int argc, char* argv[], args::ArgumentSpecs const& specs) -> Status
{
    std::vector<std::string> unknown_cli_args;
    for (int i = 1; i < argc; ++i) {
        std::string_view const raw{argv[i]};
        auto const arg = cli_arg_key(raw);
        if (arg.empty()) {
            continue;  // Not an option
        }

        size_t const eq = arg.find('=');
        std::string const key = eq != std::string_view::npos ? std::string{arg.substr(0, eq)} : std::string{arg};

        // "config" is handled specially by Config::apply_cli_overrides.
        if (key != "config" && !specs.is_known(key)) {
            unknown_cli_args.push_back(key);
        }

        // --key value (not --key=value): skip the following value token so
        // we don't misread it as another option.
        if (eq == std::string_view::npos && i + 1 < argc) {
            std::string_view const next{argv[i + 1]};
            if (!next.starts_with("-") || (next.size() > 1 && std::isdigit(static_cast<unsigned char>(next[1])) != 0)) {
                ++i;
            }
        }
    }

    if (unknown_cli_args.empty()) {
        return success();
    }
    for (auto const& key : unknown_cli_args) {
        std::println(stderr, "Error: Unknown option '--{}'", key);
    }
    return failure(ConfigError::unknown_option);
}

void erase_builtin_options(Config& config)
{
    auto& root = config.root();
    root.erase("help");
    root.erase("config-load");
    root.erase("config-save");
    root.erase("config-full-save");
    root.erase("config-dump");
    root.erase("no-default-config");
    root.erase("completion");
}

// Shared write path for --config-save and --config-full-save. Both end
// with ConfigError::builtin_handled to tell the caller "built-in handled, exit".
auto save_config_and_exit(
    Config& config, args::ArgumentSpecs& specs, std::string const& path, bool with_defaults, std::string_view success_prefix)
    -> Status
{
    if (with_defaults) {
        specs.populate_defaults(config.root(), true);
    }
    erase_builtin_options(config);
    auto status = config.write_file(path, specs);
    if (!status) {
        std::println(stderr, "Error: Failed to save config file '{}': {}", path, status.error().message());
        return failure(status.error());
    }
    std::println("{}: {}", success_prefix, path);
    return failure(ConfigError::builtin_handled);
}

auto dump_config_and_exit(Config& config, args::ArgumentSpecs& specs) -> Status
{
    specs.populate_defaults(config.root());
    erase_builtin_options(config);
    std::print("{}", config.to_toml_string(specs));
    return failure(ConfigError::builtin_handled);
}

}  // namespace

auto parse_cli_args(int argc, char* argv[], args::ArgumentSpecs& specs, HelpCallback help_callback, std::string_view program_name)
    -> Status
{
    add_builtin_options(specs);

    Config config;
    auto const opts = scan_config_file_args(argc, argv);

    if (auto status = load_config_files(config, program_name, opts); !status) {
        return status;
    }
    warn_unknown_file_keys(config, specs, !opts.load_files.empty());

    (void)config.apply_cli_overrides(argc, argv, specs);
    if (auto status = validate_cli_args(argc, argv, specs); !status) {
        return status;
    }

    if (handle_completion(config, specs, argv[0])) {
        return failure(ConfigError::builtin_handled);
    }
    if (config.get_boolean("help", false)) {
        if (help_callback != nullptr) {
            help_callback(argv[0], specs);
        }
        return failure(ConfigError::builtin_handled);
    }

    auto apply_result = specs.apply(config.root());
    if (!apply_result) {
        return apply_result;
    }

    if (!opts.save_file.empty()) {
        return save_config_and_exit(config, specs, opts.save_file, false, "Configuration saved to");
    }
    if (!opts.full_save_file.empty()) {
        return save_config_and_exit(config, specs, opts.full_save_file, true, "Configuration (with defaults) saved to");
    }
    if (config.get_boolean("config-dump", false)) {
        return dump_config_and_exit(config, specs);
    }

    return success();
}

}  // namespace statusbar::config
