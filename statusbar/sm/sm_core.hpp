#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/sm/sm_base.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <utility>

namespace statusbar::sm {

//
// Time Types
//

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

//
// Transition Structure
//

/// A transition entry: next state + optional action function pointer
/// Action functions receive context and event_time
template <typename Context, typename StateEnum>
struct Transition
{
    using ActionFn = void (*)(Context&, TimePoint);

    StateEnum next_state{};
    ActionFn action{nullptr};
    std::string_view action_name{};
    bool valid{false};  // Explicit validity flag - false means no transition defined

    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool { return valid; }
};

/// Helper to create a transition with automatic action name extraction
template <typename Context, typename StateEnum, void (*ActionFn)(Context&, TimePoint)>
constexpr auto make_transition(StateEnum next_state) -> Transition<Context, StateEnum>
{
    return Transition<Context, StateEnum>{
        .next_state = next_state,
        .action = ActionFn,
        .action_name = function_name<ActionFn>,
        .valid = true,
    };
}

//
// Transitions Helper
//

/// Helper struct to create transitions for a specific definition type
/// Usage: using T = sm::Transitions<MyDef>;
///        t.at(...) = T::action<my_func>(State::Next);
///        t.at(...) = T::transition(State::Next);
template <typename Def>
struct Transitions
{
    using Context = Def::Context;
    using State = Def::State;

    /// Create a transition with an action
    /// Action functions receive (Context&, TimePoint)
    /// @param next_state State to transition to after executing the action
    template <void (*ActionFn)(Context&, TimePoint)>
    static constexpr auto action(State next_state) -> Transition<Context, State>
    {
        return Transition<Context, State>{
            .next_state = next_state,
            .action = ActionFn,
            .action_name = function_name<ActionFn>,
            .valid = true,
        };
    }

    /// Create a transition without an action
    /// @param next_state State to transition to
    static constexpr auto transition(State next_state) -> Transition<Context, State>
    {
        return Transition<Context, State>{
            .next_state = next_state,
            .action = nullptr,
            .action_name = {},
            .valid = true,
        };
    }
};

//
// Transition Table
//

/// Transition table: 2D array of transitions indexed by [state][event]
/// Takes a Def struct that provides Context, State, Event
template <typename Def>
struct TransitionTable
{
    using Context = Def::Context;
    using State = Def::State;
    using Event = Def::Event;
    using TransitionType = Transition<Context, State>;

    static constexpr size_t num_states = enum_count<State>::value;
    static constexpr size_t num_events = enum_count<Event>::value;

    std::array<std::array<TransitionType, num_events>, num_states> data{};

    constexpr auto at(State state, Event event) noexcept -> TransitionType&
    {
        return data[static_cast<size_t>(state)][static_cast<size_t>(event)];
    }

    [[nodiscard]] constexpr auto get(State state, Event event) const noexcept -> TransitionType const&
    {
        return data[static_cast<size_t>(state)][static_cast<size_t>(event)];
    }
};

//
// Transition Observers
//

/// Default no-op observer - zero overhead when not observing
struct NullObserver
{
    template <typename State, typename Event>
    constexpr void operator()(
        State /*old_state*/, Event /*event*/, std::string_view /*action_name*/, State /*new_state*/) const noexcept
    {}
};

//
// State Machine
//

/// Standalone state machine - context is a separate struct passed by reference
/// Template parameters:
///   - Def: a struct providing Context, State, Event
///   - Table: the transition table (constexpr reference)
///   - Observer: callable with signature (State old, Event event, string_view action, State new)
template <typename Def, auto const& Table, typename Observer = NullObserver>
class StateMachine
{
  public:
    using Context = Def::Context;
    using State = Def::State;
    using Event = Def::Event;

    // Expose definition type name for DOT generation
    static constexpr std::string_view name = type_name<Def>;

    // Expose table and counts for generation utilities
    static constexpr auto const& table = Table;
    static constexpr size_t num_states = enum_count<State>::value;
    static constexpr size_t num_events = enum_count<Event>::value;
    static constexpr bool has_uct_event = HasUct<Event>;

    // Name lookup using EnumNamesFor
    using StateNames = EnumNamesFor<State, State::Count>;
    using EventNames = EnumNamesFor<Event, Event::Count>;
    static constexpr auto state_name = StateNames::get;
    static constexpr auto event_name = EventNames::get;

    constexpr explicit StateMachine(Observer obs = Observer{}) noexcept
        : observer_{obs}
    {}

    /// Handle an event, performing UCT chain before and after (if UCT is supported)
    /// event_time defaults to current time if not specified
    /// @param ctx Mutable reference to the state machine context
    /// @param event The event to process
    /// @param event_time Timestamp for the event (defaults to now)
    void handle_event(Context& ctx, Event event, TimePoint event_time = Clock::now())
    {
        if constexpr (has_uct_event) {
            process_uct_chain(ctx, event_time);
        }
        process_one_event(ctx, event, event_time);
        if constexpr (has_uct_event) {
            process_uct_chain(ctx, event_time);
        }
    }

    /// Get current state
    [[nodiscard]] constexpr auto current_state() const noexcept -> State { return state_; }

    /// Reset to initial state
    constexpr void reset() noexcept { state_ = State{}; }

  private:
    void process_one_event(Context& ctx, Event event, TimePoint event_time)
    {
        auto const& transition = Table.get(state_, event);
        if (!transition.is_valid()) {
            return;
        }
        State const old_state = state_;
        if (transition.action != nullptr) {
            transition.action(ctx, event_time);
        }
        state_ = transition.next_state;
        // Notify observer when the transition did something: either the state
        // changed, or an action ran (self-transitions with an action still
        // notify). Pure no-op self-loops (same state, no action) stay silent.
        if (old_state != state_ || transition.action != nullptr) {
            observer_(old_state, event, transition.action_name, state_);
        }
    }

    void process_uct_chain(Context& ctx, TimePoint event_time)
        requires has_uct_event
    {
        constexpr Event uct_event = enum_uct<Event>::value;
        bool changed = false;
        do {
            auto old = state_;
            process_one_event(ctx, uct_event, event_time);
            changed = (old != state_);
        } while (changed);
    }

    State state_{};
    [[no_unique_address]] Observer observer_{};
};

}  // namespace statusbar::sm
