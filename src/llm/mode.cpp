// Reference: Traveler_Phase0_Spec_v0.1.md §6 Pass Criteria PC-10
// Reference: briefs/reconcile-p0-2-2026-05-06.md §2 PC-10, §5 Dispatch #4
//
// MockProvider + LlmMode implementation.
// No network, no secrets, no real provider calls.
#include "mode.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace traveler::llm {

// ============================================================================
// MockProvider
// ============================================================================

std::string MockProvider::respond(std::string_view user_input) {
    // Trim leading/trailing whitespace for comparison
    auto start = user_input.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) {
        return "I didn't receive a message.";
    }
    auto end = user_input.find_last_not_of(" \t\n\r");
    auto trimmed = user_input.substr(start, end - start + 1);

    // Canonical "Hello." → known response
    if (trimmed == "Hello." || trimmed == "Hello" || trimmed == "hello." ||
        trimmed == "hello") {
        return std::string(kCanonicalResponse);
    }

    // Generic echo for any other input (capped for readability)
    constexpr size_t kCap = 80;
    std::string echo(trimmed.data(),
                     std::min(trimmed.size(), kCap));
    return "Mock echo: " + echo;
}

std::string MockProvider::id() const { return "mock"; }

bool MockProvider::supports_streaming() const { return true; }

tl::expected<void, Error>
MockProvider::generate(const GenerateOptions& opts,
                        std::function<void(GenerateChunk)> on_chunk) {
    // Find the last user message
    std::string user_text;
    for (auto it = opts.messages.rbegin(); it != opts.messages.rend(); ++it) {
        if (it->role == "user") {
            user_text = it->content;
            break;
        }
    }

    // Determine mock response
    std::string response = respond(user_text);

    // If streaming is requested, simulate chunk-by-chunk delivery.
    // For simplicity, we deliver the whole response as one chunk.
    // A more realistic mock could split into word-level chunks.
    if (supports_streaming()) {
        // First chunk: partial text, not done
        if (!response.empty()) {
            GenerateChunk chunk;
            chunk.delta = response.substr(0, response.size() / 2);
            chunk.done = false;
            on_chunk(chunk);
        }
    }

    // Final chunk: remainder + done flag
    GenerateChunk final_chunk;
    if (supports_streaming() && !response.empty()) {
        final_chunk.delta = response.substr(response.size() / 2);
    } else {
        final_chunk.delta = response;
    }
    final_chunk.done = true;
    on_chunk(final_chunk);

    return {};
}

// ============================================================================
// LlmMode
// ============================================================================

LlmMode::LlmMode() = default;

void LlmMode::enter() {
    auto result = session_manager_.create_session("@vega", "mock");
    if (result.has_value()) {
        active_session_ = result.value();
    } else {
        // create_session should never fail for mock, but guard anyway
        active_session_ = nullptr;
    }
}

std::string LlmMode::send_prompt(std::string_view user_text) {
    if (!active_session_) {
        return "Error: no active session. Call enter() first.";
    }

    // Add the user message to the session
    active_session_->add_message("user", std::string(user_text));

    // Build GenerateOptions from session state
    GenerateOptions opts;
    opts.model = active_session_->meta().model;
    opts.messages = active_session_->to_provider_messages();

    // Begin streaming generation on the session
    auto begin_result = active_session_->begin_generation(
        [](GenerateChunk /*chunk*/) {
            // The session itself handles chunk accumulation.
            // This callback is for UI streaming; in mock mode it's a no-op.
        });

    if (!begin_result.has_value()) {
        return "Error: " + begin_result.error().message;
    }

    // Run mock generation — on_chunk calls will commit the message
    std::string final_response;
    auto gen_result = mock_provider_.generate(
        opts,
        [&](GenerateChunk chunk) {
            active_session_->on_chunk(chunk);
            if (chunk.done) {
                // Capture the final committed message text
                const auto& msgs = active_session_->messages();
                if (!msgs.empty()) {
                    final_response = msgs.back().content;
                }
            }
        });

    if (!gen_result.has_value()) {
        return "Error: " + gen_result.error().message;
    }

    return final_response;
}

std::string LlmMode::stage_text() const {
    if (!active_session_) {
        return "(no active session)";
    }
    return render_conversation(*active_session_);
}

// ============================================================================
// render_conversation
// ============================================================================

std::string render_conversation(const Session& session) {
    std::ostringstream oss;
    const auto& messages = session.messages();
    bool first = true;

    for (const auto& msg : messages) {
        if (!first) {
            oss << "\n\n";
        }
        first = false;

        // Role label
        if (msg.role == "user") {
            oss << "[You] ";
        } else if (msg.role == "assistant") {
            oss << "[Assistant] ";
        } else if (msg.role == "system") {
            oss << "[System] ";
        } else {
            oss << "[" << msg.role << "] ";
        }

        oss << msg.content;
    }

    return oss.str();
}

}  // namespace traveler::llm
