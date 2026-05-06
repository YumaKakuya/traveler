// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-1, REQ-ADAPTERS-2)
// Reference: ~/hatch-v3/packages/opencode/src/provider/provider.ts (Provider interface)
// Port: Anthropic Messages API request/response normalisation
#pragma once

#include "../llm/provider.h"
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace traveler::adapters {

// ============================================================================
// AnthropicAdapter — maps Traveler canonical Message to Anthropic Messages API
//
// Anthropic Messages API format (https://docs.anthropic.com/en/api/messages):
//   Request:  {"model":"...", "messages":[{"role":"user","content":"..."}],
//              "max_tokens":N, "system":"..."}
//   Response: {"content":[{"type":"text","text":"..."}], "stop_reason":"..."}
//
// Key difference from OpenAI: system prompt is a top-level "system" field,
// NOT a message in the messages array.
// ============================================================================

class AnthropicAdapter : public llm::Provider {
public:
    explicit AnthropicAdapter(std::string api_key);

    // --- llm::Provider interface ---

    std::string id() const override;
    bool supports_streaming() const override;

    tl::expected<void, llm::Error>
    generate(const llm::GenerateOptions& opts,
             std::function<void(llm::GenerateChunk)> on_chunk) override;

    // --- Normalisation helpers (public for testing) ---

    // Convert Traveler GenerateOptions to Anthropic Messages API request JSON.
    // System messages (role="system") are extracted to the top-level "system" field.
    // Returns JSON string ready for HTTP POST to /v1/messages.
    [[nodiscard]] static std::string
    to_request_json(const llm::GenerateOptions& opts);

    // Extract full text content from an Anthropic complete response JSON.
    // Parses the content array, concatenating all text blocks.
    [[nodiscard]] static tl::expected<std::string, llm::Error>
    from_response_json(std::string_view json);

    // Parse a single SSE event line into a GenerateChunk.
    // Handles: content_block_start (text), content_block_delta (text_delta),
    // message_stop, and ping events.
    [[nodiscard]] static tl::expected<llm::GenerateChunk, llm::Error>
    from_sse_line(std::string_view sse_data);

private:
    std::string api_key_;
    int retry_count_{0};
};

}  // namespace traveler::adapters
