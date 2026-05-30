// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// TerminalTui unit tests

#include "statusbar/tui/tui.hpp"

#include "statusbar/test/test.hpp"

#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <string>

using namespace statusbar::tui;

namespace {

struct PipePair
{
    int read_fd{-1};
    int write_fd{-1};

    PipePair()
    {
        int fds[2]{};
        if (::pipe(fds) == 0) {
            read_fd = fds[0];
            write_fd = fds[1];
        }
    }

    ~PipePair()
    {
        if (read_fd >= 0) {
            ::close(read_fd);
        }
        if (write_fd >= 0) {
            ::close(write_fd);
        }
    }

    auto drain() -> std::string
    {
        std::string result;
        char buf[1024]{};
        while (true) {
            auto const n = ::read(read_fd, buf, sizeof(buf));
            if (n <= 0) {
                break;
            }
            result.append(buf, static_cast<size_t>(n));
        }
        return result;
    }
};

}  // namespace

TEST(tui_keys, constants_are_distinct)
{
    EXPECT_NE(KEY_NONE, KEY_UP);
    EXPECT_NE(KEY_UP, KEY_DOWN);
    EXPECT_NE(KEY_DOWN, KEY_LEFT);
    EXPECT_NE(KEY_LEFT, KEY_RIGHT);
    EXPECT_NE(KEY_RIGHT, KEY_ENTER);
    EXPECT_NE(KEY_ENTER, KEY_ESCAPE);
}

TEST(tui_keys, arrow_keys_above_ascii)
{
    EXPECT_TRUE(KEY_UP > 127);
    EXPECT_TRUE(KEY_DOWN > 127);
    EXPECT_TRUE(KEY_LEFT > 127);
    EXPECT_TRUE(KEY_RIGHT > 127);
}

TEST(tui_output, write_produces_output)
{
    PipePair pipe;
    EXPECT_TRUE(pipe.write_fd >= 0);
    {
        TerminalTui tui{pipe.write_fd};
        tui.write("hello");
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("hello") != std::string::npos);
}

TEST(tui_output, clear_produces_escape_sequence)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.clear();
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("\033[2J") != std::string::npos);
}

TEST(tui_output, move_cursor_produces_escape_sequence)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.move_cursor(5, 10);
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("\033[5;10H") != std::string::npos);
}

TEST(tui_output, write_at_combines_move_and_write)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.write_at(3, 1, "test line");
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("\033[3;1H") != std::string::npos);
    EXPECT_TRUE(output.find("test line") != std::string::npos);
}

TEST(tui_output, set_bold_produces_escape)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.set_bold(true);
        tui.write("bold");
        tui.set_bold(false);
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("\033[1m") != std::string::npos);
    EXPECT_TRUE(output.find("bold") != std::string::npos);
}

TEST(tui_output, set_reverse_produces_escape)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.set_reverse(true);
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("\033[7m") != std::string::npos);
}

TEST(tui_output, reset_style_produces_escape)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.reset_style();
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("\033[0m") != std::string::npos);
}

TEST(tui_output, hide_cursor_produces_escape)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.hide_cursor();
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("\033[?25l") != std::string::npos);
}

TEST(tui_output, show_cursor_produces_escape)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.show_cursor();
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("\033[?25h") != std::string::npos);
}

TEST(tui_output, write_rule_produces_dashes)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.write_rule(5);
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    // Should contain move-to-row-5 escape and a sequence of dashes
    EXPECT_TRUE(output.find("\033[5;1H") != std::string::npos);
    // The rule writes cols() worth of dashes; for a pipe fd, cols() returns 80
    EXPECT_TRUE(output.find("----") != std::string::npos);
}

TEST(tui_output, flush_does_not_crash)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.write("test");
        tui.flush();
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("test") != std::string::npos);
}

TEST(tui_output, valid_returns_true_for_pipe)
{
    PipePair pipe;
    TerminalTui tui{pipe.write_fd};
    EXPECT_TRUE(tui.valid());
}

TEST(tui_output, rows_default_for_pipe)
{
    PipePair pipe;
    TerminalTui tui{pipe.write_fd};
    // A pipe fd cannot ioctl TIOCGWINSZ, so rows() returns default 24
    EXPECT_EQ(tui.rows(), 24);
}

TEST(tui_output, cols_default_for_pipe)
{
    PipePair pipe;
    TerminalTui tui{pipe.write_fd};
    // A pipe fd cannot ioctl TIOCGWINSZ, so cols() returns default 80
    EXPECT_EQ(tui.cols(), 80);
}

TEST(tui_output, set_bold_off_produces_escape)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.set_bold(false);
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("\033[22m") != std::string::npos);
}

TEST(tui_output, set_reverse_off_produces_escape)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.set_reverse(false);
    }
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    auto output = pipe.drain();
    EXPECT_TRUE(output.find("\033[27m") != std::string::npos);
}

TEST(tui_sigwinch, resize_pending_initially_false)
{
    PipePair pipe;
    TerminalTui tui{pipe.write_fd};
    // Drain any leftover flag from a previous test before observing.
    (void)tui.resize_pending();
    EXPECT_FALSE(tui.resize_pending());
}

TEST(tui_sigwinch, resize_pending_detects_flag)
{
    // Simulate SIGWINCH by invoking the handler directly — same code path
    // the kernel takes, no need to actually raise a signal.
    TerminalTui::sigwinch_handler(0);
    PipePair pipe;
    TerminalTui tui{pipe.write_fd};
    EXPECT_TRUE(tui.resize_pending());
    // Second call should return false (flag was cleared)
    EXPECT_FALSE(tui.resize_pending());
}

TEST(tui_output, write_empty_string_no_crash)
{
    PipePair pipe;
    {
        TerminalTui tui{pipe.write_fd};
        tui.write("");
        tui.write(std::string_view{});
    }
    // No crash is the test
    ::close(pipe.write_fd);
    pipe.write_fd = -1;
    // Should drain cleanly
    auto output = pipe.drain();
    (void)output;
}

TEST_MAIN(statusbar_tui, tui_test)
