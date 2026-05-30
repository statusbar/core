#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

//
// State Machine Registry - type-erased registry for documentation tools
//

#include "statusbar/sm/sm_base.hpp"
#include "statusbar/sm/sm_core.hpp"
#include "statusbar/sm/sm_dot.hpp"
#include "statusbar/sm/sm_markdown.hpp"

#include <filesystem>
#include <fstream>
#include <print>
#include <string_view>

namespace statusbar::sm {

/// Output format for state machine documentation
enum class OutputFormat
{
    Dot,
    Markdown
};

/// Information about a state machine for documentation generation
/// Uses function pointers for type erasure so different Machine types
/// can be stored in a single array.
///
/// `reference` is an optional standard / RFC / clause citation. When
/// present, it's emitted under the title in Markdown output as a
/// "**Implements:** {reference}" line so readers see immediately which
/// part of which standard the SM derives from. Empty for SMs that
/// don't trace to a published spec (application-level orchestrators,
/// demos, etc.).
struct StateMachineInfo
{
    std::string_view symbol_name;  // e.g., "supervisor_sm"
    std::string_view human_name;   // e.g., "Device Supervisor"
    std::string_view reference;    // e.g., "IEEE 1722.1-2021 Clause 8.2.3" (optional)

    // Function pointers to type-erased output functions
    void (*output_to_stdout)(OutputFormat format, std::string_view title, std::string_view reference);
    bool (*output_to_file)(
        OutputFormat format, std::string_view title, std::string_view reference, std::filesystem::path const& filepath);
};

/// Output a state machine to stdout (type-erased wrapper)
template <typename Machine>
void output_machine_to_stdout(OutputFormat format, std::string_view title, std::string_view reference)
{
    if (format == OutputFormat::Dot) {
        auto dot = generate_dot<Machine>();
        std::print("{}", dot.view());
    } else {
        std::println("## {}", title);
        if (!reference.empty()) {
            std::println("\n**Implements:** {}", reference);
        }
        std::println("");
        auto md = generate_markdown_table<Machine>();
        std::println("{}", md.view());
    }
}

/// Output a state machine to a file (type-erased wrapper)
template <typename Machine>
auto output_machine_to_file(
    OutputFormat format, std::string_view title, std::string_view reference, std::filesystem::path const& filepath) -> bool
{
    std::ofstream file(filepath);
    if (!file) {
        std::println(stderr, "Error: Cannot open file for writing: {}", filepath.string());
        return false;
    }

    if (format == OutputFormat::Dot) {
        auto dot = generate_dot<Machine>();
        file << dot.view();
    } else {
        file << "# " << title << "\n\n";
        if (!reference.empty()) {
            file << "**Implements:** " << reference << "\n\n";
        }
        auto md = generate_markdown_table<Machine>();
        file << md.view();
    }

    std::println("  Wrote: {}", filepath.string());
    return true;
}

/// Helper to create a StateMachineInfo entry for a given Machine type.
/// The optional third argument is a standard / RFC / clause citation
/// that gets emitted under the title in the generated Markdown.
///
/// @code
/// constexpr auto machines = std::array{
///     make_sm_info<my_sm::Machine>("my_sm", "My State Machine"),
///     make_sm_info<acmp::Controller>("acmp_controller_sm", "ACMP Controller",
///                                    "IEEE 1722.1-2021 Clause 8.2.3"),
/// };
/// @endcode
template <typename Machine>
consteval auto make_sm_info(std::string_view symbol, std::string_view human, std::string_view reference = {}) -> StateMachineInfo
{
    return StateMachineInfo{
        .symbol_name = symbol,
        .human_name = human,
        .reference = reference,
        .output_to_stdout = &output_machine_to_stdout<Machine>,
        .output_to_file = &output_machine_to_file<Machine>,
    };
}

}  // namespace statusbar::sm
