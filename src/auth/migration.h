// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-5: TB-052 migration)
// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/token.ts (ensureMigration)
//
// TB-052 mitigation: if Traveler. and Hatch. Gen 1 coexist, they MUST NOT share
// credentials.json. Migration uses Hatch.'s refresh_token to obtain an
// independent pair for Traveler. If the same refresh_token is returned
// (no rotation), fall back with warning.
#pragma once

#include "oauth.h"
#include <string>
#include <tl/expected.hpp>

namespace traveler::auth {

// ============================================================================
// MigrationStatus — result of attempting TB-052 migration
// ============================================================================
enum class MigrationStatus {
    not_needed,            // No Hatch. credentials found
    success_independent,   // Migrated with new independent refresh token
    success_shared,        // Migrated but same refresh token returned
    skipped,               // User chose not to migrate
    failed,                // Migration failed
};

struct MigrationResult {
    MigrationStatus status{MigrationStatus::not_needed};
    std::string message;   // user-facing message for display
};

// Check if Hatch. credentials exist at ~/.config/hatch/credentials.json.
[[nodiscard]] bool detect_hatch_credentials();

// Check if Traveler. credentials already exist.
[[nodiscard]] bool traveler_credentials_exist();

// Perform TB-052 migration: read Hatch. credentials, use refresh_token to
// obtain independent pair, persist to ~/.config/traveler/credentials.json.
// Does NOT modify or delete Hatch. credentials.
//
// If the OAuth backend returns the same refresh_token (no rotation), falls
// back to copy-as-is with a user warning per Spec §7.5 fallback case.
tl::expected<MigrationResult, Error>
migrate_hatch_credentials();

}  // namespace traveler::auth
