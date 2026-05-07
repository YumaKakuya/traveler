#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace traveler::command {

/// A registered action for the Command Palette.
///
/// `id` is the stable machine-readable identifier (e.g. "open-editor-mode").
/// `display_name` is the human-readable label shown in the palette
///   (e.g. "Open Editor mode").
/// `mode_filter` restricts visibility; empty string = visible in all modes.
struct Action {
    std::string id;
    std::string display_name;
    std::string mode_filter;
};

/// Singleton registry of all Command Palette actions.
///
/// Modes register their actions at startup (REQ-CMDPAL-2).
/// The registry is consumed by `CommandPalette` for fuzzy search.
class ActionRegistry {
public:
    /// Register an action. Duplicate `id` values are silently ignored.
    void register_action(Action action);

    /// Return every registered action (no filtering, no ranking).
    const std::vector<Action>& actions() const noexcept { return actions_; }

    /// Populate the Phase 0 default action set:
    ///   - Open Cockpit mode
    ///   - Open Editor mode
    ///   - Open LLM mode
    ///   - Open Pane mode
    ///   - Open Git mode
    ///   - Split horizontal
    ///   - Split vertical
    ///   - Save file (:w)
    ///   - Quit (:q)
    ///   - Write and quit (:wq)
    ///   - Open file (:e)
    void populate_defaults();

private:
    std::vector<Action> actions_;
};

}  // namespace traveler::command
