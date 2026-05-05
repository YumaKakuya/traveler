// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/token.ts (readCredentialsData, writeBackCredentials)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-3: credentials.json mode 0600)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-5: TB-052 separation, ~/.config/traveler/)
#pragma once

#include "oauth.h"
#include <optional>
#include <string>
#include <string_view>
#include <tl/expected.hpp>

namespace traveler::auth {

// ============================================================================
// OAuthCredentials — stored token state per provider
// ============================================================================
struct OAuthCredentials {
    std::string access_token;
    std::string refresh_token;
    int64_t expires_at{0};  // unix epoch seconds

    [[nodiscard]] bool expired() const noexcept;
};

// ============================================================================
// CredentialsFile — read/write ~/.config/traveler/credentials.json
//
// File mode: 0600 (owner read/write only).
// Schema diverges intentionally from Hatch. (uses "anthropic" key, not claudeAiOauth).
// ============================================================================

// Read credentials for a provider. Returns nullopt if file or key does not exist.
tl::expected<std::optional<OAuthCredentials>, Error>
read_credentials(std::string_view provider);

// Write credentials for a provider. Creates file/directory if not present.
tl::expected<void, Error>
write_credentials(std::string_view provider, const OAuthCredentials& creds);

// Delete credentials for a provider (logout).
tl::expected<void, Error>
delete_credentials(std::string_view provider);

// Get the path to the credentials file (for diagnostics only — never log tokens).
[[nodiscard]] std::string credentials_file_path();

}  // namespace traveler::auth
