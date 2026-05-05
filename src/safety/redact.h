// Reference: ~/hatch-v3/packages/hatch-safety/src/collector/anonymizer.ts:152-154
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-7: soft-dependency)
// Reference: Traveler_Phase0_Spec_v0.1.md §8.2 (REQ-SAFETY-7: redact_for_log)
//
// CRITICAL: This header is soft-dependency for GATE-P0-3 OAuth (Spec §7.5)
// All log emission in Traveler. MUST route through this function.
#pragma once

#include <string>
#include <string_view>

namespace traveler::safety {

// ============================================================================
// redact_for_log — single redaction API for all log output
//
// Applies the canonical safety pipeline to redact PII and secrets from log
// strings. All std::cerr / log calls in Traveler. MUST route through this
// function per Spec §8.4 TB-C boundary.
//
// Returns redacted string with:
//   - Email addresses replaced with [USER]
//   - File paths replaced with [PATH]
//   - API keys / tokens replaced with [MASKED]
//   - IP addresses replaced with [PATH]
// ============================================================================
[[nodiscard]] std::string redact_for_log(std::string_view raw);

}  // namespace traveler::safety
