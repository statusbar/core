// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Tests for ClassifierRuleBuilder -> CompiledRule compilation.

#include "statusbar/net/net_compiled_rule.hpp"

#include "statusbar/ieee/ieee.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <cstdint>

using namespace statusbar;
using namespace statusbar::net;

namespace {

template <std::size_t N>
auto ethertype_at(CompiledRule<N> const& r, std::size_t offset) -> std::uint16_t
{
    return static_cast<std::uint16_t>(r.match[offset] << 8 | r.match[offset + 1]);
}

}  // namespace

TEST(compiled_rule, default_builder_emits_single_empty_rule)
{
    auto rules = ClassifierRuleBuilder<>{}.build();
    EXPECT_EQ(rules.size(), 1u);
    EXPECT_EQ(rules[0].result_flags, 0u);
    // Empty mask means "match anything" — zero AND zero == zero.
    for (auto b : rules[0].mask) {
        EXPECT_EQ(b, 0u);
    }
}

TEST(compiled_rule, ethertype_untagged_placed_at_offset_12)
{
    auto rules = ClassifierRuleBuilder<>{}.ethertype(0x22F0).result(flag::process).build();
    EXPECT_EQ(rules.size(), 1u);
    EXPECT_EQ(rules[0].mask[12], 0xFFu);
    EXPECT_EQ(rules[0].mask[13], 0xFFu);
    EXPECT_EQ(ethertype_at(rules[0], 12), 0x22F0u);
    EXPECT_EQ(rules[0].result_flags, flag::process);
}

TEST(compiled_rule, ethertype_tagged_places_tpid_at_12_and_type_at_16)
{
    auto rules = ClassifierRuleBuilder<>{}.vlan_tagged_any().ethertype(0x22F0).build();
    EXPECT_EQ(rules.size(), 1u);
    // TPID at 12 is 0x8100.
    EXPECT_EQ(ethertype_at(rules[0], 12), 0x8100u);
    EXPECT_EQ(rules[0].mask[12], 0xFFu);
    EXPECT_EQ(rules[0].mask[13], 0xFFu);
    // Inner EtherType at 16.
    EXPECT_EQ(ethertype_at(rules[0], 16), 0x22F0u);
    EXPECT_EQ(rules[0].mask[16], 0xFFu);
    EXPECT_EQ(rules[0].mask[17], 0xFFu);
}

TEST(compiled_rule, vlan_tagged_exact_sets_vid_mask_only)
{
    auto rules = ClassifierRuleBuilder<>{}.vlan_tagged_exact(2).build();
    EXPECT_EQ(rules.size(), 1u);
    // Low 12 bits of bytes 14-15 = VID 2.
    EXPECT_EQ(rules[0].mask[14], 0x0Fu);
    EXPECT_EQ(rules[0].mask[15], 0xFFu);
    EXPECT_EQ(rules[0].match[14], 0x00u);
    EXPECT_EQ(rules[0].match[15], 0x02u);
}

TEST(compiled_rule, dest_mac_packed_at_offset_0)
{
    ieee::Eui48 const dest{0x91, 0xE0, 0xF0, 0x00, 0x00, 0x42};
    auto rules = ClassifierRuleBuilder<>{}.dest_mac(dest).build();
    EXPECT_EQ(rules.size(), 1u);
    for (std::size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(rules[0].mask[i], 0xFFu);
        EXPECT_EQ(rules[0].match[i], dest.value[i]);
    }
    // Source MAC slot must remain zeroed.
    for (std::size_t i = 6; i < 12; ++i) {
        EXPECT_EQ(rules[0].mask[i], 0u);
    }
}

TEST(compiled_rule, avtp_stream_id_untagged_at_offset_18)
{
    std::array<std::uint8_t, 8> const sid{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    auto rules = ClassifierRuleBuilder<>{}.avtp_subtype(0x02).stream_id(sid).build();
    EXPECT_EQ(rules.size(), 1u);
    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(rules[0].mask[18 + i], 0xFFu);
        EXPECT_EQ(rules[0].match[18 + i], sid[i]);
    }
}

TEST(compiled_rule, avtp_stream_id_tagged_at_offset_22)
{
    std::array<std::uint8_t, 8> const sid{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    auto rules = ClassifierRuleBuilder<>{}.vlan_tagged_any().avtp_subtype(0x02).stream_id(sid).build();
    EXPECT_EQ(rules.size(), 1u);
    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(rules[0].mask[22 + i], 0xFFu);
        EXPECT_EQ(rules[0].match[22 + i], sid[i]);
    }
}

