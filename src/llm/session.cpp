// Reference: Traveler_Phase0_Spec_v0.1.md §7.6 (REQ-SESSION-1 to REQ-SESSION-4)
#include "session.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <random>
#include <sstream>

namespace traveler::llm {

// ============================================================================
// UUID generation (inline, no external dependency)
// ============================================================================

static std::string generate_uuid() {
    // Simple UUID v4 generation using random_device
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    std::array<uint8_t, 16> bytes;
    for (auto& b : bytes) b = static_cast<uint8_t>(dist(gen));

    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    char buf[37];
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  bytes[0], bytes[1], bytes[2], bytes[3],
                  bytes[4], bytes[5], bytes[6], bytes[7],
                  bytes[8], bytes[9], bytes[10], bytes[11],
                  bytes[12], bytes[13], bytes[14], bytes[15]);
    return std::string(buf);
}

static int64_t now_epoch() {
    return static_cast<int64_t>(std::time(nullptr));
}

// ============================================================================
// Session
// ============================================================================

Session::Session(SessionMeta meta)
    : meta_(std::move(meta)) {
    if (meta_.started_at == 0) {
        meta_.started_at = now_epoch();
    }
    if (meta_.last_active_at == 0) {
        meta_.last_active_at = meta_.started_at;
    }
    if (meta_.id.empty()) {
        meta_.id = generate_uuid();
    }
}

int32_t Session::add_message(std::string role, std::string content) {
    int32_t seq = static_cast<int32_t>(messages_.size());
    SessionMessage msg;
    msg.seq = seq;
    msg.role = std::move(role);
    msg.content = std::move(content);
    msg.created_at = now_epoch();
    messages_.push_back(std::move(msg));
    meta_.last_active_at = now_epoch();
    return seq;
}

tl::expected<void, Error>
Session::begin_generation(std::function<void(GenerateChunk)> on_chunk) {
    if (state_ == SessionState::streaming) {
        return tl::make_unexpected(
            Error::Provider("Generation already in progress"));
    }

    state_ = SessionState::streaming;
    partial_content_.clear();
    user_on_chunk_ = std::move(on_chunk);
    return {};
}

void Session::on_chunk(GenerateChunk chunk) {
    if (state_ != SessionState::streaming) return;

    if (chunk.error_message) {
        // REQ-SESSION-4: Preserve partial conversation on error
        if (!partial_content_.empty()) {
            add_message("assistant", partial_content_ + " [interrupted]");
            partial_content_.clear();
        }
        state_ = SessionState::error;
        // Surface the error as a message
        add_message("system", *chunk.error_message);
        if (user_on_chunk_) user_on_chunk_(chunk);
        return;
    }

    partial_content_ += chunk.delta;

    if (chunk.done) {
        // Commit the assistant message
        state_ = SessionState::idle;
        add_message("assistant", partial_content_);
        partial_content_.clear();
    }

    if (user_on_chunk_) user_on_chunk_(chunk);
}

void Session::cancel() {
    if (state_ != SessionState::streaming) return;

    if (!partial_content_.empty()) {
        partial_content_ += "\n[interrupted]";
        add_message("assistant", partial_content_);
    }
    partial_content_.clear();
    state_ = SessionState::interrupted;
}

std::vector<Message> Session::to_provider_messages() const {
    std::vector<Message> result;
    result.reserve(messages_.size());
    for (const auto& m : messages_) {
        result.push_back({m.role, m.content});
    }
    return result;
}

// ============================================================================
// SessionManager
// ============================================================================

tl::expected<Session*, Error>
SessionManager::create_session(std::string callsign, std::string model) {
    SessionMeta meta;
    meta.callsign = std::move(callsign);
    meta.model = std::move(model);

    auto session = std::make_unique<Session>(std::move(meta));
    Session* ptr = session.get();
    sessions_.push_back(std::move(session));
    active_ = ptr;
    return ptr;
}

Session* SessionManager::switch_session(std::string_view session_id) {
    for (auto& s : sessions_) {
        if (s->id() == session_id) {
            active_ = s.get();
            return active_;
        }
    }
    return nullptr;
}

}  // namespace traveler::llm
