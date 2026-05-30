// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/sm/sm_tool.hpp"

#include "statusbar/config/config.hpp"
#include "statusbar/sm/sm_registry.hpp"

#include <filesystem>
#include <iterator>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace statusbar::sm {

namespace detail {

auto build_arg_specs(SmToolConfig& config) -> args::ArgumentSpecs
{
    args::ArgumentSpecs specs;

    specs.add_choice("format", "Output format", {"dot", "markdown"}, "dot", [&](auto const& v) -> void {
        if (v == "dot") {
            config.format = OutputFormat::Dot;
        } else if (v == "markdown") {
            config.format = OutputFormat::Markdown;
        }
    });

    specs.add<std::string_view>(
        "machine",
        "State machine name (can be specified multiple times, or 'all' for all machines)",
        "all",
        [&](auto const& v) -> void {
            if (v != "all") {
                config.machines.emplace_back(v);
            }
        });

    specs.add<std::string_view>(
        "output-dir",
        "Output directory for separate files (writes all machines to individual files)",
        "",
        [&](auto const& v) -> void {
            if (!v.empty()) {
                config.output_dir = std::string{v};
                config.output_all_files = true;
            }
        });

    return specs;
}

void print_usage(char const* program_name, args::ArgumentSpecs const& specs, std::span<StateMachineInfo const> state_machines)
{
    std::println("Usage: {} [options]", program_name);
    std::println("\nGenerates DOT (Graphviz) or Markdown documentation for state machines.");
    std::println("\nOptions:");

    std::string help;
    specs.format_help_to(std::back_inserter(help));
    std::print("{}", help);

    std::println("\nAvailable state machines:");
    for (auto const& sm : state_machines) {
        std::println("  {:<20} - {}", sm.symbol_name, sm.human_name);
    }

    std::println("\nExamples:");
    std::println("  {} --format=dot                          # All machines as DOT to stdout", program_name);
    std::println("  {} --format=markdown                     # All machines as Markdown to stdout", program_name);
    std::println("  {} --format=dot --machine=<name>         # Single machine as DOT", program_name);
    std::println("  {} --format=markdown --machine=<name>    # Single machine as Markdown", program_name);
    std::println("\nOutput to separate files:");
    std::println("  {} --output-dir=docs/sm                  # All machines as .dot files", program_name);
    std::println("  {} --output-dir=docs/sm --format=markdown  # All machines as .md files", program_name);
    std::println("\nTo render DOT output:");
    std::println("  {} --format=dot | dot -Tpng -o state_machine.png", program_name);
    std::println("  {} --format=dot | dot -Tsvg -o state_machine.svg", program_name);
}

auto should_output(SmToolConfig const& config, std::string_view name) -> bool
{
    if (config.machines.empty()) {
        return true;  // Output all
    }
    for (auto const& m : config.machines) {
        if (m == name) {
            return true;
        }
    }
    return false;
}

auto output_machines(SmToolConfig const& config, std::span<StateMachineInfo const> state_machines) -> bool
{
    // Output to separate files
    if (config.output_all_files) {
        std::filesystem::path const dir{config.output_dir};

        // Create directory if it doesn't exist
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            std::println(stderr, "Error: Cannot create directory '{}': {}", config.output_dir, ec.message());
            return false;
        }

        std::string_view const ext = (config.format == OutputFormat::Dot) ? ".dot" : ".md";
        std::println("Writing state machine files to: {}", config.output_dir);

        bool success = true;
        for (auto const& sm : state_machines) {
            if (should_output(config, sm.symbol_name)) {
                auto filepath = dir / (std::string{sm.symbol_name} + std::string{ext});
                success &= sm.output_to_file(config.format, sm.human_name, sm.reference, filepath);
            }
        }
        return success;
    }

    // Output to stdout
    if (config.format == OutputFormat::Markdown) {
        std::println("# State Machines\n");
    }

    for (auto const& sm : state_machines) {
        if (should_output(config, sm.symbol_name)) {
            sm.output_to_stdout(config.format, sm.human_name, sm.reference);
        }
    }

    return true;
}

auto validate_machines(SmToolConfig const& config, std::span<StateMachineInfo const> state_machines) -> bool
{
    for (auto const& m : config.machines) {
        bool found = false;
        for (auto const& sm : state_machines) {
            if (m == sm.symbol_name) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::println(stderr, "Error: Unknown state machine '{}'\n", m);
            std::println(stderr, "Available state machines:");
            for (auto const& sm : state_machines) {
                std::println(stderr, "  {}", sm.symbol_name);
            }
            return false;
        }
    }
    return true;
}

auto is_help_flag(std::string_view arg) -> bool
{
    return arg == "--help" || arg == "-h" || arg == "-help";
}

auto has_help_arg(int argc, char* argv[]) -> bool
{
    for (int i = 1; i < argc; ++i) {
        if (is_help_flag(std::string_view{argv[i]})) {
            return true;
        }
    }
    return false;
}

}  // namespace detail

auto sm_tool(int argc, char* argv[], std::span<StateMachineInfo const> state_machines) -> int
{
    // Check for help before parsing (so we can print state machines list)
    if (detail::has_help_arg(argc, argv)) {
        SmToolConfig config;
        auto specs = detail::build_arg_specs(config);
        // Add the built-in options that parse_cli_args would add
        specs.add_flag("help", "Show this help message");
        specs.add_file("config-load", "Load configuration from TOML file (multiple allowed)");
        specs.add_file("config-save", "Save configuration to TOML file and exit");
        specs.add_flag("config-dump", "Dump all settings (including defaults) as TOML and exit");
        specs.add_choice("completion", "Generate shell completion script", {"bash", "zsh", "fish"});
        detail::print_usage(argv[0], specs, state_machines);
        return 0;
    }

    SmToolConfig config;
    auto specs = detail::build_arg_specs(config);

    auto cli_result = config::parse_cli_args(argc, argv, specs, nullptr);
    if (!cli_result) {
        return config::handled_builtin_command(cli_result) ? 0 : 1;
    }

    if (!detail::validate_machines(config, state_machines)) {
        return 1;
    }

    return detail::output_machines(config, state_machines) ? 0 : 1;
}

}  // namespace statusbar::sm
