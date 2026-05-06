// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-2, REQ-ADAPTERS-4)
// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-5)
// Port: llama.cpp local inference adapter — Phase 0 stub.
// Full implementation scheduled for GATE-P0-5 when llama.cpp submodule is
// properly integrated with xmake static linking and "Ayane" model config.
#include "llamacpp_adapter.h"

#include <string>

namespace traveler::adapters {

// ============================================================================
// LlamaCppProvider — offline stub (GATE-P0-5 implementation target)
// ============================================================================

LlamaCppProvider::LlamaCppProvider(std::string model_path)
    : model_path_(std::move(model_path)) {}

std::string LlamaCppProvider::id() const { return "llamacpp"; }

bool LlamaCppProvider::supports_streaming() const { return true; }

tl::expected<void, llm::Error>
LlamaCppProvider::generate(const llm::GenerateOptions& /*opts*/,
                           std::function<void(llm::GenerateChunk)> /*on_chunk*/) {
    return tl::make_unexpected(llm::Error::Provider(
        "llama.cpp offline inference not yet implemented (P0-5)"));
}

}  // namespace traveler::adapters
