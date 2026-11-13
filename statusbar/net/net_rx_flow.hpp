#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// The NIC's RX classification rules (`ethtool -N <if> flow-type ether ...`,
/// ETHTOOL_SRXCLSRLINS and friends in linux/ethtool.h): frames of one
/// EtherType, or of one VLAN priority, steered to an RX queue of their own.
///
/// An AF_XDP socket binds to one RX queue, and RSS hashes everything else
/// across all of them. net_rss.hpp's answer is to point every hash bucket at
/// the socket's queue, which hands the socket the host's whole inbound
/// traffic along with its streams. A classifier rule is the finer tool: the
/// AVTP EtherType, or the Class A priority, goes to the socket's queue, and an
/// RSS table that leaves that queue out keeps the rest of the traffic off it.
/// RxFlowSteering installs that pair and removes it again; what it needs of
/// the NIC is behind RxFlowControl, so the install-and-restore protocol is
/// tested here against a recorder (net_rx_flow_recorder.hpp) rather than
/// against a multi-queue NIC with CAP_NET_ADMIN, which no test host has.

#include "statusbar/net/net_rss.hpp"
#include "statusbar/status/status.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(__linux__)
#    include <linux/ethtool.h>
#endif

namespace statusbar::net {

/// One ETHER_FLOW rule: a frame matching every field that is set goes to
/// `queue`. A rule read back from the NIC that matches on anything this type
/// does not hold (a MAC address, a partial EtherType, a whole VLAN TCI) is
/// reported with `other_fields` set and its match fields empty.
struct RxFlowRule
{
    uint32_t location{0};               ///< the slot in the NIC's classifier table
    uint32_t queue{0};                  ///< the RX queue a matching frame goes to
    std::optional<uint16_t> ethertype;  ///< the frame's EtherType, matched whole
    std::optional<uint8_t> vlan_pcp;    ///< the VLAN tag's priority, 0 to 7, matched alone
    bool other_fields{false};           ///< a rule read back that matches on more than these two

    [[nodiscard]] auto operator==(RxFlowRule const&) const noexcept -> bool = default;
};

/// A run of classifier slots an owner calls its own: every rule found in it
/// at steer() is a previous owner's leftover and is removed.
struct RxFlowRange
{
    uint32_t first{0};
    uint32_t count{0};

    [[nodiscard]] constexpr auto contains(uint32_t const location) const noexcept -> bool
    {
        return location >= first && location - first < count;
    }
};

/// What RxFlowSteering installs: `rules`, each within `locations`, and an RSS
/// table that leaves `rss_excluded` out. `ntuple` turns the NIC's ntuple
/// filters on first when they are off: igb needs them for a VLAN priority
/// rule, igc honours a rule without them.
struct RxFlowPlan
{
    RxFlowRange locations;
    std::vector<RxFlowRule> rules;
    std::vector<uint32_t> rss_excluded;
    bool ntuple{true};
};

/// Whether the interface's ntuple filters are on (ETHTOOL_GFLAGS,
/// ETH_FLAG_NTUPLE). Needs no privilege.
[[nodiscard]] auto ntuple_enabled(std::string_view interface_name) -> StatusValue<bool>;

/// Turn the ntuple filters on or off (ETHTOOL_SFLAGS), the other flags kept as
/// they are. Needs CAP_NET_ADMIN. igb and igc reset the adapter on the change
/// and drop every classifier rule when the filters go off.
[[nodiscard]] auto set_ntuple(std::string_view interface_name, bool enabled) -> Status;

/// The rule at `location` (ETHTOOL_GRXCLSRULE), or nullopt for an empty slot.
/// `operation_not_supported` for an interface with no classifier at all.
[[nodiscard]] auto rx_flow_rule(std::string_view interface_name, uint32_t location) -> StatusValue<std::optional<RxFlowRule>>;

/// Insert `rule` at its location (ETHTOOL_SRXCLSRLINS), replacing whatever
/// was in that slot. Needs CAP_NET_ADMIN.
[[nodiscard]] auto insert_rx_flow_rule(std::string_view interface_name, RxFlowRule const& rule) -> Status;

/// Remove the rule at `location` (ETHTOOL_SRXCLSRLDEL). Needs CAP_NET_ADMIN.
[[nodiscard]] auto delete_rx_flow_rule(std::string_view interface_name, uint32_t location) -> Status;

/// An RSS table of `entries` buckets spread round robin over the RX queues
/// below `rx_queues` that `excluded` does not name; empty when no queue is
/// left, which a caller reads as "nothing to write".
[[nodiscard]] auto rss_indirection_excluding(size_t entries, uint32_t rx_queues, std::span<uint32_t const> excluded)
    -> RssIndirection;

namespace detail {

/// Whether `plan` can go on a NIC with `rx_queues` RX queues: `invalid_argument`
/// unless every rule lies in the plan's range at a location of its own, matches
/// on an EtherType or a VLAN priority of 0 to 7 (igc reads a rule on neither as
/// its default-queue wildcard), and names a queue below `rx_queues`, as every
/// excluded queue must too.
[[nodiscard]] auto check_rx_flow_plan(RxFlowPlan const& plan, uint32_t rx_queues) -> Status;

#if defined(__linux__)
/// The ethtool_rx_flow_spec that inserts `rule`: an ETHER_FLOW on the
/// EtherType under a full mask, FLOW_EXT with the three priority bits of the
/// VLAN TCI masked in for the priority, ring_cookie the queue.
[[nodiscard]] auto flow_spec_of(RxFlowRule const& rule) noexcept -> ethtool_rx_flow_spec;

/// The rule a spec read back describes; `other_fields` when it is not an
/// ETHER_FLOW on a whole EtherType and/or a VLAN priority alone, or drops.
[[nodiscard]] auto rule_of(ethtool_rx_flow_spec const& spec) noexcept -> RxFlowRule;
#endif

}  // namespace detail

/// The calls a steerer makes of a NIC's classifier. An interface rather than
/// the free calls above so the install-and-restore protocol can be tested:
/// RxFlowEthtool talks to the NIC, RxFlowRecorder (net_rx_flow_recorder.hpp)
/// keeps a pretend table.
class RxFlowControl
{
  public:
    RxFlowControl() = default;
    virtual ~RxFlowControl() = default;
    RxFlowControl(RxFlowControl const&) = delete;
    auto operator=(RxFlowControl const&) -> RxFlowControl& = delete;
    RxFlowControl(RxFlowControl&&) = delete;
    auto operator=(RxFlowControl&&) -> RxFlowControl& = delete;

