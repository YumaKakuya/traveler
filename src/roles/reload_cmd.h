// GATE-P0-4 T3: /roles-reload command + Command Palette entry
// Reference: Traveler_Phase0_Spec_v0.1.md §8.1 (REQ-ROLES-4)
// Reference: ~/hatch-v3/packages/opencode/src/agent/roles.ts (Bus reload pattern)
//
// Header-only for Phase 0 testability. When the Command Palette /
// slash-command dispatcher is completed (GATE-P0-2 + GATE-P0-3), the
// command registration and palette entry logic moves to reload_cmd.cpp.
#pragma once

#include "roles/registry.h"
#include <string>

namespace traveler::roles {

// ============================================================================
// ReloadResult — outcome of /roles-reload command execution.
//
// ok:       true if reload completed without errors
// message:  human-readable result or error description
// count:    number of roles loaded after reload (0 on error)
// ============================================================================
struct ReloadResult {
    bool ok{false};
    std::string message;
    size_t count{0};
};

// ============================================================================
// reload_roles_cmd — execute /roles-reload: reload RoleRegistry from the
// originally configured source path.
//
// Phase 0 manual reload only (REQ-ROLES-4). No inotify-based auto-reload.
//
// Returns:
//   - ok=true,  count>0  → reload succeeded with N roles
//   - ok=true,  count=0  → reload succeeded but file contains 0 valid roles
//   - ok=false           → reload failed (no source file yet, or parse error)
//
// Satisfies PC-3:
//   "/roles-reload after editing roles.md updates RoleRegistry;
//    subsequent @-mention uses the new mapping"
// ============================================================================
inline ReloadResult reload_roles_cmd(RoleRegistry& registry) {
    const auto& src = registry.source_path();
    if (src.empty()) {
        return {false, "No roles file has been loaded. Use /roles-load <path> first.", 0};
    }

    registry.reload();

    if (!registry.last_error().empty()) {
        return {false, registry.last_error(), registry.size()};
    }

    return {true, "", registry.size()};
}

}  // namespace traveler::roles
