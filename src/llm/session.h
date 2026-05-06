// Reference: Traveler_Phase0_Spec_v0.1.md §7.6 (REQ-SESSION-1 to REQ-SESSION-4)
// Reference: ~/hatch-v3/packages/opencode/src/session/ (Hatch. session model)
#pragma once

#include "provider.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <tl/expected.hpp>

namespace traveler::llm {

// ============================================================================
// Session — ordered message list + metadata
//
// A "session" per Spec §7.6 is an ordered list of messages plus metadata.
// Sessions are persisted to SQLite (§7.7).
// ============================================================================

struct SessionMeta {
    std::string id;              // UUID
    std::string callsign;        // "@vega" / "@altair" / "@orion" / "@rigel" / ""
    std::string model;           // "anthropic/claude-opus-4-7"
    int64_t started_at{0};       // unix epoch seconds
    int64_t last_active_at{0};   // unix epoch seconds
    std::string title;           // optional, LLM-generated
};

// Extended Message with seq and created_at for persistence
struct SessionMessage {
    std::string id;              // auto-increment integer (as string for storage)
    std::string session_id;      // FK to sessions.id
    int32_t seq{0};              // order within session
    std::string role;            // "system" | "user" | "assistant" | "tool"
    std::string content;         // message body
    int64_t created_at{0};       // unix epoch seconds
};

// ============================================================================
// SessionState — runtime state of an active session
// ============================================================================
enum class SessionState {
    idle,           // no generation in progress
    streaming,      // provider is streaming chunks
    interrupted,    // user cancelled mid-stream
    error,          // provider returned an error
};

// ============================================================================
// Session — runtime conversation state
//
// Owns an ordered message list. Manages streaming state for the active
// provider generation. On completion, the assistant message is committed.
// On cancel, a "[interrupted]" marker is appended to partial content.
// ============================================================================
class Session {
public:
    explicit Session(SessionMeta meta);
    ~Session() = default;

    // --- Metadata access ---
    [[nodiscard]] const SessionMeta& meta() const { return meta_; }
    [[nodiscard]] const std::string& id() const { return meta_.id; }
    [[nodiscard]] SessionState state() const { return state_; }

    // --- Message list ---
    [[nodiscard]] const std::vector<SessionMessage>& messages() const { return messages_; }
    [[nodiscard]] size_t message_count() const { return messages_.size(); }

    // Add a message to the session. Returns the new seq number.
    int32_t add_message(std::string role, std::string content);

    // --- Streaming ---

    // Begin a streaming generation. Sets state to streaming, creates a partial
    // assistant message slot. The on_chunk callback is called for each chunk;
    // when done=true, the message is committed. Returns error if already streaming.
    tl::expected<void, Error>
    begin_generation(std::function<void(GenerateChunk)> on_chunk);

    // Called by the provider for each chunk during streaming.
    void on_chunk(GenerateChunk chunk);

    // Cancel the current generation. Sets state to interrupted, appends
    // "[interrupted]" marker to the partial assistant message.
    void cancel();

    // --- Helper for provider integration ---

    // Build the message list suitable for provider GenerateOptions.
    [[nodiscard]] std::vector<Message> to_provider_messages() const;

private:
    SessionMeta meta_;
    SessionState state_{SessionState::idle};
    std::vector<SessionMessage> messages_;
    std::string partial_content_;       // accumulating assistant content during streaming
    std::function<void(GenerateChunk)> user_on_chunk_;  // forward to caller
};

// ============================================================================
// SessionManager — manages multiple sessions
// ============================================================================
class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager() = default;

    // Create a new session and make it active.
    tl::expected<Session*, Error>
    create_session(std::string callsign, std::string model);

    // Get the active session, if any.
    [[nodiscard]] Session* active_session() { return active_; }

    // Switch active session by ID. Returns nullptr if not found.
    Session* switch_session(std::string_view session_id);

    // List all sessions.
    [[nodiscard]] const std::vector<std::unique_ptr<Session>>& all_sessions() const {
        return sessions_;
    }

private:
    std::vector<std::unique_ptr<Session>> sessions_;
    Session* active_{nullptr};
};

}  // namespace traveler::llm
