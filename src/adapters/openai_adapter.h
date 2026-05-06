// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-1, REQ-ADAPTERS-2)
// Reference: ~/hatch-v3/packages/opencode/src/provider/provider.ts (Provider interface)
// Port: OpenAI Chat Completions API request/response normalisation
#pragma once

#include "../llm/provider.h"
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace traveler::adapters {

// ============================================================================
// OpenAIAdapter — maps Traveler canonical Message to OpenAI Chat Completions
//
// OpenAI Chat Completions format (https://platform.openai.com/docs/api-reference/chat):
//   Request:  {"model":"...", "messages":[{"role":"system","content":"..."},
//              {"role":"user","content":"..."}], "max_tokens":N,
//              "temperature":f, "stream":true}
//   SSE:      data: {"choices":[{"delta":{"content":"Hello"},"index":0}]}
//
// Key difference from Anthropic: system role IS a standard message.
// Streaming uses SSE (text/event-stream).
// ============================================================================

class OpenAIAdapter : public llm::Provider {
public:
    explicit OpenAIAdapter(std::string api_key);

    // --- llm::Provider interface ---

    std::string id() const override;
    bool supports_streaming() const override;

    tl::expected<void, llm::Error>
    generate(const llm::GenerateOptions& opts,
             std::function<void(llm::GenerateChunk)> on_chunk) override;

    // --- Normalisation helpers (public for testing) ---

    // Convert Traveler GenerateOptions to OpenAI Chat Completions request JSON.
    // All messages (including system role) go into the messages array.
    // Returns JSON string ready for HTTP POST to /v1/chat/completions.
    [[nodiscard]] static std::string
    to_request_json(const llm::GenerateOptions& opts);

    // Parse a single SSE event line into a GenerateChunk.
    // Handles: choices[].delta.content, choices[].finish_reason, and [DONE].
    [[nodiscard]] static tl::expected<llm::GenerateChunk, llm::Error>
    from_sse_line(std::string_view sse_data);

private:
    std::string api_key_;
    int retry_count_{0};
};

}  // namespace traveler::adapters
