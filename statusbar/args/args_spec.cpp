// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/args/args_spec.hpp"

#include "statusbar/status/status.hpp"
#include "statusbar/toml/toml_value.hpp"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace statusbar::args {

void ArgumentSpecs::add_flag(std::string_view name, std::string_view description)
{
    specs_.push_back(
        ArgumentSpec{
            .name = std::string{name},
            .type = ArgType::Flag,
            .description = std::string{description},
        });
}

void ArgumentSpecs::add_file(std::string_view name, std::string_view description, std::string_view default_value)
{
    specs_.push_back(
        ArgumentSpec{
            .name = std::string{name},
            .type = ArgType::File,
            .description = std::string{description},
            .default_value = std::string{default_value},
        });
}

void ArgumentSpecs::add_choice(
    std::string_view name, std::string_view description, std::initializer_list<char const*> choices, std::string_view default_value)
{
    std::vector<std::string> choice_vec;
    choice_vec.reserve(choices.size());
    for (auto const* c : choices) {
        choice_vec.emplace_back(c);
    }
    specs_.push_back(
        ArgumentSpec{
            .name = std::string{name},
            .type = ArgType::Choice,
            .description = std::string{description},
            .default_value = std::string{default_value},
            .choices = std::move(choice_vec),
        });
}

auto ArgumentSpecs::apply(toml::Table const& root) const -> Status
{
    for (auto const& spec : specs_) {
        if (static_cast<bool>(spec.apply_binding)) {
#if __cpp_exceptions
            try {
#endif
                spec.apply_binding(root);
#if __cpp_exceptions
            } catch (Status const& status) {
                return status;
            }
#endif
        }
    }
    return success();
}

auto ArgumentSpecs::is_known(std::string_view key) const -> bool
{
    for (auto const& spec : specs_) {
        if (spec.name == key) {
            return true;
        }
    }
    return false;
}

auto ArgumentSpecs::find_unknown_keys(toml::Table const& root) const -> std::vector<std::string>
{
    std::vector<std::string> unknown;
    collect_unknown_keys(root, "", unknown);
    return unknown;
}

void ArgumentSpecs::populate_defaults(toml::Table& root, bool include_all) const
{
    for (auto const& spec : specs_) {
        if (spec.default_value.empty() && !include_all) {
            continue;  // No default value and not including all
        }
        // Only set if not already present in config
        if (root.get_path(spec.name) == nullptr) {
            auto const value =
                spec.default_value.empty() ? toml::Value{std::string{""}} : toml::parse_value_string(spec.default_value);
            auto dot = spec.name.rfind('.');
            if (dot != std::string::npos) {
                auto* parent = root.get_or_create_table(spec.name.substr(0, dot));
                if (parent != nullptr) {
                    parent->set(std::string{spec.name.substr(dot + 1)}, value);
                }
            } else {
                root.set(spec.name, value);
            }
        }
    }
}

auto ArgumentSpecs::create_default_config() const -> toml::Table
{
    toml::Table root;
    populate_defaults(root);
    return root;
}

void ArgumentSpecs::merge(ArgumentSpecs const& other, std::string_view prefix)
{
    for (auto const& spec : other.specs_) {
        ArgumentSpec new_spec = spec;
        if (!prefix.empty()) {
            std::string const prefixed_name = std::string{prefix} + spec.name;
            new_spec.name = prefixed_name;

            // If there's a binding, wrap it to use the prefixed key.
            //
            // The wrapping lambda would otherwise capture a copy of
            // the original binding (itself a statusbar::sg14::inplace_function),
            // and storing that lambda back into apply_binding forms a
            // self-referential type erasure that cannot fit in any
            // fixed capacity. Hold the original via std::shared_ptr
            // so the capture is just an 8-byte pointer. Callers that
            // want to control this allocation can construct the
            // binding through std::allocate_shared.
            if (static_cast<bool>(spec.apply_binding)) {
                auto original_binding = std::make_shared<decltype(spec.apply_binding)>(spec.apply_binding);
                auto old_name = spec.name;
                new_spec.apply_binding = [original_binding, old_name, prefixed_name](toml::Table const& root) -> void {
                    toml::Table temp;
                    if (auto const* val = root.get_path(prefixed_name); val != nullptr) {
                        temp.set(old_name, *val);
                    }
                    (*original_binding)(temp);
                };
            }
        }
        specs_.push_back(std::move(new_spec));
    }
}

