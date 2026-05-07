// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-2, REQ-ADAPTERS-4)
// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-5)
// Reference: Traveler_Phase0_Spec_v0.1.md §9.3 (REQ-LLAMA-1 through REQ-LLAMA-5)
// Port: llama.cpp local inference adapter — bounded offline provider substrate.
#include "llamacpp_adapter.h"

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>

namespace traveler::adapters {
namespace fs = std::filesystem;

// ============================================================================
// Internal helpers (anonymous namespace) — minimal JSON builder
// ============================================================================
namespace {

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

}  // anonymous namespace

// ============================================================================
// determine_state — file-existence + compile-time capability check
// ============================================================================

LlamaCppState LlamaCppProvider::determine_state(const fs::path& model_path) {
    std::error_code ec;
    const bool file_exists = fs::exists(model_path, ec) && !ec
                          && fs::is_regular_file(model_path, ec) && !ec;

    if (!file_exists) {
        return LlamaCppState::ModelMissing;
    }

    // File exists, but check if we have llama.cpp support compiled in
    if (!compiled_with_llama()) {
        return LlamaCppState::ModelPresentNoInference;
    }

    // File exists + compiled with llama support = ready
    return LlamaCppState::InferenceReady;
}

// ============================================================================
// Constructor — path-only (auto-detect state)
// ============================================================================

LlamaCppProvider::LlamaCppProvider(std::string model_path)
    : model_path_(std::move(model_path))
    , state_(determine_state(model_path_)) {}

// ============================================================================
// Constructor — path + forced state (for testing)
// ============================================================================

LlamaCppProvider::LlamaCppProvider(std::string model_path, LlamaCppState forced_state)
    : model_path_(std::move(model_path))
    , state_(forced_state) {}

// ============================================================================
// id / supports_streaming
// ============================================================================

std::string LlamaCppProvider::id() const { return "llamacpp"; }

bool LlamaCppProvider::supports_streaming() const {
    // Streaming is only supported when inference is ready.
    // In stub / no-inference states, we report false since we can't
    // actually stream tokens.
    return state_ == LlamaCppState::InferenceReady;
}

// ============================================================================
// generate — bounded state-aware implementation
// ============================================================================

tl::expected<void, llm::Error>
LlamaCppProvider::generate(const llm::GenerateOptions& /*opts*/,
                           std::function<void(llm::GenerateChunk)> /*on_chunk*/) {
    switch (state_) {
        case LlamaCppState::ModelMissing:
            return tl::make_unexpected(llm::Error::Provider(
                "llama.cpp offline model not found. "
                "Expected GGUF file at: " + model_path_ + ". "
                "Run 'traveler --offline' to configure and download a model, "
                "or place a compatible GGUF file at the model path."));

        case LlamaCppState::ModelPresentNoInference:
            return tl::make_unexpected(llm::Error::Provider(
                "Model file found at '" + model_path_ + "' but llama.cpp "
                "inference is not available in this build. "
                "The binary was compiled without TRAVELER_WITH_LLAMA. "
                "Rebuild with -Dwith_llama=y to enable offline inference."));

        case LlamaCppState::InferenceReady:
            // Real inference path — requires TRAVELER_WITH_LLAMA to be defined
            // and a valid llama.cpp backend to be initialized.
            // This is the future implementation path for Worker W1 / GATE-P0-5 T7.
            // Currently, even in InferenceReady state, the actual llama.cpp
            // model loading and token generation are not wired up.
            return tl::make_unexpected(llm::Error::Provider(
                "llama.cpp backend is compiled in (TRAVELER_WITH_LLAMA) and "
                "model file exists at '" + model_path_ + "', but the inference "
                "pipeline is not yet wired. "
                "This is a current-resource gap — real inference requires "
                "llama.cpp context initialization and token generation wiring "
                "(GATE-P0-5 T11 / Worker W1)."));
    }

    return tl::make_unexpected(llm::Error::Provider(
        "llama.cpp offline inference: unknown state"));
}

// ============================================================================
// to_request_json — convert GenerateOptions → llama.cpp chat JSON
//
// llama.cpp chat format (similar to OpenAI Chat Completions):
//   {"model":"ayane","messages":[...],"max_tokens":256,"temperature":0.7}
//
// This is the Phase 0 testability seam for PC-13 adapter corpus testing.
// The format represents the normalized input that would be passed to
// llama.cpp's chat template before tokenization.
// ============================================================================

std::string LlamaCppProvider::to_request_json(const llm::GenerateOptions& opts) {
    std::ostringstream js;

    // Strip llamacpp/ vendor prefix if present
    std::string model = opts.model;
    if (model.rfind("llamacpp/", 0) == 0) {
        model = model.substr(9);
    }

    js << "{\"model\":\"" << json_escape(model) << "\"";

    // --- messages array (all messages, including system) ---
    js << ",\"messages\":[";
    for (size_t i = 0; i < opts.messages.size(); ++i) {
        if (i > 0) js << ",";
        js << "{\"role\":\"" << json_escape(opts.messages[i].role) << "\""
           << ",\"content\":\"" << json_escape(opts.messages[i].content) << "\"}";
    }
    js << "]";

    // --- max_tokens ---
    if (opts.max_tokens.has_value()) {
        js << ",\"max_tokens\":" << opts.max_tokens.value();
    } else {
        js << ",\"max_tokens\":256";  // llama.cpp default is smaller
    }

    // --- temperature ---
    if (opts.temperature.has_value()) {
        js << ",\"temperature\":" << opts.temperature.value();
    }

    js << "}";
    return js.str();
}

}  // namespace traveler::adapters
