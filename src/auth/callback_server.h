// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/index.ts (startOAuthServer, waitForOAuthCallback)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-4: local callback server on 127.0.0.1:1456)
#pragma once

#include "../llm/provider.h"
#include <chrono>
#include <string>
#include <string_view>
#include <tl/expected.hpp>

namespace traveler::auth {

// ============================================================================
// CallbackResult — result of the OAuth callback
// ============================================================================
enum class CallbackStatus {
    success,
    error,
    timeout,
    cancelled,
};

struct CallbackResult {
    CallbackStatus status{CallbackStatus::error};
    std::string authorization_code;  // valid when status == success
    std::string error_message;       // valid when status != success
};

// ============================================================================
// OAuthCallbackServer — local HTTP server for OAuth redirect
//
// Binds to 127.0.0.1:1456. Serves /callback to receive the OAuth authorization
// code. Resolves via a callback or promise when the code arrives.
//
// Uses cpp-httplib internally. Single-use: after one callback the server stops.
// ============================================================================

// Start the OAuth callback server on port 1456.
// Returns the redirect URI (http://localhost:1456/callback).
// The server runs on a background thread.
tl::expected<std::string, llm::Error> start_oauth_server();

// Wait for the OAuth callback to arrive. Blocks for up to timeout (default 5 min).
// Returns the authorization code on success, or error description on failure.
// Must be called after start_oauth_server() and before stop_oauth_server().
tl::expected<CallbackResult, llm::Error>
wait_for_oauth_callback(std::string_view expected_state,
                        std::chrono::seconds timeout = std::chrono::seconds{300});

// Stop the OAuth callback server. Safe to call even if not started.
void stop_oauth_server();

}  // namespace traveler::auth
