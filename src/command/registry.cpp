#include "command/registry.h"

#include <algorithm>

namespace traveler::command {

void ActionRegistry::register_action(Action action) {
    // Silently ignore duplicates.
    auto found = std::find_if(actions_.begin(), actions_.end(),
                              [&](const Action& a) { return a.id == action.id; });
    if (found != actions_.end()) {
        return;
    }
    actions_.push_back(std::move(action));
}

void ActionRegistry::populate_defaults() {
    register_action({"open-cockpit-mode", "Open Cockpit mode", ""});
    register_action({"open-editor-mode", "Open Editor mode", ""});
    register_action({"open-llm-mode", "Open LLM mode", ""});
    register_action({"open-pane-mode", "Open Pane mode", ""});
    register_action({"open-git-mode", "Open Git mode", ""});
    register_action({"split-horizontal", "Split horizontal", ""});
    register_action({"split-vertical", "Split vertical", ""});
    register_action({"modal-save", "Save file (:w)", ""});
    register_action({"modal-quit", "Quit (:q)", ""});
    register_action({"modal-write-quit", "Write and quit (:wq)", ""});
    register_action({"modal-open", "Open file (:e)", ""});
}

}  // namespace traveler::command
