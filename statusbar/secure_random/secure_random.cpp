// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/secure_random/secure_random.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>

#if defined(__linux__)
#    include <sys/random.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#    include <cstdlib>  // arc4random_buf
#else
#    error "secure_random_bytes: no entropy source for this platform"
#endif

namespace statusbar {

namespace {

[[noreturn]] void abort_with_message(char const* msg) noexcept
{
    std::fputs("statusbar::secure_random_bytes: fatal: ", stderr);
    std::fputs(msg, stderr);
    std::fputc('\n', stderr);
    std::abort();
}

}  // namespace

void secure_random_bytes(std::span<uint8_t> out) noexcept
{
    if (out.empty()) {
        return;
    }
#if defined(__linux__)
    auto* p = out.data();
    size_t remaining = out.size();
    while (remaining > 0) {
        ssize_t const n = ::getrandom(p, remaining, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            abort_with_message("getrandom(2) failed");
        }
        p += static_cast<size_t>(n);
        remaining -= static_cast<size_t>(n);
    }
#else
    ::arc4random_buf(out.data(), out.size());
#endif
}

}  // namespace statusbar
