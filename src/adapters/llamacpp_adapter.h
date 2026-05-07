// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-2, REQ-ADAPTERS-4)
// Reference: Traveler_Phase0_Spec_v0.1.md §9.3 (REQ-LLAMA-1 through REQ-LLAMA-5)
// Reference: ~/hatch-v3/packages/opencode/src/provider/provider.ts (Provider interface)
// Port: llama.cpp local inference adapter
#pragma once

#include "../llm/provider.h"
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <tl/expected.hpp>

namespace traveler::adapters {

// ============================================================================
// LlamaCppState — explicit bounded offline provider substrate states
//
// ModelMissing:            No GGUF file at model_path.
// ModelPresentNoInference: GGUF exists but real llama.cpp inference is
//                          unavailable (not compiled with TRAVELER_WITH_LLAMA,
//                          or runtime initialization failed).
// InferenceReady:          Compiled with llama support AND model path present
//                          and valid — inference can proceed.
// ============================================================================
enum class LlamaCppState {
    ModelMissing,
    ModelPresentNoInference,
    InferenceReady,
};

[[nodiscard]] constexpr const char* to_string(LlamaCppState s) noexcept {
    switch (s) {
        case LlamaCppState::ModelMissing:            return "model-missing";
        case LlamaCppState::ModelPresentNoInference: return "model-present-no-inference";
        case LlamaCppState::InferenceReady:          return "inference-ready";
    }
    return "unknown";
}

// ============================================================================
// LlamaCppProvider — offline llama.cpp inference adapter
//
// Implements llm::Provider for local model inference via llama.cpp.
//
// Bounded substrate with three explicit states. The state is determined by:
//   1. Whether the model_path points to an existing GGUF file.
//   2. Whether the binary was compiled with TRAVELER_WITH_LLAMA.
//
// Phase 0 bounded behavior:
//   - ModelMissing:         generate() returns Provider error "model not found"
//   - ModelPresentNoInference: generate() returns error about no llama support
//   - InferenceReady:       generate() passes to llama.cpp backend
//                           (requires TRAVELER_WITH_LLAMA + valid model path)
//
// Offline mode (Proposal §3.3): restricted to 1 callsign mount on
// Core-i3-class hardware budget.
// ============================================================================

class LlamaCppProvider : public llm::Provider {
public:
    // Construct with path to GGUF model file.
    // The state is automatically determined at construction time.
    explicit LlamaCppProvider(std::string model_path);

    // Construct with explicit model path and runtime state override.
    // Primarily for testing with artificial states.
    LlamaCppProvider(std::string model_path, LlamaCppState forced_state);

    // --- State introspection ---

    // Current state of the provider substrate.
    [[nodiscard]] LlamaCppState state() const noexcept { return state_; }

    // Whether the provider is ready for inference.
    [[nodiscard]] bool is_ready() const noexcept {
        return state_ == LlamaCppState::InferenceReady;
    }

    // Whether model file exists on disk.
    [[nodiscard]] bool model_file_exists() const noexcept {
        return state_ != LlamaCppState::ModelMissing;
    }

    // The model path used for construction.
    [[nodiscard]] const std::string& model_path() const noexcept {
        return model_path_;
    }

    // --- llm::Provider interface ---

    std::string id() const override;
    bool supports_streaming() const override;

    tl::expected<void, llm::Error>
    generate(const llm::GenerateOptions& opts,
             std::function<void(llm::GenerateChunk)> on_chunk) override;

    // --- Compile-time capability check ---

    // Whether TRAVELER_WITH_LLAMA was defined at build time (i.e., llama.cpp
    // was linked into the binary).
    [[nodiscard]] static constexpr bool compiled_with_llama() noexcept {
#ifdef TRAVELER_WITH_LLAMA
        return true;
#else
        return false;
#endif
    }

    // --- Normalisation helpers (public for testing) ---

    // Convert Traveler GenerateOptions to llama.cpp chat request JSON.
    // All messages (including system) go into the messages array.
    // Returns JSON string representing the normalized input for llama.cpp inference.
    [[nodiscard]] static std::string
    to_request_json(const llm::GenerateOptions& opts);

    // Factory: determine state by checking if model_path exists on disk.
    // Does NOT require TRAVELER_WITH_LLAMA — works for diagnostic purposes.
    [[nodiscard]] static LlamaCppState
    determine_state(const std::filesystem::path& model_path);

private:
    std::string model_path_;
    LlamaCppState state_;
};

}  // namespace traveler::adapters