    /// Whether the ntuple filters are on.
    [[nodiscard]] virtual auto ntuple(std::string_view interface_name) -> StatusValue<bool> = 0;
    /// Turn the ntuple filters on or off.
    [[nodiscard]] virtual auto set_ntuple(std::string_view interface_name, bool enabled) -> Status = 0;
    /// The rule at a location, nullopt for an empty slot.
    [[nodiscard]] virtual auto rule(std::string_view interface_name, uint32_t location)
        -> StatusValue<std::optional<RxFlowRule>> = 0;
    /// Insert a rule at its location.
    [[nodiscard]] virtual auto insert(std::string_view interface_name, RxFlowRule const& rule) -> Status = 0;
    /// Remove the rule at a location.
    [[nodiscard]] virtual auto remove(std::string_view interface_name, uint32_t location) -> Status = 0;
    /// How many RX queues the NIC has.
    [[nodiscard]] virtual auto rx_queues(std::string_view interface_name) -> StatusValue<uint32_t> = 0;
};

/// The RxFlowControl that talks to a NIC, through ethtool's ioctls.
class RxFlowEthtool final : public RxFlowControl
{
  public:
    [[nodiscard]] auto ntuple(std::string_view const interface_name) -> StatusValue<bool> override
    {
        return ntuple_enabled(interface_name);
    }

    [[nodiscard]] auto set_ntuple(std::string_view const interface_name, bool const enabled) -> Status override
    {
        return net::set_ntuple(interface_name, enabled);
    }

    [[nodiscard]] auto rule(std::string_view const interface_name, uint32_t const location)
        -> StatusValue<std::optional<RxFlowRule>> override
    {
        return rx_flow_rule(interface_name, location);
    }

    [[nodiscard]] auto insert(std::string_view const interface_name, RxFlowRule const& rule) -> Status override
    {
        return insert_rx_flow_rule(interface_name, rule);
    }

    [[nodiscard]] auto remove(std::string_view const interface_name, uint32_t const location) -> Status override
    {
        return delete_rx_flow_rule(interface_name, location);
    }

    [[nodiscard]] auto rx_queues(std::string_view const interface_name) -> StatusValue<uint32_t> override
    {
        return rx_queue_count(interface_name);
    }
};

/// Remove every rule in `range` - the leftovers of an owner that died before
/// it could - and say how many there were. Stops at the first refusal.
[[nodiscard]] auto clear_rx_flow_rules(RxFlowControl& control, std::string_view interface_name, RxFlowRange range)
    -> StatusValue<size_t>;

}  // namespace statusbar::net