auto ArgumentSpecs::generate_bash(std::string_view program_name) const -> std::string
{
    std::string result;
    auto out = std::back_inserter(result);

    // Function name - replace dashes/dots with underscores
    std::string func_name = std::string{program_name};
    std::ranges::replace(func_name, '-', '_');
    std::ranges::replace(func_name, '.', '_');

    out = std::format_to(out, "# Bash completion for {}\n", program_name);
    out = std::format_to(out, "# Source this file or add to ~/.bash_completion.d/\n\n");

    out = std::format_to(out, "_{}_completions() {{\n", func_name);
    out = std::format_to(out, "    local cur prev opts\n");
    out = std::format_to(out, "    COMPREPLY=()\n");
    out = std::format_to(out, "    cur=\"${{COMP_WORDS[COMP_CWORD]}}\"\n");
    out = std::format_to(out, "    prev=\"${{COMP_WORDS[COMP_CWORD-1]}}\"\n\n");

    // Build list of all options
    out = std::format_to(out, "    opts=\"");
    for (auto const& spec : specs_) {
        if (spec.type == ArgType::Flag) {
            out = std::format_to(out, "--{} ", spec.name);
        } else {
            out = std::format_to(out, "--{}= ", spec.name);
        }
    }
    out = std::format_to(out, "\"\n\n");

    // Handle value completion based on previous argument
    out = std::format_to(out, "    # Handle value completion for specific options\n");
    out = std::format_to(out, "    case \"${{prev}}\" in\n");

    for (auto const& spec : specs_) {
        if (spec.type == ArgType::Flag) {
            continue;
        }
        out = std::format_to(out, "        --{})\n", spec.name);
        out = format_bash_value_completion(out, spec);
        out = std::format_to(out, "            ;;\n");
    }
    out = std::format_to(out, "    esac\n\n");

    // Handle --option= style (value after equals sign)
    out = std::format_to(out, "    # Handle --option=value style\n");
    out = std::format_to(out, "    if [[ \"${{cur}}\" == --*=* ]]; then\n");
    out = std::format_to(out, "        local opt=\"${{cur%%=*}}\"\n");
    out = std::format_to(out, "        local val=\"${{cur#*=}}\"\n");
    out = std::format_to(out, "        case \"${{opt}}\" in\n");

    for (auto const& spec : specs_) {
        if (spec.type == ArgType::Choice) {
            out = std::format_to(out, "            --{})\n", spec.name);
            out = std::format_to(out, "                COMPREPLY=( $(compgen -W \"");
            for (size_t i = 0; i < spec.choices.size(); ++i) {
                if (i > 0) {
                    out = std::format_to(out, " ");
                }
                out = std::format_to(out, "{}", spec.choices[i]);
            }
            out = std::format_to(out, "\" -- \"${{val}}\") )\n");
            out = std::format_to(out, "                COMPREPLY=( \"${{COMPREPLY[@]/#/${{opt}}=}}\" )\n");
            out = std::format_to(out, "                return 0\n");
            out = std::format_to(out, "                ;;\n");
        } else if (spec.type == ArgType::File) {
            out = std::format_to(out, "            --{})\n", spec.name);
            out = std::format_to(out, "                COMPREPLY=( $(compgen -f -- \"${{val}}\") )\n");
            out = std::format_to(out, "                COMPREPLY=( \"${{COMPREPLY[@]/#/${{opt}}=}}\" )\n");
            out = std::format_to(out, "                return 0\n");
            out = std::format_to(out, "                ;;\n");
        } else if (spec.type == ArgType::Directory) {
            out = std::format_to(out, "            --{})\n", spec.name);
            out = std::format_to(out, "                COMPREPLY=( $(compgen -d -- \"${{val}}\") )\n");
            out = std::format_to(out, "                COMPREPLY=( \"${{COMPREPLY[@]/#/${{opt}}=}}\" )\n");
            out = std::format_to(out, "                return 0\n");
            out = std::format_to(out, "                ;;\n");
        }
    }
    out = std::format_to(out, "        esac\n");
    out = std::format_to(out, "    fi\n\n");

    // Default: complete options
    out = std::format_to(out, "    # Default: complete options\n");
    out = std::format_to(out, "    if [[ \"${{cur}}\" == -* ]]; then\n");
    out = std::format_to(out, "        COMPREPLY=( $(compgen -W \"${{opts}}\" -- \"${{cur}}\") )\n");
    out = std::format_to(out, "        return 0\n");
    out = std::format_to(out, "    fi\n");

    out = std::format_to(out, "}}\n\n");
    out = std::format_to(out, "complete -F _{}_completions {}\n", func_name, program_name);

    return result;
}