TEST(compiled_rule, vlan_any_fan_out_produces_two_rules)
{
    auto rules = ClassifierRuleBuilder<>{}.ethertype(0x22F0).vlan_any().result(flag::rx_avtp_stream).build();
    EXPECT_EQ(rules.size(), 2u);
    // First variant: untagged, EtherType at 12.
    EXPECT_EQ(ethertype_at(rules[0], 12), 0x22F0u);
    // Second variant: TPID at 12, EtherType at 16.
    EXPECT_EQ(ethertype_at(rules[1], 12), 0x8100u);
    EXPECT_EQ(ethertype_at(rules[1], 16), 0x22F0u);
    EXPECT_EQ(rules[0].result_flags, flag::rx_avtp_stream);
    EXPECT_EQ(rules[1].result_flags, flag::rx_avtp_stream);
}

TEST(compiled_rule, stream_id_u64_big_endian_packs_msb_first)
{
    auto rules = ClassifierRuleBuilder<>{}.avtp_subtype(0x02).stream_id(0x0011223344556677ULL).build();
    std::array<std::uint8_t, 8> const expected{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(rules[0].match[18 + i], expected[i]);
    }
}

TEST(compiled_rule_flags, user_bits_do_not_overlap_system_bits)
{
    EXPECT_EQ(flag::SYSTEM_MASK & flag::USER_MASK, 0ULL);
    EXPECT_EQ(flag::SYSTEM_MASK | flag::USER_MASK, ~0ULL);
    // All named system flags fall in the system mask.
    EXPECT_EQ(flag::process & flag::USER_MASK, 0ULL);
    EXPECT_EQ(flag::rx_avtp_stream & flag::USER_MASK, 0ULL);
}

TEST(compiled_rule_flags, user_bit_template_and_runtime_agree)
{
    EXPECT_EQ(flag::user_bit<0>, 1ULL << 16);
    EXPECT_EQ(flag::user_bit<47>, 1ULL << 63);
    EXPECT_EQ(flag::make_user_bit(0), flag::user_bit<0>);
    EXPECT_EQ(flag::make_user_bit(47), flag::user_bit<47>);
    // Out-of-range runtime call returns 0 (safe).
    EXPECT_EQ(flag::make_user_bit(48), 0ULL);
}

TEST(compiled_rule_flags, split_helpers_isolate_halves)
{
    std::uint64_t const composite = flag::rx_gptp | flag::user_bit<3> | flag::user_bit<7>;
    EXPECT_EQ(flag::system_bits(composite), flag::rx_gptp);
    EXPECT_EQ(flag::user_bits(composite), flag::user_bit<3> | flag::user_bit<7>);
    EXPECT_EQ(flag::system_bits(composite) | flag::user_bits(composite), composite);
}

TEST(compiled_rule_ip, ipv4_src_dst_packed_at_right_offsets_untagged)
{
    ip::IPv4Address const src{10, 0, 0, 1};
    ip::IPv4Address const dst{192, 168, 1, 42};
    auto rules = ClassifierRuleBuilder<64>{}.ethertype(0x0800).vlan_untagged().ipv4_src(src).ipv4_dst(dst).build();
    EXPECT_EQ(rules.size(), 1u);
    // Untagged: src at 26..29, dst at 30..33.
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(rules[0].mask[26 + i], 0xFFu);
        EXPECT_EQ(rules[0].match[26 + i], src.span()[i]);
        EXPECT_EQ(rules[0].mask[30 + i], 0xFFu);
        EXPECT_EQ(rules[0].match[30 + i], dst.span()[i]);
    }
}

TEST(compiled_rule_ip, ipv6_src_dst_fit_in_64_byte_window_untagged)
{
    ip::IPv6Address const src{0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01};
    ip::IPv6Address const dst{0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x42};
    auto rules = ClassifierRuleBuilder<64>{}.ethertype(0x86DD).vlan_untagged().ipv6_src(src).ipv6_dst(dst).build();
    EXPECT_EQ(rules.size(), 1u);
    // Untagged: src at 22..37, dst at 38..53.
    for (std::size_t i = 0; i < 16; ++i) {
        EXPECT_EQ(rules[0].mask[22 + i], 0xFFu);
        EXPECT_EQ(rules[0].match[22 + i], src.span()[i]);
        EXPECT_EQ(rules[0].mask[38 + i], 0xFFu);
        EXPECT_EQ(rules[0].match[38 + i], dst.span()[i]);
    }
}

