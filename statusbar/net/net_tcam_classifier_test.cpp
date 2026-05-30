// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Tests for the TcamClassifier evaluator.

#include "statusbar/net/net_tcam_classifier.hpp"

#include "statusbar/ieee/ieee.hpp"
#include "statusbar/net/net_compiled_rule.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <cstdint>
#include <cstring>

using namespace statusbar;
using namespace statusbar::net;

namespace {

/// Build a plausible untagged AVTP/AAF frame in a 64-byte buffer.
auto make_untagged_avtp_frame(
    ieee::Eui48 const& dest,
    ieee::Eui48 const& src,
    std::uint16_t ethertype,
    std::uint8_t subtype,
    std::array<std::uint8_t, 8> const& stream_id) -> std::array<std::uint8_t, 64>
{
    std::array<std::uint8_t, 64> f{};
    std::memcpy(f.data() + 0, dest.value.data(), 6);
    std::memcpy(f.data() + 6, src.value.data(), 6);
    f[12] = static_cast<std::uint8_t>(ethertype >> 8);
    f[13] = static_cast<std::uint8_t>(ethertype & 0xFFU);
    f[14] = subtype;  // AVTP common header starts here
    // bytes 15..17 are other common-header fields we don't model in the test
    std::memcpy(f.data() + 18, stream_id.data(), 8);
    return f;
}

auto make_tagged_avtp_frame(
    ieee::Eui48 const& dest,
    ieee::Eui48 const& src,
    std::uint16_t vid,
    std::uint16_t ethertype,
    std::uint8_t subtype,
    std::array<std::uint8_t, 8> const& stream_id) -> std::array<std::uint8_t, 64>
{
    std::array<std::uint8_t, 64> f{};
    std::memcpy(f.data() + 0, dest.value.data(), 6);
    std::memcpy(f.data() + 6, src.value.data(), 6);
    f[12] = 0x81;
    f[13] = 0x00;  // TPID
    f[14] = static_cast<std::uint8_t>((vid >> 8) & 0x0FU);
    f[15] = static_cast<std::uint8_t>(vid & 0xFFU);
    f[16] = static_cast<std::uint8_t>(ethertype >> 8);
    f[17] = static_cast<std::uint8_t>(ethertype & 0xFFU);
    f[18] = subtype;
    std::memcpy(f.data() + 22, stream_id.data(), 8);
    return f;
}

}  // namespace

TEST(tcam_classifier, empty_rules_returns_default)
{
    TcamClassifier<> tc;
    tc.set_rules({}, flag::forward_tap);
    std::array<std::uint8_t, 64> f{};
    EXPECT_EQ(tc.classify_first(f), flag::forward_tap);
}

TEST(tcam_classifier, short_frame_returns_default)
{
    auto rules = ClassifierRuleBuilder<>{}.ethertype(0x22F0).result(flag::process).build();
    TcamClassifier<> tc;
    tc.set_rules(rules, flag::forward_tap);
    std::array<std::uint8_t, 14> short_frame{};  // below TCAM_WINDOW_BYTES
    EXPECT_EQ(tc.classify_first(short_frame), flag::forward_tap);
}

TEST(tcam_classifier, ethertype_only_matches_any_dest_and_src)
{
    auto rules = ClassifierRuleBuilder<>{}.ethertype(0x22F0).result(flag::process).build();
    TcamClassifier<> tc;
    tc.set_rules(rules, flag::forward_tap);
    auto f = make_untagged_avtp_frame(
        {0x91, 0xE0, 0xF0, 0x00, 0x00, 0x42},
        {0x02, 0x00, 0x00, 0x00, 0x00, 0x01},
        0x22F0,
        0x02,
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77});
    EXPECT_EQ(tc.classify_first(f), flag::process);
}

TEST(tcam_classifier, tagged_rule_rejects_untagged_frame)
{
    auto rules = ClassifierRuleBuilder<>{}.vlan_tagged_any().ethertype(0x22F0).result(flag::process).build();
    TcamClassifier<> tc;
    tc.set_rules(rules, flag::forward_tap);
    auto f = make_untagged_avtp_frame(
        {0x91, 0xE0, 0xF0, 0x00, 0x00, 0x42},
        {0x02, 0x00, 0x00, 0x00, 0x00, 0x01},
        0x22F0,
        0x02,
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77});
    EXPECT_EQ(tc.classify_first(f), flag::forward_tap);
}

TEST(tcam_classifier, untagged_rule_rejects_tagged_frame)
{
    auto rules = ClassifierRuleBuilder<>{}.vlan_untagged().ethertype(0x22F0).result(flag::process).build();
    TcamClassifier<> tc;
    tc.set_rules(rules, flag::forward_tap);
    auto f = make_tagged_avtp_frame(
        {0x91, 0xE0, 0xF0, 0x00, 0x00, 0x42},
        {0x02, 0x00, 0x00, 0x00, 0x00, 0x01},
        2,
        0x22F0,
        0x02,
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77});
    EXPECT_EQ(tc.classify_first(f), flag::forward_tap);
}

