// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/net/net_rx_flow.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

#if defined(__linux__)
#    include "statusbar/net/net_posix_util.hpp"

#    include <arpa/inet.h>
#endif

namespace statusbar::net {

namespace {

/// The RX queues below `rx_queues` that `excluded` leaves alone, in order.
[[nodiscard]] auto queues_left(uint32_t const rx_queues, std::span<uint32_t const> const excluded) -> std::vector<uint32_t>
{
    std::vector<uint32_t> left;
    for (uint32_t queue = 0; queue < rx_queues; ++queue) {
        if (!std::ranges::contains(excluded, queue)) {
            left.push_back(queue);
        }
    }
    return left;
}

}  // namespace

auto rss_indirection_excluding(size_t const entries, uint32_t const rx_queues, std::span<uint32_t const> const excluded)
    -> RssIndirection
{
    auto const queues = queues_left(rx_queues, excluded);
    if (queues.empty()) {
        return {};
    }
    RssIndirection table(entries, 0);
    for (size_t bucket = 0; bucket < entries; ++bucket) {
        table[bucket] = queues[bucket % queues.size()];
    }
    return table;
}

namespace detail {

namespace {

constexpr uint8_t MAX_VLAN_PCP = 7;

/// Whether `rule` is one the plan may carry: within `range`, matching on
/// something, its queue below `rx_queues`.
[[nodiscard]] auto rule_fits(RxFlowRule const& rule, RxFlowRange const range, uint32_t const rx_queues) noexcept -> bool
{
    bool const priority_ok = !rule.vlan_pcp.has_value() || *rule.vlan_pcp <= MAX_VLAN_PCP;
    bool const matches = rule.ethertype.has_value() || rule.vlan_pcp.has_value();
    return matches && priority_ok && range.contains(rule.location) && rule.queue < rx_queues;
}

/// Whether two of `rules` name the same slot.
[[nodiscard]] auto locations_collide(std::span<RxFlowRule const> const rules) noexcept -> bool
{
    for (size_t i = 0; i < rules.size(); ++i) {
        if (std::ranges::contains(rules.subspan(i + 1), rules[i].location, &RxFlowRule::location)) {
            return true;
        }
    }
    return false;
}

}  // namespace

auto check_rx_flow_plan(RxFlowPlan const& plan, uint32_t const rx_queues) -> Status
{
    bool const rules_fit = std::ranges::all_of(
        plan.rules, [&plan, rx_queues](RxFlowRule const& rule) { return rule_fits(rule, plan.locations, rx_queues); });
    bool const excluded_fit =
        std::ranges::all_of(plan.rss_excluded, [rx_queues](uint32_t const queue) { return queue < rx_queues; });
    if (!rules_fit || !excluded_fit || locations_collide(plan.rules)) {
        return failure(std::make_error_code(std::errc::invalid_argument));
    }
    return success();
}

}  // namespace detail

#if defined(__linux__)

namespace detail {

namespace {

// The classifier masks say which bits a frame must match: all of them for a
// whole EtherType, the three priority bits of the TCI for a VLAN priority.
// (ethtool(8)'s `m` option is the inverse, the bits to ignore; the CLI turns
// it round before the ioctl.) Both are 16-bit fields in network order.
constexpr uint16_t WHOLE_ETHERTYPE = 0xffff;
constexpr uint16_t VLAN_PRIORITY_BITS = 0xe000;
constexpr unsigned VLAN_PRIORITY_SHIFT = 13;

/// Whether any element of a mask array has a bit set.
template <typename Word, size_t N>
[[nodiscard]] auto any_bit(Word const (&words)[N]) noexcept -> bool
{
    return std::ranges::any_of(words, [](Word const word) { return word != 0; });
}

/// The match fields of `rule` written into `spec`: the whole EtherType, the
/// VLAN priority bits, whichever the rule has.
auto write_match_fields(ethtool_rx_flow_spec& spec, RxFlowRule const& rule) noexcept -> void
{
    if (rule.ethertype) {
        spec.h_u.ether_spec.h_proto = htons(*rule.ethertype);
        spec.m_u.ether_spec.h_proto = htons(WHOLE_ETHERTYPE);
    }
    if (rule.vlan_pcp) {
        spec.h_ext.vlan_tci = htons(static_cast<uint16_t>((*rule.vlan_pcp & MAX_VLAN_PCP) << VLAN_PRIORITY_SHIFT));
        spec.m_ext.vlan_tci = htons(VLAN_PRIORITY_BITS);
    }
}

/// Whether the FLOW_EXT part of `spec` matches on anything but the VLAN
/// priority: a VLAN EtherType, user data, or more of the TCI than the
/// priority bits.
[[nodiscard]] auto ext_matches_more(ethtool_rx_flow_spec const& spec) noexcept -> bool
{
    if ((spec.flow_type & FLOW_EXT) == 0) {
        return false;
    }
    uint16_t const tci_mask = ntohs(spec.m_ext.vlan_tci);
    bool const tci_more = tci_mask != 0 && tci_mask != VLAN_PRIORITY_BITS;
    return spec.m_ext.vlan_etype != 0 || any_bit(spec.m_ext.data) || tci_more;
}

/// Whether the ETHER_FLOW part of `spec` matches on anything but a whole
/// EtherType: a MAC address, or part of the type.
[[nodiscard]] auto ether_matches_more(ethtool_rx_flow_spec const& spec) noexcept -> bool
{
    auto const& mask = spec.m_u.ether_spec;
    uint16_t const type_mask = ntohs(mask.h_proto);
    bool const partial_type = type_mask != 0 && type_mask != WHOLE_ETHERTYPE;
    return partial_type || any_bit(mask.h_dest) || any_bit(mask.h_source);
}

/// Whether `spec` is anything but an ETHER_FLOW steering to a queue: another
/// flow type, a MAC extension, an RSS context, a drop, or a VF's queue.
[[nodiscard]] auto not_a_plain_steer(ethtool_rx_flow_spec const& spec) noexcept -> bool
{
    uint32_t const base_type = spec.flow_type & ~uint32_t{FLOW_EXT};
    bool const drops = spec.ring_cookie == RX_CLS_FLOW_DISC;
    bool const to_a_vf = (spec.ring_cookie & ~uint64_t{ETHTOOL_RX_FLOW_SPEC_RING}) != 0;
    return base_type != ETHER_FLOW || drops || to_a_vf;
}

}  // namespace

auto flow_spec_of(RxFlowRule const& rule) noexcept -> ethtool_rx_flow_spec
{
    ethtool_rx_flow_spec spec{};
    spec.flow_type = rule.vlan_pcp.has_value() ? (ETHER_FLOW | FLOW_EXT) : ETHER_FLOW;
    spec.ring_cookie = rule.queue;
    spec.location = rule.location;
    write_match_fields(spec, rule);
    return spec;
}

auto rule_of(ethtool_rx_flow_spec const& spec) noexcept -> RxFlowRule
{
    RxFlowRule rule{.location = spec.location, .queue = static_cast<uint32_t>(spec.ring_cookie & ETHTOOL_RX_FLOW_SPEC_RING)};
    rule.other_fields = not_a_plain_steer(spec) || ether_matches_more(spec) || ext_matches_more(spec);
    if (rule.other_fields) {
        return rule;
    }
    if (ntohs(spec.m_u.ether_spec.h_proto) == WHOLE_ETHERTYPE) {
        rule.ethertype = ntohs(spec.h_u.ether_spec.h_proto);
    }
    if ((spec.flow_type & FLOW_EXT) != 0 && ntohs(spec.m_ext.vlan_tci) == VLAN_PRIORITY_BITS) {
        rule.vlan_pcp = static_cast<uint8_t>(ntohs(spec.h_ext.vlan_tci) >> VLAN_PRIORITY_SHIFT);
    }
    return rule;
}

}  // namespace detail

namespace {

/// A classifier command for the slot at `location`.
[[nodiscard]] auto classifier_request(uint32_t const cmd, uint32_t const location) noexcept -> ethtool_rxnfc
{
    ethtool_rxnfc request{};
    request.cmd = cmd;
    request.fs.location = location;
    return request;
}

/// The interface's ETH_FLAG_* flags (ETHTOOL_GFLAGS).
[[nodiscard]] auto read_flags(std::string_view const interface_name) -> StatusValue<uint32_t>
{
    ethtool_value value{};
    value.cmd = ETHTOOL_GFLAGS;
    if (auto const status = ethtool_ioctl(interface_name, &value); !status) {
        return failure(status.error());
    }
    return value.data;
}

/// Whether a GRXCLSRULE refusal means an empty slot: igb and igc say EINVAL
/// for one, the drivers that keep a rule list say ENOENT.
[[nodiscard]] auto means_empty_slot(std::error_code const error) noexcept -> bool
{
    return error == std::errc::invalid_argument || error == std::errc::no_such_file_or_directory;
}

}  // namespace

auto ntuple_enabled(std::string_view const interface_name) -> StatusValue<bool>
{
    auto const flags = read_flags(interface_name);
    if (!flags) {
        return failure(flags.error());
    }
    return (*flags & ETH_FLAG_NTUPLE) != 0;
}

auto set_ntuple(std::string_view const interface_name, bool const enabled) -> Status
{
    auto const flags = read_flags(interface_name);
    if (!flags) {
        return failure(flags.error());
    }
    ethtool_value value{};
    value.cmd = ETHTOOL_SFLAGS;
    value.data = enabled ? (*flags | ETH_FLAG_NTUPLE) : (*flags & ~uint32_t{ETH_FLAG_NTUPLE});
    return ethtool_ioctl(interface_name, &value);
}

auto rx_flow_rule(std::string_view const interface_name, uint32_t const location) -> StatusValue<std::optional<RxFlowRule>>
{
    auto request = classifier_request(ETHTOOL_GRXCLSRULE, location);
    if (auto const status = ethtool_ioctl(interface_name, &request); !status) {
        if (means_empty_slot(status.error())) {
            return std::optional<RxFlowRule>{};
        }
        return failure(status.error());
    }
    return std::optional{detail::rule_of(request.fs)};
}

auto insert_rx_flow_rule(std::string_view const interface_name, RxFlowRule const& rule) -> Status
{
    auto request = classifier_request(ETHTOOL_SRXCLSRLINS, rule.location);
    request.fs = detail::flow_spec_of(rule);
    return ethtool_ioctl(interface_name, &request);
}

auto delete_rx_flow_rule(std::string_view const interface_name, uint32_t const location) -> Status
{
    auto request = classifier_request(ETHTOOL_SRXCLSRLDEL, location);
    return ethtool_ioctl(interface_name, &request);
}

#else  // !__linux__

// ethtool's classifier is a Linux interface; nothing equivalent reaches user
// space on the BSDs or macOS.

namespace {

[[nodiscard]] auto not_supported() -> std::error_code
{
    return std::make_error_code(std::errc::operation_not_supported);
}

}  // namespace

auto ntuple_enabled(std::string_view const) -> StatusValue<bool>
{
    return failure(not_supported());
}

auto set_ntuple(std::string_view const, bool const) -> Status
{
    return failure(not_supported());
}

auto rx_flow_rule(std::string_view const, uint32_t const) -> StatusValue<std::optional<RxFlowRule>>
{
    return failure(not_supported());
}

auto insert_rx_flow_rule(std::string_view const, RxFlowRule const&) -> Status
{
    return failure(not_supported());
}

auto delete_rx_flow_rule(std::string_view const, uint32_t const) -> Status
{
    return failure(not_supported());
}

#endif  // __linux__

auto clear_rx_flow_rules(RxFlowControl& control, std::string_view const interface_name, RxFlowRange const range)
    -> StatusValue<size_t>
{
    size_t removed = 0;
    for (uint32_t slot = 0; slot < range.count; ++slot) {
        uint32_t const location = range.first + slot;
        auto const rule = control.rule(interface_name, location);
        if (!rule) {
            return failure(rule.error());
        }
        if (!rule->has_value()) {
            continue;
        }
        if (auto const status = control.remove(interface_name, location); !status) {
            return failure(status.error());
        }
        ++removed;
    }
    return removed;
}

}  // namespace statusbar::net
