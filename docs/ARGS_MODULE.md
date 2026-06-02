[← back to module index](README.md)

# args

Declarative CLI argument specification with type-safe value binding and
automatic bash/zsh/fish completion-script generation.

## Overview

The `args` module lets you describe a program's command-line options once
and reuse that description for help text, default-value population,
config-file validation, runtime value binding, and shell-completion
scripts.

Argument values arrive as a `toml::Table` (typically the result of
merging a TOML file with CLI overrides parsed elsewhere). Calling
`ArgumentSpecs::apply()` walks the registered specs, pulls each value
out of the table with the right type coercion, and hands it to a
user-supplied setter lambda. Validation errors are reported by throwing
a `statusbar::Status` from the lambda; `apply()` catches and returns it.

C++ types are mapped to `ArgType` (the completion-hint enum) via the
`ArgTypeTraits` template, so adding an `int` argument is `specs.add<int>(...)`
and the module knows to format help as `--port=N`. User-defined types
opt in by satisfying the `ConfigParseable` concept — define
`config_parse` and `config_format` in the type's own namespace and
`specs.add<MyType>(...)` works.

The module is intentionally narrow: it owns argument *description* and
*binding*, but not argument *parsing* from `argv[]`. That conversion
(CLI string to `toml::Table` entries) is performed by the caller before
`apply()` runs.

## Key types

- `ArgType` — enum of completion-hint categories: `String`, `Integer`, `Float`, `Flag`, `Choice`, `File`, `Directory`, `Device`.
- `ArgumentSpec` — value struct describing one option: name, type, description, default, choices, list flag, and an optional binding.
- `ArgumentSpecs` — the module's main entry point. Build with `add()` / `add_flag()` / `add_choice()` / `add_file()` / `add_device()` / `add_list()`; apply config with `apply()`; emit help with `format_help_to()`; emit completion scripts with `generate_bash()` / `generate_zsh()` / `generate_fish()`; compose via `merge()`.
- `ArgTypeTraits<T>` — trait that maps a C++ type to its `ArgType` and knows how to extract a `T` from a `toml::Table`.
- `ConfigParseable` — concept user types satisfy (via ADL `config_parse` / `config_format` functions) to plug into `ArgTypeTraits`. `config_parse` returns `statusbar::StatusValue<T>` so a failed parse can surface its own error code; the trait still falls back to the supplied default on failure.
- `is_string_shaped(ArgType)` — predicate used by config layers that must keep certain CLI values verbatim instead of auto-typing them.

## Quick example

```cpp
#include "statusbar/args/args.hpp"
#include "statusbar/toml/toml_value.hpp"

using namespace statusbar;

int main()
{
    bool verbose = false;
    int port = 0;
    args::ArgumentSpecs specs;
    specs.add_flag("verbose", "Enable verbose output", [&](bool v) { verbose = v; });
    specs.add<int>("port", "Listen port", 8080, [&](int v) { port = v; });
    // Typically merged from a config file + CLI.
    toml::Table root;
    root.set("verbose", toml::Value{true});
    root.set("port", toml::Value{int64_t{9090}});
    // apply() returns a Status (it catches any thrown one).
    auto status = specs.apply(root);
    if (!status) {
        // status carries the error_code; surface it as you see fit.
        return 1;
    }
    return 0;
}
```

## Headers

- `statusbar/args/args.hpp` — module header (pulls in `args_spec.hpp` transitively). This is what consumers should `#include`.
- `statusbar/args/args_spec.hpp` — definitions of `ArgType`, `ArgumentSpec`, `ArgumentSpecs`, and `ArgTypeTraits`.

## Dependencies

- **Statusbar modules:** [`status`](STATUS_MODULE.md) (binding lambdas throw `Status`; `apply()` returns one), [`toml`](TOML_MODULE.md) (values flow through `toml::Table` / `toml::Value` / `toml::get_list`).
- **System / external:**
  - `sg14::inplace_function` from `statusbar/sg14/inplace_function.h` — used to store binding lambdas.
  - `<format>`, `<print>`, `<concepts>` from the C++20/23 standard library.

> **Convention:** `statusbar/sg14/` holds vendored third-party headers with their own [README](../statusbar/sg14/README.md); it is *not* a first-party statusbar module. That is why sg14 types appear under "System / external" rather than "Statusbar modules" — subsequent module docs (e.g. `CONTAINER_MODULE.md`, which uses `inplace_vector`) follow the same convention.

## Notes & caveats

- `ArgumentSpecs::apply()` returns `Status`; a binding lambda signals validation failure by throwing a `Status`, which `apply()` catches and returns.
- Don't return error codes from the lambda — throw. The setter signature is `void(T)`, so throwing is the only channel a failing setter has to surface an error to `apply()`.
- Stored bindings live in `sg14::inplace_function<void(toml::Table const&), 96>`. A capture list larger than ~96 bytes will fail to compile; capture by reference or pull state into a struct.
- `ArgType::Choice` validation is *not* enforced by `apply()` itself; the choice list is used for help text and completion. Enforce membership inside your setter if you need it.
- List arguments (`add_list<T>`) split CLI values on commas (`--name=a,b,c`) but also accept TOML arrays; empty elements from trailing or doubled commas are skipped.
- `merge(other, prefix)` rewrites the merged specs' display names but wraps their bindings so they continue looking up the original unprefixed key in config — design the inner specs around the unprefixed names.
- `populate_defaults()` will not overwrite values already present in the table; pass `include_all = true` to also emit entries for options whose default is empty.
- This module does *not* parse `argv[]` itself. Wire it up to your CLI parser of choice and feed the resulting key/value pairs into a `toml::Table` before calling `apply()`.

## Further reading

- [`toml`](TOML_MODULE.md) — the value/table type used to feed `apply()`.
- [`status`](STATUS_MODULE.md) — error-reporting type thrown from binding lambdas.
- [`statusbar/sg14/README.md`](../statusbar/sg14/README.md) — vendored `inplace_function` / `inplace_vector` headers.
