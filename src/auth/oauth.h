// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/index.ts (generatePKCE, authorize, exchangeCodeForTokens)
// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/token.ts (refreshAccessToken, refreshInternal)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-1, REQ-OAUTH-2)
#pragma once

#include "../llm/provider.h"
#include <string>
#include <tl/expected.hpp>

namespace traveler::auth {

// ============================================================================
// OAuth constants (verbatim from Hatch. Gen 1)
// ============================================================================

// Client ID shared with Claude Code (TB-012 CLOSED).
constexpr const char* OAUTH_CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";
constexpr const char* OAUTH_AUTHORIZE_URL = "https://claude.ai/oauth/authorize";
constexpr const char* OAUTH_TOKEN_URL = "https://claude.ai/v1/oauth/token";
constexpr int OAUTH_PORT = 1456;
constexpr const char* OAUTH_SCOPES =
    "user:file_upload user:inference user:mcp_servers user:profile user:sessions:claude_code";
constexpr const char* OAUTH_REDIRECT_PATH = "/callback";

// ============================================================================
// PkceCodes — PKCE challenge/verifier pair
// ============================================================================
struct PkceCodes {
    std::string verifier;   // 43-character random string
    std::string challenge;  // base64url(SHA-256(verifier))
};

// Generate a PKCE code pair: random 43-char verifier, SHA-256 → base64url challenge.
// Faithful to Hatch. generatePKCE() in index.ts.
tl::expected<PkceCodes, llm::Error> generate_pkce();

// Generate a random state value for CSRF protection.
// Faithful to Hatch. generateState() in index.ts.
tl::expected<std::string, llm::Error> generate_state();

// ============================================================================
// TokenResponse — OAuth token endpoint response
// ============================================================================
struct TokenResponse {
    std::string access_token;
    std::string refresh_token;   // may be empty if server doesn't rotate
    int64_t expires_in{36000};   // default 10 hours
};

// Exchange authorization code for tokens (POST to /v1/oauth/token).
tl::expected<TokenResponse, llm::Error>
exchange_code_for_tokens(std::string_view code,
                         std::string_view redirect_uri,
                         std::string_view code_verifier);

// Refresh access token using a refresh token.
// Uses grant_type=refresh_token against the Anthropic OAuth token endpoint.
tl::expected<TokenResponse, llm::Error>
refresh_access_token(std::string_view refresh_token);

// Build the full authorization URL with all parameters.
[[nodiscard]] std::string
build_authorization_url(std::string_view challenge,
                        std::string_view state,
                        std::string_view redirect_uri);

}  // namespace traveler::auth
