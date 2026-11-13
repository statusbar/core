// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// Unit tests for the RX flow rules module.
//
// The decisions - what a plan may ask for, how the RSS table is spread round
// the steered queues, how a rule and its ethtool spec map onto each other -
// are pure and tested here without a NIC. The install-and-restore protocol
// of RxFlowSteering is driven through an RxFlowRecorder and an RssRecorder,
// which is what lets "the rules are gone and the table is back" be asserted.
//
// A real insert needs a NIC with a classifier and CAP_NET_ADMIN; no host this
// was written on has one, so the real ioctl path is exercised only by its
// failure cases.

#include "statusbar/net/net_rx_flow.hpp"

#include "statusbar/net/net_rss_recorder.hpp"
#include "statusbar/net/net_rx_flow_recorder.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <system_error>
#include <vector>

using namespace statusbar;
using namespace statusbar::net;

namespace {

constexpr uint16_t AVTP_ETHERTYPE = 0x22f0;

[[nodiscard]] auto ethertype_rule(uint32_t const location, uint32_t const queue, uint16_t const ethertype) -> RxFlowRule
{
    return RxFlowRule{.location = location, .queue = queue, .ethertype = ethertype};
}

[[nodiscard]] auto pcp_rule(uint32_t const location, uint32_t const queue, uint8_t const pcp) -> RxFlowRule
{
    return RxFlowRule{.location = location, .queue = queue, .vlan_pcp = pcp};
}

}  // namespace

// The spread

TEST(net_rx_flow, the_spread_goes_round_robin_over_the_queues_left)
{
    std::array<uint32_t, 1> const excluded{2};
    auto const table = rss_indirection_excluding(8, 4, excluded);
    std::vector<uint32_t> const expected{0, 1, 3, 0, 1, 3, 0, 1};
    EXPECT_TRUE(table == expected);
}

TEST(net_rx_flow, excluding_nothing_spreads_over_every_queue)
{
    auto const table = rss_indirection_excluding(4, 2, {});
    std::vector<uint32_t> const expected{0, 1, 0, 1};
    EXPECT_TRUE(table == expected);
}

TEST(net_rx_flow, nothing_left_to_spread_over_is_an_empty_table)
{
    std::array<uint32_t, 2> const excluded{0, 1};
    EXPECT_TRUE(rss_indirection_excluding(8, 2, excluded).empty());
    EXPECT_TRUE(rss_indirection_excluding(8, 0, {}).empty());
}

// The plan check

TEST(net_rx_flow, a_plan_within_its_range_and_queues_is_accepted)
{
    RxFlowPlan const plan{
        .locations = {.first = 8, .count = 4},
        .rules = {ethertype_rule(8, 1, AVTP_ETHERTYPE), pcp_rule(9, 1, 3)},
        .rss_excluded = {1}};
    EXPECT_TRUE(detail::check_rx_flow_plan(plan, 4).has_value());
}

TEST(net_rx_flow, a_rule_outside_the_range_is_refused)
{
    RxFlowPlan const plan{.locations = {.first = 8, .count = 4}, .rules = {ethertype_rule(12, 1, AVTP_ETHERTYPE)}};
    auto const checked = detail::check_rx_flow_plan(plan, 4);
    EXPECT_FALSE(checked.has_value());
    EXPECT_TRUE(checked.error() == std::errc::invalid_argument);
}

TEST(net_rx_flow, a_queue_the_nic_does_not_have_is_refused)
{
    RxFlowPlan const rule_queue{.locations = {.first = 0, .count = 4}, .rules = {ethertype_rule(0, 4, AVTP_ETHERTYPE)}};
    EXPECT_FALSE(detail::check_rx_flow_plan(rule_queue, 4).has_value());

    RxFlowPlan const excluded_queue{.locations = {.first = 0, .count = 4}, .rules = {}, .rss_excluded = {7}};
    EXPECT_FALSE(detail::check_rx_flow_plan(excluded_queue, 4).has_value());
}

TEST(net_rx_flow, two_rules_at_one_location_are_refused)
{
    RxFlowPlan const plan{
        .locations = {.first = 0, .count = 4}, .rules = {ethertype_rule(1, 0, AVTP_ETHERTYPE), pcp_rule(1, 0, 3)}};
    EXPECT_FALSE(detail::check_rx_flow_plan(plan, 4).has_value());
}

TEST(net_rx_flow, a_rule_matching_on_nothing_is_refused)
{
    RxFlowPlan const plan{.locations = {.first = 0, .count = 4}, .rules = {RxFlowRule{.location = 0, .queue = 1}}};
    EXPECT_FALSE(detail::check_rx_flow_plan(plan, 4).has_value());
}

TEST(net_rx_flow, a_vlan_priority_past_seven_is_refused)
{
    RxFlowPlan const plan{.locations = {.first = 0, .count = 4}, .rules = {pcp_rule(0, 1, 8)}};
    EXPECT_FALSE(detail::check_rx_flow_plan(plan, 4).has_value());
}

TEST(net_rx_flow, a_range_contains_its_slots_alone)
{
    constexpr RxFlowRange range{.first = 16, .count = 4};
    EXPECT_FALSE(range.contains(15));
    EXPECT_TRUE(range.contains(16));
    EXPECT_TRUE(range.contains(19));
    EXPECT_FALSE(range.contains(20));
    constexpr RxFlowRange empty{.first = 3, .count = 0};
    EXPECT_FALSE(empty.contains(3));
}

// The spec, where there is one

#if defined(__linux__)

