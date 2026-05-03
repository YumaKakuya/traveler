#include "command/parser.h"

#include <sstream>
#include <utility>

namespace traveler::command {
namespace {

const std::vector<std::string> kRoster{"@vega", "@altair", "@orion", "@rigel"};

const std::vector<LeaderBinding> kLeaderBindings{
    {true, 'c', "Cockpit", "Cockpit mode", {}},
    {true, 'e', "Editor", "Editor mode", {}},
    {true, 'l', "LLM", "LLM mode", {}},
    {true, 'p', "Pane", "Pane mode", {}},
    {true, 'g', "Git", "Git mode", {}},
    {true, 's', "Split", "Horizontal split", {}},
};

SlashParseResult slash_error(std::string error) {
    SlashParseResult result;
    result.error = std::move(error);
    return result;
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string mention_fragment(std::string_view input) {
    const auto at = input.rfind('@');
    if (at == std::string_view::npos) {
        return {};
    }

    std::string fragment;
    for (std::size_t i = at; i < input.size(); ++i) {
        const char c = input[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            break;
        }
        fragment.push_back(c);
    }
    return fragment;
}

}  // namespace

SlashParseResult parse_slash_command(std::string_view input) {
    if (input.empty() || input.front() != '/') {
        return slash_error("slash command must start with '/'");
    }

    input.remove_prefix(1);
    while (!input.empty() && input.front() == ' ') {
        input.remove_prefix(1);
    }
    if (input.empty()) {
        return slash_error("slash command name is empty");
    }

    std::stringstream stream{std::string(input)};
    SlashParseResult result;
    result.ok = true;
    stream >> result.command.name;

    std::string arg;
    while (stream >> arg) {
        result.command.args.push_back(arg);
    }

    if (result.command.name.empty()) {
        return slash_error("slash command name is empty");
    }
    return result;
}

LeaderBinding parse_leader_key(char pressed_key) {
    for (const auto& binding : kLeaderBindings) {
        if (binding.key == pressed_key) {
            return binding;
        }
    }

    LeaderBinding result;
    result.key = pressed_key;
    result.error = "unknown leader binding";
    return result;
}

std::vector<std::string> complete_callsign(std::string_view tower_input) {
    const std::string fragment = mention_fragment(tower_input);
    if (fragment.empty()) {
        return {};
    }

    std::vector<std::string> ranked;
    for (const auto& callsign : kRoster) {
        if (starts_with(callsign, fragment)) {
            ranked.push_back(callsign);
        }
    }
    for (const auto& callsign : kRoster) {
        if (!starts_with(callsign, fragment)) {
            ranked.push_back(callsign);
        }
    }
    return ranked;
}

const std::vector<std::string>& callsign_roster() {
    return kRoster;
}

const std::vector<LeaderBinding>& leader_bindings() {
    return kLeaderBindings;
}

}  // namespace traveler::command

