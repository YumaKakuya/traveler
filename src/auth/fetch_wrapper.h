// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/fetch.ts (createClaudeSubFetch, L83-112)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-6: custom HTTP wrapper)
//
// Ports the claude-sub fetch wrapper verbatim:
//   - Body JSON injection (injectBillingAndIdentity)
//   - Headers for api.anthropic.com
//   - CC_VERSION, SESSION_ID, BASE_BETAS from fetch.ts
#pragma once

#include "../llm/provider.h"
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <tl/expected.hpp>

namespace traveler::auth {

// ============================================================================
// Constants from fetch.ts
// ============================================================================

// CC_VERSION from fetch.ts L4 — canonical Claude Code version fingerprint.
constexpr const char* CC_VERSION = "2.1.101";

// BASE_BETAS from fetch.ts L8-14.
inline const char* BASE_BETAS[] = {
    "claude-code-20250219",
    "oauth-2025-04-20",
    "interleaved-thinking-2025-05-14",
    "prompt-caching-scope-2026-01-05",
    "context-management-2025-06-27",
};

// ============================================================================
// HttpHeaders / HttpRequest — minimal HTTP abstraction
// ============================================================================
using HttpHeaders = std::map<std::string, std::string>;

struct HttpRequest {
    std::string method{"POST"};
    std::string url;
    HttpHeaders headers;
    std::string body;
};

struct HttpResponse {
    int status{0};
    std::string body;
};

// ============================================================================
// FetchWrapper — claude-sub custom HTTP wrapper
//
// Modifies outgoing requests per fetch.ts L83-112:
//   1. Parses JSON body, applies injectBillingAndIdentity, re-serializes
//   2. Sets Anthropic-specific headers (Authorization, anthropic-version, etc.)
//   3. Deletes x-api-key header
//
// The wrapper takes a get_token callback that returns the current OAuth access
// token. This is called per-request so that token refresh does not require
// wrapping recreation.
// ============================================================================

// Get the current access token. Returns empty if no valid token available.
using TokenProvider = std::function<tl::expected<std::string, llm::Error>()>;

// Create a wrapped HTTP request function that applies claude-sub modifications.
// Returns a function that takes an HttpRequest and returns an HttpResponse.
//
// The returned function:
//   - Modifies the JSON body with billing+identity injection
//   - Sets required Anthropic headers
//   - Calls the underlying HTTP transport
//   - On 401/403, attempts one token-refresh-and-retry
using HttpTransport = std::function<tl::expected<HttpResponse, llm::Error>(const HttpRequest&)>;

// Build the modified request. Does NOT execute HTTP — just returns the
// modified HttpRequest for the caller to execute with their transport.
// This is the testable, pure-function form of the fetch wrapper.
tl::expected<HttpRequest, llm::Error>
prepare_claude_sub_request(const HttpRequest& original,
                           std::string_view access_token);

// Inject billing and identity into a JSON request body string.
// Modifies the body in-place: strips model suffix, adds billing system entry.
// Returns the modified JSON string.
tl::expected<std::string, llm::Error>
inject_billing_and_identity(std::string_view json_body);

// Merge caller's anthropic-beta header with BASE_BETAS.
[[nodiscard]] std::string merge_betas(std::string_view existing_beta);

// Generate a UUID-v4 string for x-client-request-id.
[[nodiscard]] std::string generate_uuid_v4();

// Get or create the session ID (once per process invocation).
[[nodiscard]] const std::string& session_id();

}  // namespace traveler::auth