TEST(net_rx_flow, an_ethertype_rule_comes_back_from_its_spec)
{
    auto const rule = ethertype_rule(5, 2, AVTP_ETHERTYPE);
    auto const spec = detail::flow_spec_of(rule);
    EXPECT_EQ(spec.flow_type, static_cast<uint32_t>(ETHER_FLOW));
    EXPECT_EQ(spec.location, 5U);
    EXPECT_EQ(spec.ring_cookie, 2U);
    EXPECT_EQ(spec.m_u.ether_spec.h_proto, static_cast<__be16>(0xffff));
    EXPECT_TRUE(detail::rule_of(spec) == rule);
}

TEST(net_rx_flow, a_priority_rule_masks_the_three_priority_bits_alone)
{
    auto const rule = pcp_rule(6, 3, 5);
    auto const spec = detail::flow_spec_of(rule);
    EXPECT_EQ(spec.flow_type, static_cast<uint32_t>(ETHER_FLOW | FLOW_EXT));
    EXPECT_EQ(ntohs(spec.m_ext.vlan_tci), 0xe000U);
    EXPECT_EQ(ntohs(spec.h_ext.vlan_tci), static_cast<uint16_t>(5U << 13U));
    EXPECT_EQ(spec.m_u.ether_spec.h_proto, static_cast<__be16>(0));
    EXPECT_TRUE(detail::rule_of(spec) == rule);
}

TEST(net_rx_flow, a_rule_on_both_fields_keeps_both)
{
    RxFlowRule const rule{.location = 1, .queue = 1, .ethertype = AVTP_ETHERTYPE, .vlan_pcp = 3};
    EXPECT_TRUE(detail::rule_of(detail::flow_spec_of(rule)) == rule);
}

TEST(net_rx_flow, a_spec_matching_on_other_fields_is_marked)
{
    auto spec = detail::flow_spec_of(ethertype_rule(1, 1, AVTP_ETHERTYPE));
    spec.m_u.ether_spec.h_dest[0] = 0xff;
    auto const marked = detail::rule_of(spec);
    EXPECT_TRUE(marked.other_fields);
    EXPECT_FALSE(marked.ethertype.has_value());
    EXPECT_EQ(marked.location, 1U);

    auto whole_tci = detail::flow_spec_of(pcp_rule(2, 1, 3));
    whole_tci.m_ext.vlan_tci = htons(0xffff);
    EXPECT_TRUE(detail::rule_of(whole_tci).other_fields);

    auto other_type = detail::flow_spec_of(ethertype_rule(3, 1, AVTP_ETHERTYPE));
    other_type.flow_type = IP_USER_FLOW;
    EXPECT_TRUE(detail::rule_of(other_type).other_fields);
}

TEST(net_rx_flow, a_drop_rule_is_marked)
{
    auto spec = detail::flow_spec_of(ethertype_rule(1, 1, AVTP_ETHERTYPE));
    spec.ring_cookie = RX_CLS_FLOW_DISC;
    EXPECT_TRUE(detail::rule_of(spec).other_fields);
}

#endif  // __linux__

// Clearing a range

TEST(net_rx_flow, clearing_removes_and_counts_the_rules_in_the_range_alone)
{
    RxFlowRecorder nic;
    nic.place(ethertype_rule(7, 0, AVTP_ETHERTYPE));   // just below the range
    nic.place(ethertype_rule(8, 0, AVTP_ETHERTYPE));   // in it
    nic.place(pcp_rule(10, 1, 3));                     // in it
    nic.place(ethertype_rule(12, 0, AVTP_ETHERTYPE));  // just past it

    auto const removed = clear_rx_flow_rules(nic, "eth0", RxFlowRange{.first = 8, .count = 4});
    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 2U);
    EXPECT_EQ(nic.rules().size(), 2U);
    EXPECT_EQ(nic.count(RxFlowRecorder::Op::rule_read), 4U);
    EXPECT_EQ(nic.count(RxFlowRecorder::Op::remove), 2U);
}

TEST(net_rx_flow, clearing_an_empty_range_touches_nothing)
{
    RxFlowRecorder nic;
    nic.place(ethertype_rule(0, 0, AVTP_ETHERTYPE));
    auto const removed = clear_rx_flow_rules(nic, "eth0", RxFlowRange{.first = 0, .count = 0});
    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 0U);
    EXPECT_TRUE(nic.calls().empty());
}

TEST(net_rx_flow, clearing_stops_at_a_nic_without_a_classifier)
{
    RxFlowRecorder nic;
    nic.set_unsupported();
    auto const removed = clear_rx_flow_rules(nic, "eth0", RxFlowRange{.first = 0, .count = 4});
    EXPECT_FALSE(removed.has_value());
    EXPECT_TRUE(removed.error() == std::errc::operation_not_supported);
    EXPECT_EQ(nic.count(RxFlowRecorder::Op::rule_read), 1U);
}

// The real NIC, where the only answer anyone can count on is a refusal

TEST(net_rx_flow, loopback_has_no_classifier)
{
    auto const rule = rx_flow_rule("lo", 0);
    EXPECT_FALSE(rule.has_value());
    if (!rule) {
        EXPECT_TRUE(rule.error() == std::errc::operation_not_supported);
    }
}

TEST(net_rx_flow, an_unknown_interface_is_told_apart_from_no_classifier)
{
#if !defined(__linux__)
    SKIP("the classifier is Linux-only; the stub answers operation_not_supported for every name");
#endif
    auto const rule = rx_flow_rule("statusbar-no-such-if", 0);
    EXPECT_FALSE(rule.has_value());
    if (!rule) {
        EXPECT_FALSE(rule.error() == std::errc::operation_not_supported);
    }
}

TEST_MAIN(statusbar_net, net_rx_flow_test)
