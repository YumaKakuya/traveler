#include "core/dispatcher.h"

#include <array>
#include <utility>

namespace traveler::core {
namespace {

constexpr std::array<std::pair<Mode, std::string_view>, 6> kModes{{
    {Mode::Cockpit, "Cockpit"},
    {Mode::Editor, "Editor"},
    {Mode::LLM, "LLM"},
    {Mode::Pane, "Pane"},
    {Mode::Git, "Git"},
    {Mode::Split, "Split"},
}};

std::string mode_key(Mode mode) {
    return std::string(mode_name(mode));
}

TransitionResult success(Mode from, Mode to) {
    return TransitionResult{true, from, to, {}};
}

TransitionResult failure(Mode from, std::string error) {
    return TransitionResult{false, from, from, std::move(error)};
}

std::string_view strip_slash(std::string_view command) {
    if (!command.empty() && command.front() == '/') {
        command.remove_prefix(1);
    }
    return command;
}

std::string_view trim_command(std::string_view command) {
    while (!command.empty() && command.front() == ' ') {
        command.remove_prefix(1);
    }
    while (!command.empty() && command.back() == ' ') {
        command.remove_suffix(1);
    }
    return command;
}

}  // namespace

std::string_view mode_name(Mode mode) {
    for (const auto& entry : kModes) {
        if (entry.first == mode) {
            return entry.second;
        }
    }
    return "Cockpit";
}

std::optional<Mode> mode_from_name(std::string_view name) {
    for (const auto& entry : kModes) {
        if (entry.second == name) {
            return entry.first;
        }
    }
    return std::nullopt;
}

Dispatcher::Dispatcher() {
    for (const auto& entry : kModes) {
        mode_state_.emplace(std::string(entry.second), std::string{});
    }
}

Mode Dispatcher::current_mode() const noexcept {
    return current_;
}

TransitionResult Dispatcher::transition_to(Mode next) {
    const Mode from = current_;
    current_ = next;
    return success(from, next);
}

TransitionResult Dispatcher::transition_command(std::string_view slash_command) {
    const Mode from = current_;
    std::string_view command = trim_command(strip_slash(slash_command));
    if (command.empty()) {
        return failure(from, "empty mode command");
    }

    auto next = mode_from_name(command);
    if (!next.has_value()) {
        return failure(from, "unknown mode command");
    }
    return transition_to(*next);
}

void Dispatcher::set_mode_state(Mode mode, std::string state) {
    mode_state_[mode_key(mode)] = std::move(state);
}

std::string Dispatcher::mode_state(Mode mode) const {
    const auto found = mode_state_.find(mode_key(mode));
    if (found == mode_state_.end()) {
        return {};
    }
    return found->second;
}

}  // namespace traveler::core

