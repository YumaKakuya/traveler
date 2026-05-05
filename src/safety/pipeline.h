// Reference: Traveler_Phase0_Spec_v0.1.md §8.2 (REQ-SAFETY-1: three-stage pipeline)
// Reference: ~/hatch-v3/packages/hatch-safety/src/translator/llm/canonicalize.ts:53-96
#pragma once

#include <string>
#include <string_view>

namespace traveler::safety {

// ============================================================================
// Pipeline — three-stage safety orchestrator (Spec REQ-SAFETY-1)
//
// Stage 1: Anonymise (PII strip before LLM input)
// Stage 2: Mask (post-LLM output redaction)
// Stage 3: Translate (pass-through scaffold for Phase 0)
//
// All stages route through canonicalize() per REQ-SAFETY-2 / N-3.
// ============================================================================

// Anonymise: strips PII from input before sending to LLM
[[nodiscard]] std::string anonymise(std::string_view input);

// Mask: redacts secrets from LLM output
[[nodiscard]] std::string mask(std::string_view output);

// Translate: Phase 0 pass-through (full LLM-translation deferred to Phase 1+)
[[nodiscard]] std::string translate(std::string_view input);

}  // namespace traveler::safety
