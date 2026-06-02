[← back to module index](README.md)

# config

Configuration management that cascades TOML files with CLI overrides
and wires the result into typed argument bindings.

## Overview

The `config` module is the runtime counterpart to [`args`](ARGS_MODULE.md).
Where `args` *describes* options, `config` *gathers their values* from
disk and CLI and stores them in a single `toml::Table` keyed by dotted
paths.

A `Config` owns a `toml::Table` plus the list of files it has loaded.
Files merge in declaration order (later overrides earlier); CLI args
(`--key=value` / `--key value`, dotted keys like `--server.port=8080`)
override files. `--config=FILE` loads additional TOML files mid-parse.

`parse_cli_args()` is the recommended driver. It cascades the default
user config (under `$XDG_CONFIG_HOME` or `~/.config/$program_name/`),
then any `--config-load` files, then CLI argv, and finally calls
`ArgumentSpecs::apply()` to push each value into the user's binding
lambdas. Built-in commands (`--help`, `--completion`, `--config-save`)
signal "stop here" via a sentinel `Status` (`ConfigError::builtin_handled`);
use `handled_builtin_command()` to choose between exit code 0 and 1.

Prefer the `apply_cli_overrides(argc, argv, specs)` overload when specs
are available: it consults `ArgType` so string-shaped values (paths,
devices, choices) are stored verbatim instead of being coerced by TOML
auto-typing.

## Key types

- `Config` — the store. Load with `load_file()` / `load_file_if_exists()` / `load_string()`; override with `apply_cli_overrides()`; read with `get_string()`, `get_integer()`, `get_float()`, `get_boolean()`, `get_array()`, `get_table()`, `get_list<T>()`; write with `write_file()` / `to_toml_string()`. `ConfigParseable` user types plug in via `get<T>()` / `set<T>()`.
- `HelpCallback` — `void (*)(char const*, args::ArgumentSpecs const&)`, invoked when `--help` is seen. Defaults to `default_print_usage`.
- `parse_cli_args()` — one-shot driver: cascades default + `--config-load` files, parses argv, dispatches built-ins, then calls `ArgumentSpecs::apply()`.
- `parse_config_or_exit()` — template wrapper around any `(argc, argv) -> StatusValue<Config>` parser; `std::exit(0)` on `ConfigError::builtin_handled`, else `std::exit(1)`.
- `handled_builtin_command(Status)` — predicate: `true` iff the status came from a benign built-in (`ConfigError::builtin_handled`).
- `default_config_path(program_name)` — resolves `$XDG_CONFIG_HOME/$program_name/config.toml` or `$HOME/.config/$program_name/config.toml`; empty if neither env var is set.
- `handle_completion()` — emits a bash/zsh/fish completion script to stdout if `--completion=SHELL` is present; returns `true` when it did.

## Quick example

```cpp
#include "statusbar/config/config.hpp"
#include "statusbar/args/args_spec.hpp"

using namespace statusbar;

int main(int argc, char* argv[])
{
    int64_t port = 0;
    bool verbose = false;
    args::ArgumentSpecs specs;
    specs.add<int64_t>("port", "Listen port", 8080, [&](int64_t v) { port = v; });
    specs.add_flag("verbose", "Enable verbose output", [&](bool v) { verbose = v; });

    // Cascades $HOME/.config/myapp/config.toml, then --config-load files,
    // then CLI args; finally calls specs.apply().
    auto status = config::parse_cli_args(argc, argv, specs, config::default_print_usage, "myapp");
    if (!status) {
        return config::handled_builtin_command(status) ? 0 : 1;
    }
    (void)port; (void)verbose;
    return 0;
}
```

## Headers

- `statusbar/config/config.hpp` — module header. This is what consumers should `#include`.
- `statusbar/config/config_store.hpp` — `Config` class: TOML cascade, CLI overrides, typed accessors, serialisation.
- `statusbar/config/config_cli.hpp` — `parse_cli_args()`, `parse_config_or_exit()`, `HelpCallback`, `default_print_usage()`, `handle_completion()`, `handled_builtin_command()`, `default_config_path()`.

## Dependencies

- **Statusbar modules:** [`args`](ARGS_MODULE.md) (`ArgumentSpecs`, `ConfigParseable`, `ArgType`), [`status`](STATUS_MODULE.md) (`Status` / `StatusValue` / `failure` / `success` are re-exported into `statusbar::config`), [`toml`](TOML_MODULE.md) (`Table`, `Value`, `Array`, `get_list`, `parse_value_string` from `toml_value.hpp`, `toml_parser.hpp`, `toml_error.hpp`).
- **System / external:** `<optional>`, `<span>`, `<string>`, `<string_view>`, `<vector>`, `<type_traits>`, `<cstdlib>`.

## Notes & caveats

- `apply_cli_overrides()` returns the remaining (non-override) arguments including `argv[0]` — that's where positional arguments surface.
- The spec-less `apply_cli_overrides(argc, argv)` auto-types via TOML rules; strings of pure octal-looking digits get coerced to integers. Pass `ArgumentSpecs` to the three-arg overload to keep string-shaped values verbatim.
- `load_file()` treats a missing file as an error; `load_file_if_exists()` does not. `parse_cli_args()` uses the latter for the default user config — missing is silent.
- A falsy `Status` from `parse_cli_args()` isn't always a failure: built-in commands set `ConfigError::builtin_handled`. Always gate exit-code selection on `handled_builtin_command()`.
- `--config=FILE` (low-level) and `--config-load=FILE` (driver option) are distinct; both may appear multiple times. `--no-default-config` disables `parse_cli_args()`'s user-config lookup.
- `set_from_string()` auto-detects type via TOML rules; `set_string()` always stores a string. Use the latter when the destination is textual.
- `write_file(path, specs)` and `to_toml_string(specs)` annotate each emitted value with the matching spec's description as `#` comment lines.
- Control characters in string values are escaped as `\uXXXX` on serialisation.

## Further reading

- [`args`](ARGS_MODULE.md) — option-description layer; `Config` is the value layer underneath.
- [`toml`](TOML_MODULE.md) — underlying table / value / parser types.
- [`status`](STATUS_MODULE.md) — error type returned by `parse_cli_args()`.
