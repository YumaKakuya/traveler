#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace traveler::core {

enum class Mode {
    Cockpit,
    Editor,
    LLM,
    Pane,
    Git,
    Split,
};

struct TransitionResult {
    bool ok{false};
    Mode from{Mode::Cockpit};
    Mode to{Mode::Cockpit};
    std::string error;
};

std::string_view mode_name(Mode mode);
std::optional<Mode> mode_from_name(std::string_view name);

class Dispatcher {
public:
    Dispatcher();

    Mode current_mode() const noexcept;
    TransitionResult transition_to(Mode next);
    TransitionResult transition_command(std::string_view slash_command);

    void set_mode_state(Mode mode, std::string state);
    std::string mode_state(Mode mode) const;

private:
    Mode current_{Mode::Cockpit};
    std::unordered_map<std::string, std::string> mode_state_;
};

}  // namespace traveler::core
