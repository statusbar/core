[← back to module index](README.md)

# tui

Minimal terminal-UI helper: raw-mode setup, ANSI cursor / style escape
sequences, non-blocking key reads with arrow-key decoding, and a
SIGWINCH-driven resize flag — all wrapped in a single RAII class.

## Overview

The `tui` module is one header and one cpp. Its job is to give a
console program enough control over the terminal to draw a status
screen, a menu, or a busy interactive view without pulling in `ncurses`
or `termbox`. There is no widget tree, no double-buffering, no colour
palette abstraction — just a thin layer above raw `termios`, `write(2)`
and `ioctl(TIOCGWINSZ)`.

Constructing a `TerminalTui` puts the terminal into raw mode (no echo,
no canonical line buffering, no ISIG, non-blocking single-byte reads)
and installs a `SIGWINCH` handler. Destructing it restores the saved
`termios`, shows the cursor, and resets style. Because the destructor
must run for the terminal to be usable again, the class is non-copyable
*and* non-movable: there should be at most one live instance.

Raw mode is only entered when the output descriptor is `STDOUT_FILENO`
*and* `STDIN_FILENO` is a tty. Tests pass a pipe write-end as the output
descriptor, which keeps the parent shell's terminal untouched and lets
multiple tests run in parallel safely.

Output methods (`clear`, `move_cursor`, `write_at`, `write_rule`,
`set_bold`, `set_reverse`, `reset_style`, `hide_cursor`, `show_cursor`)
emit standard CSI sequences directly with `write(2)`. `flush()` calls
`fsync` on the output fd. `rows()` / `cols()` query `TIOCGWINSZ` and
fall back to 24 / 80 when the descriptor is not a tty.

`read_key()` performs a single non-blocking `read` on `STDIN_FILENO`,
returns `KEY_NONE` when nothing is available, remaps `\r` to
`KEY_ENTER`, and uses `poll()` with a 50 ms timeout to disambiguate a
bare Escape press from the start of a CSI arrow-key sequence
(`ESC [ A/B/C/D`).

## Key types

- `TerminalTui` — RAII terminal handle. Non-copyable, non-movable.
  Methods: `valid`, `clear`, `move_cursor`, `write`, `write_at`,
  `write_rule`, `set_bold`, `set_reverse`, `reset_style`, `hide_cursor`,
  `show_cursor`, `flush`, `rows`, `cols`, `read_key`, `resize_pending`,
  plus the static `install_sigwinch_handler` and `sigwinch_handler`
  (invoke directly to simulate SIGWINCH in tests).
- `KEY_NONE`, `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`,
  `KEY_ENTER`, `KEY_ESCAPE`, `KEY_CTRL_L` — `constexpr int` key codes
  returned by `read_key()`. Arrow keys use values above 255 so they
  never collide with ASCII bytes.

## Quick example

Adapted from `core/statusbar/tui/tui_test.cpp`. The test suite uses a
pipe write-end as the output descriptor; production code typically lets
`TerminalTui` default to `STDOUT_FILENO`.

```cpp
#include "statusbar/tui/tui.hpp"

using namespace statusbar::tui;

int main()
{
    TerminalTui tui;  // STDOUT_FILENO; raw mode if stdin is a tty
    tui.clear();
    tui.hide_cursor();
    tui.write_at(1, 1, "Press q to quit, arrows to move");
    tui.write_rule(2);

    int row = 4, col = 1;
    while (true) {
        if (tui.resize_pending()) tui.clear();
        int const k = tui.read_key();
        if (k == 'q' || k == KEY_ESCAPE) break;
        if (k == KEY_UP)    --row;
        if (k == KEY_DOWN)  ++row;
        if (k == KEY_LEFT)  --col;
        if (k == KEY_RIGHT) ++col;
        tui.write_at(row, col, "*");
    }
    // Destructor restores terminal state.
}
```

## Headers

- `statusbar/tui/tui.hpp` — sole public header. Declares `TerminalTui`
  and the `KEY_*` constants.

## Dependencies

- **Statusbar modules:** none. `tui` sits near the leaves of the
  dependency graph; only its `*_test.cpp` pulls in
  [`test`](TEST_MODULE.md).
- **System / external:** `<termios.h>`, `<unistd.h>`, `<sys/ioctl.h>`,
  `<poll.h>`, `<csignal>`, `<cstdio>`, `<cstring>`, `<cerrno>`,
  `<atomic>`, `<string>`, `<string_view>`. POSIX-only — the module uses
  `termios`, `TIOCGWINSZ`, `SIGWINCH`, and `poll`.

## Notes & caveats

- **POSIX / TTY only.** Raw mode requires `STDIN_FILENO` to satisfy
  `isatty()` and the output fd to equal `STDOUT_FILENO`. If either
  check fails the constructor returns a usable object that still emits
  escapes through `write()` but does not touch terminal settings — this
  is what makes the pipe-backed unit tests safe.
- **ANSI / CSI escapes are assumed.** `clear`, `move_cursor`,
  `set_bold`, `set_reverse`, `hide_cursor`, etc. emit raw CSI sequences
  (`\033[2J`, `\033[%d;%dH`, `\033[1m`, `\033[7m`, `\033[?25l`, …) with
  no terminfo lookup. Terminals without ANSI support will display
  garbage; legacy Windows consoles need VT processing enabled.
- **Non-copyable and non-movable by design.** The destructor restores
  `termios`; duplicating the handle would double-restore and corrupt
  the user's terminal. Hold a `TerminalTui` by reference or in a single
  owning scope.
- **Resize handling is cooperative.** A SIGWINCH handler sets an
  internal atomic flag; call `resize_pending()` from your event loop
  to consume it. The handler is installed process-wide and stays
  installed after the `TerminalTui` is destroyed — it remains safe
  because it only writes to the atomic.
- **`read_key()` reads from `STDIN_FILENO`, not the output fd.** It
  only works when stdin is the same terminal `TerminalTui` was built
  for. Outside raw mode it still works but will block by line rather
  than per-byte.
- **`flush()` calls `fsync(2)`** on the output fd. The module does no
  userspace buffering — every `write_*` call is a direct `write(2)`.
- **Single instance only.** Two simultaneous `TerminalTui` objects on
  the same process would race over `termios` and the second destructor
  would restore stale settings.

## Further reading

- [`test`](TEST_MODULE.md) — used by `tui_test.cpp` to exercise the
  escape-sequence output through a pipe pair.
