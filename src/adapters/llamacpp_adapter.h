// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-2, REQ-ADAPTERS-4)
// Reference: ~/hatch-v3/packages/opencode/src/provider/provider.ts (Provider interface)
// Port: llama.cpp local inference adapter
#pragma once

#include "../llm/provider.h"
#include <functional>
#include <string>
#include <string_view>
#include <tl/expected.hpp>

namespace traveler::adapters {

// ============================================================================
// LlamaCppProvider — offline llama.cpp inference adapter
//
// Implements llm::Provider for local model inference via llama.cpp.
// Phase 0: stub implementation. Full llama.cpp integration in GATE-P0-5
// when the llama.cpp submodule is integrated with xmake.
//
// Offline mode (Proposal §3.3): restricted to 1 callsign mount on
// Core-i3-class hardware budget.
// ============================================================================

class LlamaCppProvider : public llm::Provider {
public:
    // Construct with path to GGUF model file.
    explicit LlamaCppProvider(std::string model_path);

    // --- llm::Provider interface ---

    std::string id() const override;
    bool supports_streaming() const override;

    tl::expected<void, llm::Error>
    generate(const llm::GenerateOptions& opts,
             std::function<void(llm::GenerateChunk)> on_chunk) override;

    // --- Normalisation helpers (public for testing) ---

    // Convert Traveler GenerateOptions to llama.cpp chat request JSON.
    // All messages (including system) go into the messages array.
    // Returns JSON string representing the normalized input for llama.cpp inference.
    [[nodiscard]] static std::string
    to_request_json(const llm::GenerateOptions& opts);

private:
    std::string model_path_;
};

}  // namespace traveler::adapters
