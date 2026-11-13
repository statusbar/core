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

// ---------------------------------------------------------------------------
// Who takes the rules out again
// ---------------------------------------------------------------------------
//
// RxFlowSteering, as a guard, for the reason RssSteering is one: a classifier
// rule outlives the process that inserted it, and a NIC left with a stale rule
// steers a stream at a queue nobody reads. Best effort, as any destructor is;
// a crash leaks the rules, which is what the range is for - the next owner
// finds them in its range and removes them before it installs its own.

/// Holds a plan's rules and RSS table on an interface, and puts both back.
///
/// steer() clears the plan's range of a previous owner's leftovers, turns the
/// ntuple filters on when the plan asks and they are off, inserts the rules,
/// and writes an RSS table that leaves the plan's queues out. restore() takes
/// the rules out and puts the table back: the one read before the write, or
/// the driver's default when the table found was a dead owner's. The ntuple
/// filters stay on either way: igb and igc reset the adapter to change them,
/// and igb its PHC with it.
class RxFlowSteering
{
  public:
    /// A guard that owes nothing.
    RxFlowSteering() = default;

    ~RxFlowSteering();

    RxFlowSteering(RxFlowSteering&& other) noexcept;
    /// The guard assigned over restores first, so steer() a plan for a range
    /// a live guard holds only after that guard's restore(): its restore would
    /// otherwise take the new rules out.
    auto operator=(RxFlowSteering&& other) noexcept -> RxFlowSteering&;

    RxFlowSteering(RxFlowSteering const&) = delete;
    auto operator=(RxFlowSteering const&) -> RxFlowSteering& = delete;

    /// Install `plan` on `interface_name`. On any refusal what was done is
    /// undone and the refusal returned: `invalid_argument` from
    /// check_rx_flow_plan, `operation_not_supported` from a NIC with no
    /// classifier, the driver's own otherwise. A NIC without RSS keeps the
    /// rules alone.
    /// @param flows The classifier to steer; must outlive the guard
    /// @param rss The RSS table to spread; must outlive the guard
    /// @param interface_name The interface to steer
    /// @param plan What to install
    [[nodiscard]] static auto steer(RxFlowControl& flows, RssControl& rss, std::string_view interface_name, RxFlowPlan plan)
        -> StatusValue<RxFlowSteering>;

    /// The RSS table back and the rules removed, once: the first refusal is
    /// returned and nothing more is owed, since a second try would meet the
    /// same NIC. Success when nothing was owed, so it is safe to call twice.
    /// Refuses the table with `state_not_recoverable` when its size changed
    /// under us (the channel count moved), and still removes the rules.
    [[nodiscard]] auto restore() -> Status;

    /// Give up the restore deliberately, leaving the NIC steered.
    auto release() noexcept -> void;

    /// Whether a restore is still owed.
    [[nodiscard]] auto holds() const noexcept -> bool { return owed_.flows != nullptr; }
    /// How many rules a previous owner had left in the range at steer().
    [[nodiscard]] auto leftovers() const noexcept -> size_t { return installed_.leftovers; }
    /// Whether steer() turned the ntuple filters on (they stay on).
    [[nodiscard]] auto enabled_ntuple() const noexcept -> bool { return installed_.enabled_ntuple; }
    /// Whether steer() wrote an RSS table, so a restore puts one back.
    [[nodiscard]] auto wrote_rss() const noexcept -> bool { return !owed_.previous.empty(); }
    /// The plan installed.
    [[nodiscard]] auto plan() const noexcept -> RxFlowPlan const& { return installed_.plan; }

  private:
    /// What steer() installed, kept for the report after a restore.
    struct Installed
    {
        std::string interface;
        RxFlowPlan plan;
        uint32_t rx_queues{0};
        size_t leftovers{0};
        bool enabled_ntuple{false};
    };

    /// What is owed to the NIC until restore() or release(); empty owes nothing.
    struct Owed
    {
        RxFlowControl* flows{nullptr};
        RssControl* rss{nullptr};
        RssIndirection previous;  ///< the table read before the write, to put back
        size_t inserted{0};       ///< how many of the plan's rules are in, to take out
        bool default_rss{false};  ///< put the driver's default back, not `previous`
    };

    RxFlowSteering(RxFlowControl& flows, RssControl& rss, std::string_view interface_name, RxFlowPlan plan, uint32_t rx_queues);

    [[nodiscard]] auto install() -> Status;
    [[nodiscard]] auto clear_leftovers() -> Status;
    /// Whether the plan wants the ntuple filters on and they are off.
    [[nodiscard]] auto ntuple_write_due() const -> StatusValue<bool>;
    [[nodiscard]] auto enable_ntuple() -> Status;
    [[nodiscard]] auto insert_rules() -> Status;
    [[nodiscard]] auto steer_rss() -> Status;
    [[nodiscard]] auto write_rss(RssIndirection const& wanted, RssIndirection current) -> Status;
    [[nodiscard]] auto restore_rss() -> Status;
    [[nodiscard]] auto remove_rules() -> Status;

    Installed installed_;
    Owed owed_;
};

}  // namespace statusbar::net
