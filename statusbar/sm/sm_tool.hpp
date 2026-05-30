#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

//
// State Machine Documentation Tool - reusable CLI for generating SM docs
//

#include "statusbar/args/args.hpp"
#include "statusbar/sm/sm_registry.hpp"

#include <expected>
#include <filesystem>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace statusbar::sm {

/// Configuration for SM documentation tool
struct SmToolConfig
{
    OutputFormat format{OutputFormat::Dot};
    std::vector<std::string> machines;  // Empty means all
    std::string output_dir;             // Empty means stdout
    bool output_all_files{false};       // Output all machines to separate files
    bool show_help{false};              // Show help and exit
};

namespace detail {

auto build_arg_specs(SmToolConfig& config) -> args::ArgumentSpecs;

void print_usage(char const* program_name, args::ArgumentSpecs const& specs, std::span<StateMachineInfo const> state_machines);

auto should_output(SmToolConfig const& config, std::string_view name) -> bool;

auto output_machines(SmToolConfig const& config, std::span<StateMachineInfo const> state_machines) -> bool;

auto validate_machines(SmToolConfig const& config, std::span<StateMachineInfo const> state_machines) -> bool;

/// Check if help was requested in command line arguments
auto has_help_arg(int argc, char* argv[]) -> bool;

}  // namespace detail

/// Run the state machine documentation tool (span overload)
/// @param argc Command line argument count
/// @param argv Command line arguments
/// @param state_machines Span of state machine info entries
/// @return Exit code (0 for success)
auto sm_tool(int argc, char* argv[], std::span<StateMachineInfo const> state_machines) -> int;

/// Run the state machine documentation tool (array overload)
/// @param argc Command line argument count
/// @param argv Command line arguments
/// @param state_machines Array of state machine info entries
/// @return Exit code (0 for success)
///
/// Example usage:
/// @code
/// constexpr auto machines = std::array{
///     make_sm_info<my_sm::Machine>("my_sm", "My State Machine"),
/// };
///
/// int main(int argc, char* argv[])
/// {
///     return statusbar::sm::sm_tool(argc, argv, machines);
/// }
/// @endcode
template <size_t N>
auto sm_tool(int argc, char* argv[], std::array<StateMachineInfo, N> const& state_machines) -> int
{
    return sm_tool(argc, argv, std::span<StateMachineInfo const>{state_machines});
}

}  // namespace statusbar::sm
