// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/config/config_error.hpp"

#include <string>
#include <system_error>

namespace statusbar::config {

auto ConfigErrorCategory::message(int ev) const -> std::string
{
    switch (static_cast<ConfigError>(ev)) {
        case ConfigError::builtin_handled:
            return "Built-in CLI command handled (exit cleanly)";
        case ConfigError::unknown_option:
            return "Unknown command-line option";
        default:
            return "Unknown config error";
    }
}

auto config_error_category() noexcept -> std::error_category const&
{
    static ConfigErrorCategory const instance;
    return instance;
}

auto make_error_code(ConfigError e) noexcept -> std::error_code
{
    return {static_cast<int>(e), config_error_category()};
}

}  // namespace statusbar::config
