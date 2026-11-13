#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// An `RxFlowControl` that keeps a pretend NIC's classifier table instead of
/// a real one.
///
/// Shipped, like RssRecorder, because what it makes testable is a consumer's
/// obligation: that it installs its rules, leaves another owner's alone, and
/// takes its own out again when it goes. Asserting that needs neither
/// CAP_NET_ADMIN nor a NIC with a classifier, which no test host here has.
///
/// It also programs the answers a real NIC gives only at the worst moment:
/// a driver without a classifier, a rule the NIC refuses part way through a
/// plan, a removal that fails.

#include "statusbar/net/net_rx_flow.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

namespace statusbar::net {

class RxFlowRecorder final : public RxFlowControl
{
  public:
    /// What a call was: enough to count and to check the order.
    enum class Op : uint8_t
    {
        ntuple_read,
        ntuple_write,
        rule_read,
        insert,
        remove,
        rx_queues,
    };

    /// One recorded call; `location` is the slot for the rule calls, 0 otherwise.
    struct Call
    {
        Op op;
        uint32_t location;

        [[nodiscard]] auto operator==(Call const&) const noexcept -> bool = default;
    };

    RxFlowRecorder() = default;
    ~RxFlowRecorder() override = default;

    /// How many RX queues the pretend NIC has.
    auto set_rx_queues(uint32_t const queues) noexcept -> void { rx_queues_ = queues; }
    /// The ntuple flag as it stands before anyone touches it.
    auto set_ntuple_flag(bool const enabled) noexcept -> void { ntuple_ = enabled; }
    /// A rule already in the table, left by a previous owner or an administrator.
    auto place(RxFlowRule const& rule) -> void { put(rule); }
    /// A NIC with no classifier: every rule call answers operation_not_supported.
    auto set_unsupported() noexcept -> void { unsupported_ = true; }
    /// Make the insert after `inserts` more succeed fail with `error`, once.
    auto fail_insert_after(size_t const inserts, std::error_code const error) noexcept -> void
    {
        inserts_before_failure_ = inserts;
        insert_error_ = error;
    }
    /// Make the next removal fail with `error`, once, leaving the rule in place.
    auto fail_next_remove(std::error_code const error) noexcept -> void { remove_error_ = error; }
    /// Make the next ntuple write fail with `error`, once, leaving the flag as it was.
    auto fail_next_ntuple_write(std::error_code const error) noexcept -> void { ntuple_error_ = error; }

    /// The ntuple flag as it now stands.
    [[nodiscard]] auto ntuple_flag() const noexcept -> bool { return ntuple_; }
    /// The table as it now stands, in insertion order.
    [[nodiscard]] auto rules() const noexcept -> std::span<RxFlowRule const> { return rules_; }
    /// Every call made, in order.
    [[nodiscard]] auto calls() const noexcept -> std::span<Call const> { return calls_; }
    /// How many calls of one kind were made.
    [[nodiscard]] auto count(Op const op) const noexcept -> size_t
    {
        return static_cast<size_t>(std::ranges::count(calls_, op, &Call::op));
    }

    [[nodiscard]] auto ntuple(std::string_view) -> StatusValue<bool> override
    {
        calls_.push_back({Op::ntuple_read, 0});
        return ntuple_;
    }

    [[nodiscard]] auto set_ntuple(std::string_view, bool const enabled) -> Status override
    {
        calls_.push_back({Op::ntuple_write, 0});
        if (auto const error = take(ntuple_error_); error) {
            return failure(*error);
        }
        ntuple_ = enabled;
        if (!enabled) {
            rules_.clear();  // as igb and igc do when the filters go off
        }
        return success();
    }

    [[nodiscard]] auto rule(std::string_view, uint32_t const location) -> StatusValue<std::optional<RxFlowRule>> override
    {
        calls_.push_back({Op::rule_read, location});
        if (unsupported_) {
            return failure(std::make_error_code(std::errc::operation_not_supported));
        }
        auto const found = find(location);
        if (found == rules_.end()) {
            return std::optional<RxFlowRule>{};
        }
        return std::optional{*found};
    }

    [[nodiscard]] auto insert(std::string_view, RxFlowRule const& rule) -> Status override
    {
        calls_.push_back({Op::insert, rule.location});
        if (unsupported_) {
            return failure(std::make_error_code(std::errc::operation_not_supported));
        }
        if (insert_fails_now()) {
            return failure(insert_error_);
        }
        put(rule);
        return success();
    }

    [[nodiscard]] auto remove(std::string_view, uint32_t const location) -> Status override
    {
        calls_.push_back({Op::remove, location});
        if (auto const error = take(remove_error_); error) {
            return failure(*error);
        }
        auto const found = find(location);
        if (found == rules_.end()) {
            return failure(std::make_error_code(std::errc::invalid_argument));
        }
        rules_.erase(found);
        return success();
    }

    [[nodiscard]] auto rx_queues(std::string_view) -> StatusValue<uint32_t> override
    {
        calls_.push_back({Op::rx_queues, 0});
        return rx_queues_;
    }

  private:
    /// Consume a programmed one-shot error, if one is armed.
    static auto take(std::optional<std::error_code>& slot) -> std::optional<std::error_code>
    {
        auto error = slot;
        slot.reset();
        return error;
    }

    /// Whether this insert is the one programmed to fail, counting it down.
    auto insert_fails_now() noexcept -> bool
    {
        if (!inserts_before_failure_.has_value()) {
            return false;
        }
        if (*inserts_before_failure_ > 0) {
            --*inserts_before_failure_;
            return false;
        }
        inserts_before_failure_.reset();
        return true;
    }

    auto find(uint32_t const location) -> std::vector<RxFlowRule>::iterator
    {
        return std::ranges::find(rules_, location, &RxFlowRule::location);
    }

    /// `rule` into its slot, replacing what is there, as SRXCLSRLINS does.
    auto put(RxFlowRule const& rule) -> void
    {
        if (auto const found = find(rule.location); found != rules_.end()) {
            *found = rule;
            return;
        }
        rules_.push_back(rule);
    }

    std::vector<RxFlowRule> rules_;
    std::vector<Call> calls_;
    std::optional<size_t> inserts_before_failure_;
    std::error_code insert_error_;
    std::optional<std::error_code> remove_error_;
    std::optional<std::error_code> ntuple_error_;
    uint32_t rx_queues_{4};
    bool ntuple_{false};
    bool unsupported_{false};
};

}  // namespace statusbar::net