auto ArgumentSpecs::generate_zsh(std::string_view program_name) const -> std::string
{
    std::string result;
    auto out = std::back_inserter(result);

    out = std::format_to(out, "#compdef {}\n", program_name);
    out = std::format_to(out, "# Zsh completion for {}\n", program_name);
    out = std::format_to(out, "# Place in a directory in your $fpath (e.g., ~/.zsh/completions/)\n\n");

    out = std::format_to(out, "_{}() {{\n", program_name);
    out = std::format_to(out, "    local -a opts\n");
    out = std::format_to(out, "    opts=(\n");

    for (auto const& spec : specs_) {
        // Escape single quotes in description
        std::string desc = spec.description;
        size_t pos = 0;
        while ((pos = desc.find('\'', pos)) != std::string::npos) {
            desc.replace(pos, 1, "'\\''");
            pos += 4;
        }

        if (spec.type == ArgType::Flag) {
            out = std::format_to(out, "        '--{}[{}]'\n", spec.name, desc);
        } else {
            std::string value_desc = get_value_description(spec);
            out = std::format_to(out, "        '--{}=[{}]:{}:{}'\n", spec.name, desc, spec.name, value_desc);
        }
    }

    out = std::format_to(out, "    )\n\n");
    out = std::format_to(out, "    _arguments -s $opts\n");
    out = std::format_to(out, "}}\n\n");
    out = std::format_to(out, "_{} \"$@\"\n", program_name);

    return result;
}

auto ArgumentSpecs::generate_fish(std::string_view program_name) const -> std::string
{
    std::string result;
    auto out = std::back_inserter(result);

    out = std::format_to(out, "# Fish completion for {}\n", program_name);
    out = std::format_to(out, "# Place in ~/.config/fish/completions/{}.fish\n\n", program_name);

    for (auto const& spec : specs_) {
        out = std::format_to(out, "complete -c {} -l {} -d '{}'", program_name, spec.name, spec.description);

        switch (spec.type) {
            case ArgType::Flag:
                // No argument required
                break;

            case ArgType::Choice:
                out = std::format_to(out, " -x -a '");
                for (size_t i = 0; i < spec.choices.size(); ++i) {
                    if (i > 0) {
                        out = std::format_to(out, " ");
                    }
                    out = std::format_to(out, "{}", spec.choices[i]);
                }
                out = std::format_to(out, "'");
                break;

            case ArgType::File:
                out = std::format_to(out, " -r -F");
                break;

            case ArgType::Directory:
                out = std::format_to(out, " -r -a '(__fish_complete_directories)'");
                break;

            case ArgType::Device:
            default:
                out = std::format_to(out, " -r");
                break;
        }

        out = std::format_to(out, "\n");
    }

    return result;
}

auto ArgumentSpecs::get_value_description(ArgumentSpec const& spec) -> std::string
{
    switch (spec.type) {
        case ArgType::Choice:
            if (!spec.choices.empty()) {
                std::string choices_str = "(";
                for (size_t i = 0; i < spec.choices.size(); ++i) {
                    if (i > 0) {
                        choices_str += " ";
                    }
                    choices_str += spec.choices[i];
                }
                choices_str += ")";
                return choices_str;
            }
            return " ";

        case ArgType::File:
            return "_files";

        case ArgType::Directory:
            return "_directories";

        case ArgType::Device:
            // Network interfaces
            return "_net_interfaces";

        default:
            return " ";
    }
}

void ArgumentSpecs::collect_unknown_keys(
    toml::Table const& table, std::string const& prefix, std::vector<std::string>& unknown) const
{
    for (auto const& [key, value] : table) {
        std::string const full_key = prefix.empty() ? key : prefix + "." + key;

        if (value.is_table()) {
            // Recurse into nested tables
            collect_unknown_keys(*value.as_table(), full_key, unknown);
        } else {
            // Check if this leaf key is known
            if (!is_known(full_key)) {
                unknown.push_back(full_key);
            }
        }
    }
}

}  // namespace statusbar::args
