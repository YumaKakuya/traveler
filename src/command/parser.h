#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace traveler::command {

struct SlashCommand {
    std::string name;
    std::vector<std::string> args;
};

struct SlashParseResult {
    bool ok{false};
    SlashCommand command;
    std::string error;
};

struct LeaderBinding {
    bool ok{false};
    char key{0};
    std::string command;
    std::string display_name;
    std::string error;
};

SlashParseResult parse_slash_command(std::string_view input);
LeaderBinding parse_leader_key(char pressed_key);
std::vector<std::string> complete_callsign(std::string_view tower_input);
const std::vector<std::string>& callsign_roster();
const std::vector<LeaderBinding>& leader_bindings();

}  // namespace traveler::command
