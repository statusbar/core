#pragma once

// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// Reusable test-support utilities for state-machine tests.
//
// These observers attach to a StateMachine to inspect transitions without the
// SM under test needing any bespoke instrumentation:
//   - RecordingObserver: records the full sequence of transitions.
//   - ActionRecorder:    records the name of the most recent action that ran.
//   - Observed:          bundles a StateMachine with an ActionRecorder and the
//                        storage it writes to, for the common "which action
//                        just ran" assertion.

#include "statusbar/sm/sm_core.hpp"

#include <string_view>
#include <utility>
#include <vector>

namespace statusbar::sm::test {

/// Observer that records every transition it is notified of into an external
/// vector. Useful for asserting an exact transition sequence.
template <typename Def>
struct RecordingObserver
{
    using State = typename Def::State;
    using Event = typename Def::Event;

    struct Transition
    {
        State old_state;
        Event event;
        std::string_view action_name;
        State new_state;
    };

    std::vector<Transition>* transitions{nullptr};

    void operator()(State old_state, Event event, std::string_view action_name, State new_state) const
    {
        if (transitions != nullptr) {
            transitions->push_back({old_state, event, action_name, new_state});
        }
    }
};

/// Observer that captures the name of the most recently executed action.
///
/// Only action-bearing transitions update it (no-action transitions carry an
/// empty action name and are ignored), so it reflects the last action that
/// actually ran. Action-bearing self-transitions are included, since the
/// framework notifies the observer whenever an action runs even when the state
/// does not change.
template <typename Def>
struct ActionRecorder
{
    using State = typename Def::State;
    using Event = typename Def::Event;

    std::string_view* last_action{nullptr};

    void operator()(State /*old_state*/, Event /*event*/, std::string_view action_name, State /*new_state*/) const
    {
        if (last_action != nullptr && !action_name.empty()) {
            *last_action = action_name;
        }
    }
};

/// A StateMachine bundled with an ActionRecorder and the storage it writes to.
///
/// Tests read `.last_action` to see which action last ran, and may reset it
/// (e.g. `machine.last_action = {}`) before exercising events that should run
/// no action. `handle_event`/`current_state`/`reset` forward to the underlying
/// machine.
template <typename Def, auto const& Table>
struct Observed
{
    using State = typename Def::State;
    using Event = typename Def::Event;

    std::string_view last_action{};
    StateMachine<Def, Table, ActionRecorder<Def>> machine{ActionRecorder<Def>{&last_action}};

    Observed() = default;
    Observed(Observed const&) = delete;
    auto operator=(Observed const&) -> Observed& = delete;

    template <typename... Args>
    void handle_event(Args&&... args)
    {
        machine.handle_event(std::forward<Args>(args)...);
    }

    [[nodiscard]] auto current_state() const noexcept -> State { return machine.current_state(); }

    void reset() noexcept { machine.reset(); }
};

}  // namespace statusbar::sm::test
