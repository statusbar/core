#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// TrafficClassifier — userspace Ethernet frame classifier
/// Mirrors XDP/BPF in-kernel filtering in userspace.
/// Routes frames to process (real-time), forward_tap (kernel stack), or drop
/// based on ethertype, VLAN tag, IP protocol, and UDP destination port.

#include "statusbar/net/net_ethernet_port.hpp"

#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

namespace statusbar::net {

/// Action returned by TrafficClassifier::classify()
enum class FrameAction : uint8_t
{
    process,      ///< Deliver to protocol handler (real-time path)
    forward_tap,  ///< Forward to TAP device (non-real-time traffic)
    drop,         ///< Discard
};

/// A single classifier rule.
/// Rules are evaluated in order; the first matching rule wins.
/// A field value of 0 means "match any" (not checked).
struct ClassifierRule
{
    uint16_t ethertype{0};   ///< 0 = match any ethertype
    uint8_t ip_protocol{0};  ///< 0 = match any (only checked for IPv4/IPv6)
    uint16_t udp_port{0};    ///< 0 = match any (only checked for UDP)
    VlanMatch vlan_match{VlanMatch::untagged_only};
    uint16_t vlan_id{0};     ///< VLAN ID for tagged_exact matching
    uint8_t vlan_pcp{0xFF};  ///< 0xFF = match any PCP
    FrameAction action{FrameAction::process};
};

/// Userspace frame classifier.
///
/// Performs linear scan of rules against raw Ethernet frame bytes.
/// Reads ethertype, VLAN tags, IP protocol, and UDP destination port.
/// First matching rule wins; returns default_action if no rule matches.
class TrafficClassifier
{
  public:
    /// @param memory_resource Memory resource for the rules_ vector.
    ///        nullptr → std::pmr::get_default_resource().
    explicit TrafficClassifier(std::pmr::memory_resource* memory_resource = nullptr)
        : rules_{memory_resource != nullptr ? memory_resource : std::pmr::get_default_resource()}
    {}

    /// Replace the rule set and default action.
    /// Rules are stored by value (copied from the span).
    /// @param rules New classifier rules (evaluated in order, first match wins)
    /// @param default_action Action to take when no rule matches
    void set_rules(std::span<ClassifierRule const> rules, FrameAction default_action = FrameAction::forward_tap)
    {
        rules_.assign(rules.begin(), rules.end());
        default_action_ = default_action;
    }

    /// Classify a raw Ethernet frame.
    ///
    /// Returns default_action_ if the frame is shorter than 14 bytes
    /// (cannot read the ethertype field).
    /// Otherwise evaluates rules in order and returns the action of the
    /// first matching rule, or default_action_ if none match.
    /// @param frame Raw Ethernet frame including the 14-byte header
    [[nodiscard]] auto classify(std::span<uint8_t const> frame) const noexcept -> FrameAction;

  private:
    std::pmr::vector<ClassifierRule> rules_;
    FrameAction default_action_{FrameAction::forward_tap};
};

}  // namespace statusbar::net
