// Reference: Traveler_Phase0_Spec_v0.1.md §8.1 (REQ-ROLES-2: RoleRegistry class)
// Reference: ~/hatch-v3/packages/opencode/src/agent/roles.ts:43-45 (lookup pattern)
#pragma once

#include "types.h"
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace traveler::roles {

// ============================================================================
// RoleRegistry — in-memory registry mapping callsigns to RoleEntry
//
// Wraps the parsed roles.md content and provides fast lookup.
// Phase 0: manual reload only via /roles-reload command (REQ-ROLES-4).
// ============================================================================
class RoleRegistry {
public:
    RoleRegistry() = default;

    // Load roles from a roles.md file. If parsing fails, registry is left empty
    // and the error is available via last_error() (faithful to roles.ts behavior).
    void load(const std::filesystem::path& roles_md);

    // Look up a role by callsign. Callsign may include or omit the '@' prefix.
    // Returns std::nullopt if not found.
    [[nodiscard]] std::optional<RoleEntry> lookup(std::string_view callsign) const;

    // Return all known callsigns (without '@' prefix).
    [[nodiscard]] std::vector<std::string> all_callsigns() const;

    // Number of loaded roles
    [[nodiscard]] size_t size() const { return entries_.size(); }

    // Check if registry is empty (no file loaded or parse failed)
    [[nodiscard]] bool empty() const { return entries_.empty(); }

    // Last parse error message (empty string on success)
    [[nodiscard]] const std::string& last_error() const { return last_error_; }

    // Reload from the same path
    void reload();

private:
    std::unordered_map<std::string, RoleEntry> entries_;
    std::filesystem::path source_path_;
    std::string last_error_;
};

}  // namespace traveler::roles
