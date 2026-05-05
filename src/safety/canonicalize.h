// Reference: ~/hatch-v3/packages/hatch-safety/src/translator/llm/canonicalize.ts:1-96
// Reference: ~/hatch-v3/packages/hatch-safety/src/collector/anonymizer.ts:100-154
// Reference: Traveler_Phase0_Spec_v0.1.md §8.2 (REQ-SAFETY-2: single canonicalize function)
// Port: 1:1 TypeScript -> C++20. Anonymisation + Masking SHARE canonicalize().
#pragma once

#include <string>
#include <string_view>

namespace traveler::safety {

// ============================================================================
// canonicalize — single unified pipeline (Spec REQ-SAFETY-2, N-3)
//
// Order (faithful to Hatch SSS-001 §3.1 C1):
//   1. Strip NUL bytes
//   2. Strip PII (emails, paths, IPs, hostnames)  — anonymiser
//   3. Mask secrets (API keys, tokens, credentials) — mask
//   4. Return canonical string
//
// Returns: redacted string with PII replaced by placeholders:
//   [USER], [PATH], [PATH]:[NUM], [MASKED], etc.
// ============================================================================
[[nodiscard]] std::string canonicalize(std::string_view input);

}  // namespace traveler::safety
