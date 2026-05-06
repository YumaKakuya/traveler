// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-3)
// Port: Tool-call adapter scaffold for future MCP client integration
//
// Header-only scaffold — unused in Phase 0. Committed to ensure Phase 1+
// MCP-client GATE has the adapter substrate ready.
//
// Phase 0 Non-Goal: MCP client integration (Spec §1.7, item 2).
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace traveler::adapters {

// ============================================================================
// ToolCall — placeholder struct for future tool-call operations
//
// Phase 1+ will define:
//   - tool_call_id, name, arguments (JSON string)
//   - ToolResult: tool_call_id, content, is_error
//   - Conversion between provider-specific tool-call formats and canonical
// ============================================================================

struct ToolCall {
    std::string id;            // unique call identifier
    std::string name;          // function name
    std::string arguments;     // JSON-encoded arguments string
};

struct ToolResult {
    std::string tool_call_id;  // matches ToolCall.id
    std::string content;       // result content (text or JSON)
    bool is_error{false};      // true if tool execution failed
};

// ============================================================================
// ToolCallAdapter — placeholder provider for tool-enabled inference
//
// Future: wraps a base Provider to add tool-call support.
// Anthropic: tools + tool_use / tool_result content blocks
// OpenAI: tools + tool_calls in assistant messages
// Google: tools + functionCall / functionResponse parts
// ============================================================================

class ToolCallAdapter {
public:
    virtual ~ToolCallAdapter() = default;

    // Check if the provider supports native tool calling
    [[nodiscard]] virtual bool supports_tool_calls() const { return false; }

    // Convert Traveler tool definitions to provider-specific format
    [[nodiscard]] static std::string
    tools_to_provider_format(const std::vector<ToolCall>& /*tools*/,
                             std::string_view /*provider_id*/) {
        // Phase 0: not implemented
        return "[]";
    }
};

}  // namespace traveler::adapters
