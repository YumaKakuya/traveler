// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-2, REQ-ADAPTERS-4)
// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-5)
// Port: llama.cpp local inference adapter — Phase 0 stub.
// Full implementation scheduled for GATE-P0-5 when llama.cpp submodule is
// properly integrated with xmake static linking and "Ayane" model config.
#include "llamacpp_adapter.h"

#include <cstdio>
#include <sstream>
#include <string>

namespace traveler::adapters {

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

// ---------------------------------------------------------------------------
// to_request_json — convert GenerateOptions → llama.cpp chat JSON
//
// llama.cpp chat format (similar to OpenAI Chat Completions):
//   {"model":"ayane","messages":[...],"max_tokens":256,"temperature":0.7}
//
// This is the Phase 0 testability seam for PC-13 adapter corpus testing.
// The format represents the normalized input that would be passed to
// llama.cpp's chat template before tokenization.
// ---------------------------------------------------------------------------
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
