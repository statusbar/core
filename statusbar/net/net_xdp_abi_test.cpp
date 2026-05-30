// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// Compile-time proof that the shared BPF ABI struct (`tcam_rule` in
// net_tcam_bpf_abi.h) is byte-for-byte identical to the userspace
// `CompiledRule<32>`. If either side drifts, this translation unit
// refuses to compile.

#include "statusbar/net/net_compiled_rule.hpp"
#include "statusbar/net/net_tcam_bpf_abi.h"
#include "statusbar/net/net_tcam_classifier.hpp"
#include "statusbar/test/test.hpp"

#include <cstddef>
#include <type_traits>

namespace {

using statusbar::net::CompiledRule;
namespace flag = statusbar::net::flag;

static_assert(std::is_standard_layout_v<CompiledRule<32>>, "CompiledRule<32> must be standard-layout so offsetof is well-defined");
static_assert(
    std::is_trivially_copyable_v<CompiledRule<32>>,
    "CompiledRule<32> must be trivially copyable so the installer can memcpy it into the BPF map");

static_assert(sizeof(tcam_rule) == sizeof(CompiledRule<32>), "tcam_rule and CompiledRule<32> must have identical size");

static_assert(offsetof(tcam_rule, mask) == offsetof(CompiledRule<32>, mask), "mask offset mismatch");
static_assert(offsetof(tcam_rule, match) == offsetof(CompiledRule<32>, match), "match offset mismatch");
static_assert(offsetof(tcam_rule, result_flags) == offsetof(CompiledRule<32>, result_flags), "result_flags offset mismatch");
static_assert(offsetof(tcam_rule, min_frame_size) == offsetof(CompiledRule<32>, min_frame_size), "min_frame_size offset mismatch");
static_assert(offsetof(tcam_rule, priority) == offsetof(CompiledRule<32>, priority), "priority offset mismatch");

static_assert(TCAM_FLAG_PROCESS == flag::process, "TCAM_FLAG_PROCESS must match flag::process");
static_assert(TCAM_FLAG_FORWARD_TAP == flag::forward_tap, "TCAM_FLAG_FORWARD_TAP must match flag::forward_tap");
static_assert(TCAM_FLAG_DROP == flag::drop, "TCAM_FLAG_DROP must match flag::drop");

static_assert(TCAM_WINDOW_BYTES == statusbar::net::tcam_default_window_bytes, "ABI window size must match userspace default");
static_assert(TCAM_MAX_RULES == statusbar::net::tcam_default_capacity, "ABI max-rules must match userspace classifier capacity");

}  // namespace

TEST(xdp_abi, layout_equivalence)
{
    EXPECT_EQ(sizeof(tcam_rule), sizeof(CompiledRule<32>));
    EXPECT_EQ(offsetof(tcam_rule, mask), offsetof(CompiledRule<32>, mask));
    EXPECT_EQ(offsetof(tcam_rule, match), offsetof(CompiledRule<32>, match));
    EXPECT_EQ(offsetof(tcam_rule, result_flags), offsetof(CompiledRule<32>, result_flags));
    EXPECT_EQ(offsetof(tcam_rule, min_frame_size), offsetof(CompiledRule<32>, min_frame_size));
    EXPECT_EQ(offsetof(tcam_rule, priority), offsetof(CompiledRule<32>, priority));
}

TEST(xdp_abi, flag_bits_match)
{
    EXPECT_EQ(TCAM_FLAG_PROCESS, flag::process);
    EXPECT_EQ(TCAM_FLAG_FORWARD_TAP, flag::forward_tap);
    EXPECT_EQ(TCAM_FLAG_DROP, flag::drop);
}

TEST_MAIN(statusbar_net, net_xdp_abi_test)