TEST(compiled_rule_ip, ipv6_dst_skipped_when_window_too_small)
{
    // A 32-byte window can't fit IPv6 dst (offsets 38..53 untagged).
    // write_range is a silent no-op; the rule survives but the dst
    // field contributes no constraint.
    ip::IPv6Address const dst{0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    auto rules = ClassifierRuleBuilder<32>{}.ethertype(0x86DD).ipv6_dst(dst).build();
    EXPECT_EQ(rules.size(), 1u);
    // Ethernet fields still set; bytes past 32 obviously don't exist.
    EXPECT_EQ(rules[0].mask[12], 0xFFu);
}

TEST(compiled_rule_atdecc, entity_id_fields_use_correct_offsets)
{
    // Shared controller_entity_id offset across ACMP/AECP is AVTP+12.
    // In a 64-byte window untagged that's byte 26..33.
    ieee::Eui64 const ctrl = ieee::Eui64{}.from_uint64(0x0011223344556677ULL);
    ieee::Eui64 const target = ieee::Eui64{}.from_uint64(0x8899AABBCCDDEEFFULL);
    ieee::Eui64 const talker = ieee::Eui64{}.from_uint64(0x1000000000000001ULL);
    ieee::Eui64 const listener = ieee::Eui64{}.from_uint64(0x2000000000000002ULL);
    auto rules = ClassifierRuleBuilder<64>{}
                     .ethertype(0x22F0)
                     .vlan_untagged()
                     .avtp_subtype(0xFC)  // ACMP
                     .atdecc_controller_entity_id(ctrl)
                     .acmp_talker_entity_id(talker)
                     .acmp_listener_entity_id(listener)
                     .build();
    EXPECT_EQ(rules.size(), 1u);
    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(rules[0].match[26 + i], ctrl.span()[i]);      // controller @ 26..33
        EXPECT_EQ(rules[0].match[34 + i], talker.span()[i]);    // talker @ 34..41
        EXPECT_EQ(rules[0].match[42 + i], listener.span()[i]);  // listener @ 42..49
    }

    // AECP target_entity_id sits at AVTP+4 = byte 18..25.
    auto aecp =
        ClassifierRuleBuilder<64>{}.ethertype(0x22F0).vlan_untagged().avtp_subtype(0xFB).aecp_target_entity_id(target).build();
    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(aecp[0].match[18 + i], target.span()[i]);
    }
}

TEST(compiled_rule_constexpr, builder_runs_at_compile_time)
{
    constexpr auto rules = ClassifierRuleBuilder<32>{}
                               .ethertype(0x22F0)
                               .vlan_tagged_exact(2)
                               .avtp_subtype(0x02)
                               .result(flag::rx_avtp_stream | flag::user_bit<0>)
                               .priority(20)
                               .build();
    static_assert(rules.size() == 1, "builder should produce exactly one rule");
    static_assert(rules[0].priority == 20, "priority should round-trip");
    static_assert(rules[0].result_flags == (flag::rx_avtp_stream | flag::user_bit<0>));
    static_assert(rules[0].mask[16] == 0xFF, "tagged EtherType mask at byte 16");
    static_assert(rules[0].match[16] == 0x22);
    static_assert(rules[0].match[17] == 0xF0);
}

TEST(compiled_rule_flags, result_accepts_user_bits_in_high_halves)
{
    constexpr std::uint64_t my_app_flag = flag::user_bit<10>;  // bit 26
    auto rules = ClassifierRuleBuilder<>{}.ethertype(0x22F0).result(flag::process | my_app_flag).build();
    EXPECT_EQ(rules.size(), 1u);
    EXPECT_EQ(rules[0].result_flags, flag::process | my_app_flag);
    EXPECT_EQ(flag::system_bits(rules[0].result_flags), flag::process);
    EXPECT_EQ(flag::user_bits(rules[0].result_flags), my_app_flag);
}

TEST_MAIN(statusbar_net, net_compiled_rule_test)