TEST(tcam_classifier, vlan_any_catches_both_framings)
{
    auto rules = ClassifierRuleBuilder<>{}.vlan_any().ethertype(0x22F0).result(flag::rx_avtp_stream).build();
    TcamClassifier<> tc;
    tc.set_rules(rules, 0u);

    auto untagged = make_untagged_avtp_frame(
        {0x91, 0xE0, 0xF0, 0x00, 0x00, 0x42}, {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}, 0x22F0, 0x02, {0, 0, 0, 0, 0, 0, 0, 0});
    auto tagged = make_tagged_avtp_frame(
        {0x91, 0xE0, 0xF0, 0x00, 0x00, 0x42}, {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}, 2, 0x22F0, 0x02, {0, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(tc.classify_first(untagged), flag::rx_avtp_stream);
    EXPECT_EQ(tc.classify_first(tagged), flag::rx_avtp_stream);
}

TEST(tcam_classifier, stream_id_matches_at_correct_offset_untagged)
{
    std::array<std::uint8_t, 8> const my_sid = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    std::array<std::uint8_t, 8> const other_sid = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    auto rules = ClassifierRuleBuilder<>{}.ethertype(0x22F0).avtp_subtype(0x02).stream_id(my_sid).result(flag::process).build();
    TcamClassifier<> tc;
    tc.set_rules(rules, flag::drop);

    auto match_frame =
        make_untagged_avtp_frame({0x91, 0xE0, 0xF0, 0x00, 0x00, 0x42}, {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}, 0x22F0, 0x02, my_sid);
    auto miss_frame = make_untagged_avtp_frame(
        {0x91, 0xE0, 0xF0, 0x00, 0x00, 0x42}, {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}, 0x22F0, 0x02, other_sid);
    EXPECT_EQ(tc.classify_first(match_frame), flag::process);
    EXPECT_EQ(tc.classify_first(miss_frame), flag::drop);
}

TEST(tcam_classifier, first_match_wins)
{
    auto rules_a = ClassifierRuleBuilder<>{}.ethertype(0x22F0).result(0x01U).build();
    auto rules_b = ClassifierRuleBuilder<>{}.ethertype(0x22F0).result(0x02U).build();
    std::vector<CompiledRule<>> combined;
    combined.insert(combined.end(), rules_a.begin(), rules_a.end());
    combined.insert(combined.end(), rules_b.begin(), rules_b.end());
    TcamClassifier<> tc;
    tc.set_rules(combined, 0u);
    auto f = make_untagged_avtp_frame(
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0x22F0, 0x00, {0, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(tc.classify_first(f), 0x01U);
}

TEST(tcam_classifier, classify_all_ors_every_matching_rule)
{
    auto rules_a = ClassifierRuleBuilder<>{}.ethertype(0x22F0).result(0x01U).build();
    auto rules_b = ClassifierRuleBuilder<>{}.ethertype(0x22F0).result(0x02U).build();
    std::vector<CompiledRule<>> combined;
    combined.insert(combined.end(), rules_a.begin(), rules_a.end());
    combined.insert(combined.end(), rules_b.begin(), rules_b.end());
    TcamClassifier<> tc;
    tc.set_rules(combined, 0u);
    auto f = make_untagged_avtp_frame(
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0x22F0, 0x00, {0, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(tc.classify_all(f), 0x03U);
}

TEST(tcam_classifier_64, distinguishes_udp_dest_port_in_ipv4)
{
    // Template a 64-byte window so the UDP dest port fits. Hand-build
    // a CompiledRule<64> that matches IPv4+UDP+port-319 (gPTP) in an
    // untagged frame:
    //   bytes 12-13: EtherType 0x0800
    //   byte 14 (IHL/version) partial match
    //   byte 23 (IPv4 protocol) == 17 (UDP)
    //   bytes 36-37 (UDP dest port) == 319 = 0x013F
    CompiledRule<64> r{};
    r.mask[12] = 0xFF;
    r.mask[13] = 0xFF;
    r.match[12] = 0x08;
    r.match[13] = 0x00;
    r.mask[23] = 0xFF;
    r.match[23] = 17;
    r.mask[36] = 0xFF;
    r.mask[37] = 0xFF;
    r.match[36] = 0x01;
    r.match[37] = 0x3F;
    r.result_flags = flag::rx_gptp;
    r.min_frame_size = 64;

    TcamClassifier<64> tc;
    tc.set_rules(std::span<CompiledRule<64> const>(&r, 1), flag::forward_tap);

    std::array<std::uint8_t, 64> frame{};
    // minimal IPv4 header: version=4, IHL=5, protocol=17, rest zero.
    frame[12] = 0x08;
    frame[13] = 0x00;  // IPv4 EtherType
    frame[14] = 0x45;  // version=4, IHL=5
    frame[23] = 17;    // protocol UDP
    frame[36] = 0x01;
    frame[37] = 0x3F;  // UDP dest port 319
    EXPECT_EQ(tc.classify_first(frame), flag::rx_gptp);

    frame[37] = 0x40;  // dest port 320 → no longer matches
    EXPECT_EQ(tc.classify_first(frame), flag::forward_tap);
}

TEST(tcam_classifier_capacity, small_capacity_rejects_overflow)
{
    // Template capacity down to 1 so we can prove assign throws
    // on overflow without allocating a huge ruleset.
    TcamClassifier<32, 1> tc;
    auto one_rule = ClassifierRuleBuilder<>{}.ethertype(0x22F0).result(flag::process).build();
    tc.set_rules(one_rule, flag::forward_tap);
    EXPECT_EQ(tc.rule_count(), 1u);

    std::vector<CompiledRule<32>> too_many(one_rule.begin(), one_rule.end());
    too_many.push_back(one_rule[0]);  // two rules into a capacity-1 classifier
    bool threw = false;
    try {
        tc.set_rules(too_many);
    } catch (std::bad_alloc const&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
}

TEST_MAIN(statusbar_net, net_tcam_classifier_test)
