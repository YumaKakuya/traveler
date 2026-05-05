// Reference: Traveler_Phase0_Spec_v0.1.md §8.1 (RoleRegistry)
// Reference: ~/hatch-v3/packages/opencode/src/agent/roles.ts:43-45 (lookup pattern)
#include "registry.h"
#include "parser.h"

namespace traveler::roles {

void RoleRegistry::load(const std::filesystem::path& roles_md) {
    source_path_ = roles_md;
    last_error_.clear();
    entries_.clear();

    auto result = parseRoles(source_path_.parent_path().string());
    if (!result.has_value()) {
        last_error_ = result.error().message;
        return;
    }

    const auto& parsed_map = result.value();
    for (const auto& [name, parsed] : parsed_map) {
        RoleEntry entry;
        // Callsign: prepend '@' if not already present
        entry.callsign = "@" + name;
        entry.model = parsed.model;
        // Tier: derive from variant or model
        if (!parsed.variant.empty()) {
            entry.tier = parsed.variant;
        } else {
            // Extract tier from model name (e.g., "anthropic/claude-opus-4-7" → "opus")
            auto slash = parsed.model.find('/');
            if (slash != std::string::npos) {
                std::string model_part = parsed.model.substr(slash + 1);
                // Simple tier extraction: look for known tier keywords
                if (model_part.find("opus") != std::string::npos) entry.tier = "opus";
                else if (model_part.find("sonnet") != std::string::npos) entry.tier = "sonnet";
                else if (model_part.find("haiku") != std::string::npos) entry.tier = "haiku";
                else if (model_part.find("gemini") != std::string::npos) entry.tier = "gemini";
                else if (model_part.find("gpt") != std::string::npos) entry.tier = "gpt";
                else entry.tier = "default";
            }
        }
        entry.system_prompt = parsed.prompt;
        entries_[name] = std::move(entry);
    }
}

std::optional<RoleEntry> RoleRegistry::lookup(std::string_view callsign) const {
    std::string name(callsign);
    // Strip leading '@' if present
    if (!name.empty() && name[0] == '@') {
        name = name.substr(1);
    }
    auto it = entries_.find(name);
    if (it != entries_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> RoleRegistry::all_callsigns() const {
    std::vector<std::string> result;
    result.reserve(entries_.size());
    for (const auto& [name, entry] : entries_) {
        result.push_back(entry.callsign);
    }
    return result;
}

void RoleRegistry::reload() {
    if (!source_path_.empty()) {
        load(source_path_);
    }
}

}  // namespace traveler::roles
