#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// @file args.hpp
/// @brief Args Module — CLI argument specification, type traits, and shell completion
///
/// Provides a declarative system for defining CLI arguments with type-safe
/// binding and automatic shell completion generation (bash/zsh).
///
/// Usage:
/// @code
///   statusbar::args::ArgumentSpecs specs;
///   specs.add("timecode", ArgType::String, "Start timecode in HH:MM:SS:FF format", "00:00:00:00");
///   specs.add("fps", ArgType::Choice, "Frame rate", "30ndf", {"23.976", "24", "25", "29.97df", "30ndf"});
///   specs.add("help", ArgType::Flag, "Show help message");
///
///   // Generate shell completion script
///   std::print("{}", specs.generate_bash("my_tool"));
/// @endcode

#include "statusbar/args/args_spec.hpp"

namespace statusbar::args {}
