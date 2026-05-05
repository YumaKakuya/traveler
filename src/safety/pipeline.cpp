// Reference: Traveler_Phase0_Spec_v0.1.md §8.2 (REQ-SAFETY-1)
#include "pipeline.h"
#include "canonicalize.h"

namespace traveler::safety {

// Stage 1: Anonymise — strip PII before LLM input
std::string anonymise(std::string_view input) {
    return canonicalize(input);
}

// Stage 2: Mask — redact secrets from LLM output
std::string mask(std::string_view output) {
    return canonicalize(output);
}

// Stage 3: Translate — Phase 0 pass-through scaffold
// Full LLM-translation pipeline deferred to Phase 1+ per Spec §8.2 REQ-SAFETY-1.
std::string translate(std::string_view input) {
    return std::string(input);
}

}  // namespace traveler::safety
