#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// Terminal TUI Module
// Provides raw terminal mode, ANSI escape sequences, and keyboard input

#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <string_view>

namespace statusbar::tui {

/// Key code constants returned by read_key()
constexpr int KEY_NONE = 0;      ///< No key available (non-blocking read returned nothing)
constexpr int KEY_UP = 256;      ///< Up arrow key
constexpr int KEY_DOWN = 257;    ///< Down arrow key
constexpr int KEY_LEFT = 258;    ///< Left arrow key
constexpr int KEY_RIGHT = 259;   ///< Right arrow key
constexpr int KEY_ENTER = '\n';  ///< Enter key (also matches \r which is remapped)
constexpr int KEY_ESCAPE = 27;   ///< Escape key
constexpr int KEY_CTRL_L = 12;   ///< Ctrl-L (form feed, conventional clear/refresh)

/// RAII terminal TUI class providing raw mode, ANSI rendering, and key input.
///
/// On construction, the terminal is placed into raw mode (no echo, no line
/// buffering, no signal processing). On destruction the original terminal
/// settings are restored and the cursor is made visible again.
///
/// Non-copyable and non-movable — there should be at most one active instance.
class TerminalTui
{
  public:
    /// Construct and initialise the terminal.
    /// \param output_fd  File descriptor to write to. Defaults to STDOUT_FILENO
    ///                   when -1 is passed.
    explicit TerminalTui(int output_fd = -1);

    /// Restore the terminal to its original state.
    ~TerminalTui();

    TerminalTui(TerminalTui const&) = delete;
    auto operator=(TerminalTui const&) -> TerminalTui& = delete;
    TerminalTui(TerminalTui&&) = delete;
    auto operator=(TerminalTui&&) -> TerminalTui& = delete;

    /// Returns true if the output file descriptor is valid.
    [[nodiscard]] auto valid() const -> bool { return output_fd_ >= 0; }

    /// Clear the entire screen and move the cursor to the home position.
    auto clear() -> void;

    /// Move the cursor to the specified row and column (1-based).
    /// \param row  Row number (1 = top).
    /// \param col  Column number (1 = left).
    auto move_cursor(int row, int col) -> void;

    /// Write a string to the output file descriptor.
    /// \param text  Text to write.
    auto write(std::string_view text) -> void;

    /// Move the cursor then write a string.
    /// \param row   Row number (1-based).
    /// \param col   Column number (1-based).
    /// \param text  Text to write.
    auto write_at(int row, int col, std::string_view text) -> void;

    /// Write a horizontal rule (dashes) spanning the full terminal width at
    /// the given row.
    /// \param row  Row number (1-based).
    auto write_rule(int row) -> void;

    /// Enable or disable bold text rendering.
    /// \param on  true to enable bold, false to return to normal weight.
    auto set_bold(bool on) -> void;

    /// Enable or disable reverse-video (inverted colours) rendering.
    /// \param on  true to enable reverse, false to disable.
    auto set_reverse(bool on) -> void;

    /// Reset all text attributes to their default values.
    auto reset_style() -> void;

    /// Hide the terminal cursor.
    auto hide_cursor() -> void;

    /// Show the terminal cursor.
    auto show_cursor() -> void;

    /// Flush the output file descriptor (calls fsync).
    auto flush() -> void;

    /// Return the current terminal height in rows.
    /// Falls back to 24 if the size cannot be determined.
    [[nodiscard]] auto rows() const -> int;

    /// Return the current terminal width in columns.
    /// Falls back to 80 if the size cannot be determined.
    [[nodiscard]] auto cols() const -> int;

    /// Read a single key press in a non-blocking fashion.
    /// Arrow keys are decoded from their ANSI escape sequences and returned
    /// as KEY_UP, KEY_DOWN, KEY_LEFT, or KEY_RIGHT.  A carriage-return is
    /// reported as KEY_ENTER.  Returns KEY_NONE when no input is available.
    [[nodiscard]] auto read_key() const -> int;

    /// Check if a terminal resize (SIGWINCH) has occurred since the last call.
    /// Clears the flag after reading, so subsequent calls return false until
    /// the next resize event.
    [[nodiscard]] auto resize_pending() -> bool;

    /// Install a SIGWINCH handler that sets a flag when the terminal is resized.
    /// Called automatically by the constructor. Safe to call multiple times.
    static auto install_sigwinch_handler() -> void;

    /// SIGWINCH handler. Sets the internal resize flag; consulted via
    /// resize_pending(). Signal-safe (only touches an atomic). Public so tests
    /// can drive the resize-handling path without raising a real signal.
    static auto sigwinch_handler(int sig) noexcept -> void;

  private:
    /// Write a raw escape sequence to the output file descriptor.
    /// \param seq  Escape sequence string (including the leading ESC character).
    auto write_escape(std::string_view seq) -> void;

    /// Flag set by sigwinch_handler. Private so consumers go through
    /// resize_pending() / sigwinch_handler() rather than poking the atomic.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::atomic<bool> sigwinch_received_;

    int output_fd_;              ///< Output file descriptor
    bool raw_mode_set_{false};   ///< True if raw mode was successfully applied
    ::termios saved_termios_{};  ///< Original terminal settings to restore on destruction
};

}  // namespace statusbar::tui
