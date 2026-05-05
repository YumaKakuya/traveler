// Reference: ~/hatch-v3/packages/opencode/src/agent/roles.ts:10-19 (ParsedRole type)
// Reference: Traveler_Phase0_Spec_v0.1.md §8.1 (RoleEntry struct)
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace traveler::roles {

// ============================================================================
// Error type for tl::expected<T, Error> (Spec §11.7 / T-8)
// ============================================================================
struct Error {
    std::string message;

    [[nodiscard]] static Error Parse(std::string msg) {
        return {std::move(msg)};
    }
    [[nodiscard]] static Error IO(std::string msg) {
        return {std::move(msg)};
    }
    [[nodiscard]] static Error Validation(std::string msg) {
        return {std::move(msg)};
    }
};

// ============================================================================
// ParsedRole — intermediate parser output (faithful to roles.ts L10-19)
// ============================================================================
struct ParsedRole {
    std::string model;              // "provider/model" (validated: must contain '/')
    std::string variant;            // opus | sonnet | haiku | etc.
    std::string mode;               // "subagent" | "primary" | "all" | ""
    double temperature{-1};         // -1 = not set
    double top_p{-1};               // -1 = not set
    std::string description;
    bool hidden{false};
    int steps{-1};                  // -1 = not set
    std::string prompt;             // body H2 section content (system_prompt)
};

// ============================================================================
// RoleEntry — canonical registry entry (Spec §8.1 struct)
// ============================================================================
struct RoleEntry {
    std::string callsign;           // "@vega", "@altair", "@orion", "@rigel"
    std::string model;              // vendor+model, e.g., "anthropic/claude-opus-4-7"
    std::string tier;               // "opus" | "sonnet" | "haiku" | "gpt-5.4" etc.
    std::string system_prompt;      // role-specific system prompt from roles.md body
};

}  // namespace traveler::roles
