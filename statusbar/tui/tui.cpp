// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/tui/tui.hpp"

#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>

#include <sys/ioctl.h>

namespace statusbar::tui {

// Static member definition
std::atomic<bool> TerminalTui::sigwinch_received_{false};

auto TerminalTui::sigwinch_handler(int /*sig*/) noexcept -> void
{
    sigwinch_received_.store(true, std::memory_order_relaxed);
}

auto TerminalTui::install_sigwinch_handler() -> void
{
    std::signal(SIGWINCH, &TerminalTui::sigwinch_handler);
}

auto TerminalTui::resize_pending() -> bool
{
    return sigwinch_received_.exchange(false, std::memory_order_relaxed);
}

TerminalTui::TerminalTui(int output_fd)
    : output_fd_(output_fd < 0 ? STDOUT_FILENO : output_fd)
{
    if (output_fd_ < 0) {
        return;
    }

    // Only enable raw mode for interactive use — when output goes to stdout
    // AND stdin is a real tty. Pipe-based tests pass a write fd for output;
    // we must NOT modify the parent shell's terminal in that case (would
    // leak across parallel tests and corrupt the user's terminal on early
    // panic / racy drop ordering).
    if (output_fd_ != STDOUT_FILENO) {
        return;
    }
    if (::isatty(STDIN_FILENO) == 0) {
        return;
    }
    if (::tcgetattr(STDIN_FILENO, &saved_termios_) != 0) {
        return;
    }

    // Build raw-mode settings from the saved state
    ::termios raw = saved_termios_;
    raw.c_iflag &= ~static_cast<::tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~static_cast<::tcflag_t>(OPOST);
    raw.c_cflag |= static_cast<::tcflag_t>(CS8);
    raw.c_lflag &= ~static_cast<::tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;  // Non-blocking reads
    raw.c_cc[VTIME] = 0;

    if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
        raw_mode_set_ = true;
        install_sigwinch_handler();
    }
}

TerminalTui::~TerminalTui()
{
    if (raw_mode_set_) {
        show_cursor();
        reset_style();
        (void)::tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios_);
        raw_mode_set_ = false;
    }
}

auto TerminalTui::clear() -> void
{
    write_escape("\033[2J\033[H");
}

auto TerminalTui::move_cursor(int row, int col) -> void
{
    char buf[32];
    int const len = ::snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
    if (len > 0) {
        write_escape(std::string_view(buf, static_cast<std::string_view::size_type>(len)));
    }
}

auto TerminalTui::write(std::string_view text) -> void
{
    if (output_fd_ < 0 || text.empty()) {
        return;
    }
    ::write(output_fd_, text.data(), text.size());
}

auto TerminalTui::write_at(int row, int col, std::string_view text) -> void
{
    move_cursor(row, col);
    write(text);
}

auto TerminalTui::write_rule(int row) -> void
{
    int const width = cols();
    move_cursor(row, 1);
    std::string const dashes(static_cast<std::string::size_type>(width), '-');
    write(dashes);
}

auto TerminalTui::set_bold(bool on) -> void
{
    if (on) {
        write_escape("\033[1m");
    } else {
        write_escape("\033[22m");
    }
}

auto TerminalTui::set_reverse(bool on) -> void
{
    if (on) {
        write_escape("\033[7m");
    } else {
        write_escape("\033[27m");
    }
}

auto TerminalTui::reset_style() -> void
{
    write_escape("\033[0m");
}

auto TerminalTui::hide_cursor() -> void
{
    write_escape("\033[?25l");
}

auto TerminalTui::show_cursor() -> void
{
    write_escape("\033[?25h");
}

auto TerminalTui::flush() -> void
{
    if (output_fd_ >= 0) {
        ::fsync(output_fd_);
    }
}

auto TerminalTui::rows() const -> int
{
    struct ::winsize ws{};
    if (output_fd_ >= 0 && ::ioctl(output_fd_, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return static_cast<int>(ws.ws_row);
    }
    return 24;
}

auto TerminalTui::cols() const -> int
{
    struct ::winsize ws{};
    if (output_fd_ >= 0 && ::ioctl(output_fd_, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return static_cast<int>(ws.ws_col);
    }
    return 80;
}

auto TerminalTui::read_key() const -> int
{
    unsigned char ch = 0;
    if (::read(STDIN_FILENO, &ch, 1) != 1) {
        return KEY_NONE;
    }

    // Carriage return → Enter
    if (ch == '\r') {
        return KEY_ENTER;
    }

    // Potential escape sequence for arrow keys. Use poll() with a
    // short timeout to distinguish a standalone Escape press from
    // the start of a multi-byte sequence (e.g. ESC [ A for arrow up).
    if (ch == KEY_ESCAPE) {
        pollfd pfd{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
        if (::poll(&pfd, 1, 50) <= 0) {
            return KEY_ESCAPE;  // no more bytes within 50ms — standalone Escape
        }
        unsigned char seq0 = 0;
        if (::read(STDIN_FILENO, &seq0, 1) != 1 || seq0 != '[') {
            return KEY_ESCAPE;
        }
        if (::poll(&pfd, 1, 50) <= 0) {
            return KEY_ESCAPE;
        }
        unsigned char seq1 = 0;
        if (::read(STDIN_FILENO, &seq1, 1) != 1) {
            return KEY_ESCAPE;
        }

        switch (seq1) {
            case 'A':
                return KEY_UP;
            case 'B':
                return KEY_DOWN;
            case 'C':
                return KEY_RIGHT;
            case 'D':
                return KEY_LEFT;
            default:
                return KEY_ESCAPE;
        }
    }

    return static_cast<int>(ch);
}

auto TerminalTui::write_escape(std::string_view seq) -> void
{
    if (output_fd_ < 0 || seq.empty()) {
        return;
    }
    ::write(output_fd_, seq.data(), seq.size());
}

}  // namespace statusbar::tui
