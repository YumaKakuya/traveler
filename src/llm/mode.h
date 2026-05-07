// Reference: Traveler_Phase0_Spec_v0.1.md §6 Pass Criteria PC-10
// Reference: briefs/reconcile-p0-2-2026-05-06.md §2 PC-10, §5 Dispatch #4
//
// LLM mode substrate for GATE-P0-2 PC-10:
//   - Single-session conversation state via Session + SessionManager
//   - MockProvider: deterministic mock LLM responder (no network, no secrets)
//   - LlmMode: wraps session + mock provider for LLM mode UI
//   - Stage render: exposes complete assistant response as display text
//
// Real provider validation belongs to GATE-P0-3.
#pragma once

#include "provider.h"
#include "session.h"

#include <memory>
#include <string>
#include <string_view>

namespace traveler::llm {

// ============================================================================
// MockProvider — deterministic mock LLM responder for PC-10
//
// Implements the Provider interface without network I/O. Responses are
// map-driven: "Hello." maps to the canonical mock greeting. All other
// prompts receive a generic echo-style response.
//
// This mock is scoped to GATE-P0-2 only. GATE-P0-3 replaces it with real
// provider adapters (Anthropic / OpenAI / Google / llama.cpp).
// ============================================================================
class MockProvider : public Provider {
public:
    // Canonic mock response for the "Hello." prompt.
    static constexpr std::string_view kCanonicalResponse =
        "Hello from Traveler mock LLM";

    // Map a user prompt to a deterministic mock response.
    // "Hello."                   → kCanonicalResponse
    // "[empty]"                  → "I didn't receive a message."
    // any other text             → "Mock echo: <first 80 chars of input>"
    static std::string respond(std::string_view user_input);

    // ---- Provider interface ----
    [[nodiscard]] std::string id() const override;
    [[nodiscard]] bool supports_streaming() const override;

    // generate() takes the last user message from opts.messages and
    // synchronously calls on_chunk() with a single chunk containing
    // the mock response (delta = respond(text), done = true).
    // Never returns an error in mock mode.
    tl::expected<void, Error>
    generate(const GenerateOptions& opts,
             std::function<void(GenerateChunk)> on_chunk) override;
};

// ============================================================================
// LlmMode — single-session LLM conversation state (PC-10)
//
// Owns a SessionManager + MockProvider.  Provides a simple API for the
// LLM-mode TUI (or unit test):
//
//   enter()       — start a fresh conversation session
//   send_prompt() — submit user text, receive assistant response
//   stage_text()  — full conversation formatted for Stage display
// ============================================================================
class LlmMode {
public:
    LlmMode();
    ~LlmMode() = default;

    // Start a new conversation.  Discards any prior session and creates
    // a fresh one with callsign="@vega" and model="mock".
    void enter();

    // Send user text to the mock provider.  The user message is appended
    // to the session, the mock response is generated, and the assistant
    // message is committed.  Returns the assistant's response text.
    // Throws nothing — errors are surfaced in the return string.
    std::string send_prompt(std::string_view user_text);

    // Full conversation formatted as plain text for Stage rendering.
    // Each message is prefixed with its role and separated by blank lines.
    [[nodiscard]] std::string stage_text() const;

    // Access the active session.  Returns nullptr before enter().
    [[nodiscard]] Session* session() { return active_session_; }
    [[nodiscard]] const Session* session() const { return active_session_; }

    // True after enter() has been called and a session exists.
    [[nodiscard]] bool has_active_session() const {
        return active_session_ != nullptr;
    }

private:
    MockProvider mock_provider_;
    SessionManager session_manager_;
    Session* active_session_{nullptr};
};

// ============================================================================
// Free function — render a Session as Stage-display text
// ============================================================================

// Format all messages of a session into a single string suitable for
// Stage (conversation pane) display.  Role labels are human-readable:
//   [You]        for "user"
//   [Assistant]  for "assistant"
//   [System]     for "system"
std::string render_conversation(const Session& session);

}  // namespace traveler::llm
