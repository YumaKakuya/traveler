// Reference: Traveler_Phase0_Spec_v0.1.md §7.6 (REQ-SESSION-4: error handling)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.2 (REQ-ANTHROPIC-4: three-fallback paths)
#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace traveler::llm {

// REQ-ANTHROPIC-4 three-fallback paths (R1-CPO-003 fallback discoverability)
// Must list all three: re-authenticate, API-key, offline mode
inline constexpr const char* kAuthFallbackText =
    "Run 'traveler providers login anthropic' to re-authenticate. "
    "Or use API-key auth: set ANTHROPIC_API_KEY env or run "
    "'traveler providers login anthropic --api-key'. "
    "Or switch to offline mode with /offline-mode "
    "(no network required after first-run model download).";

// Return the REQ-ANTHROPIC-4 three-fallback authentication error text.
// Used when OAuth token expires and refresh fails.
inline std::string auth_fallback_message() {
    return std::string("OAuth authentication failed.\n\n") + kAuthFallbackText;
}

// Render a provider error message suitable for display in Stage.
// Preserves partial conversation context per REQ-SESSION-4.
// For auth errors, includes the REQ-ANTHROPIC-4 three-fallback paths.
inline std::string render_provider_error(std::string_view kind,
                                          std::string_view detail) {
    std::ostringstream oss;
    oss << kind << " error: " << detail;

    if (kind == "auth") {
        oss << "\n\n" << kAuthFallbackText;
    } else if (kind == "network") {
        oss << "\n\nCheck your internet connection and try again.";
    } else {
        oss << "\n\nThe LLM provider returned an unexpected error. "
            << "Check your credentials and try again later.";
    }
    return oss.str();
}

}  // namespace traveler::llm
