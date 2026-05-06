// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-1, REQ-ADAPTERS-2)
// Reference: ~/hatch-v3/packages/opencode/src/provider/provider.ts (Provider interface)
// Port: Gemini API request/response normalisation
#pragma once

#include "../llm/provider.h"
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace traveler::adapters {

// ============================================================================
// GoogleAdapter — maps Traveler canonical Message to Gemini API
//
// Gemini API format (https://ai.google.dev/gemini-api/docs/text-generation):
//   Request:  {"contents":[{"role":"user","parts":[{"text":"..."}]}],
//              "systemInstruction":{"parts":[{"text":"..."}]},
//              "generationConfig":{"maxOutputTokens":N, "temperature":f}}
//   Response: {"candidates":[{"content":{"parts":[{"text":"..."}],
//              "role":"model"}}]}
//
// Key difference: system prompt goes in "systemInstruction" field.
// Messages use "parts" arrays with {"text":"..."} objects.
// ============================================================================

class GoogleAdapter : public llm::Provider {
public:
    explicit GoogleAdapter(std::string api_key);

    // --- llm::Provider interface ---

    std::string id() const override;
    bool supports_streaming() const override;

    tl::expected<void, llm::Error>
    generate(const llm::GenerateOptions& opts,
             std::function<void(llm::GenerateChunk)> on_chunk) override;

    // --- Normalisation helpers (public for testing) ---

    // Convert Traveler GenerateOptions to Gemini API request JSON.
    // System messages (role="system") are extracted to "systemInstruction".
    // Returns JSON string ready for HTTP POST to Gemini endpoint.
    [[nodiscard]] static std::string
    to_request_json(const llm::GenerateOptions& opts);

    // Extract full text content from a Gemini complete response JSON.
    // Parses candidates[].content.parts[], concatenating all text parts.
    [[nodiscard]] static tl::expected<std::string, llm::Error>
    from_response_json(std::string_view json);

    // Parse a single SSE event line into a GenerateChunk
    // (Gemini uses SSE for streaming; chunk format is similar to complete
    //  response but with partial candidates).
    [[nodiscard]] static tl::expected<llm::GenerateChunk, llm::Error>
    from_sse_line(std::string_view sse_data);

private:
    std::string api_key_;
    int retry_count_{0};
};

}  // namespace traveler::adapters
