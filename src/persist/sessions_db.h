// Reference: Traveler_Phase0_Spec_v0.1.md §7.7 (SQLite Session Store)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.7 (REQ-SQLITE-1, REQ-SQLITE-2, REQ-SQLITE-3)
#pragma once

#include "../llm/session.h"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <tl/expected.hpp>

// Forward-declare sqlite3
struct sqlite3;

namespace traveler::persist {

// ============================================================================
// SessionsDb — SQLite-backed session and message store
//
// Single-thread mode (T-7). Database file: ~/.local/state/traveler/sessions.db
// Schema: sessions, messages, schema_version tables.
// No FTS in Phase 0.
// ============================================================================

class SessionsDb {
public:
    SessionsDb();
    ~SessionsDb();

    // Non-copyable, non-movable (sqlite3* raw pointer ownership)
    SessionsDb(const SessionsDb&) = delete;
    SessionsDb& operator=(const SessionsDb&) = delete;
    SessionsDb(SessionsDb&&) = delete;
    SessionsDb& operator=(SessionsDb&&) = delete;

    // --- Lifecycle ---

    // Open (or create) the database. Runs schema migration if needed.
    tl::expected<void, llm::Error> open();

    // Close the database. Safe to call multiple times.
    void close();

    // Check if the database is open.
    [[nodiscard]] bool is_open() const { return db_ != nullptr; }

    // --- Sessions ---

    // Insert a new session. Returns the session ID (UUID).
    tl::expected<std::string, llm::Error>
    insert_session(const llm::SessionMeta& meta);

    // Update a session's last_active_at and title.
    tl::expected<void, llm::Error>
    update_session(std::string_view session_id,
                   int64_t last_active_at,
                   std::optional<std::string_view> title = std::nullopt);

    // Get a session by ID.
    tl::expected<std::optional<llm::SessionMeta>, llm::Error>
    get_session(std::string_view session_id);

    // List all sessions, ordered by last_active_at DESC.
    tl::expected<std::vector<llm::SessionMeta>, llm::Error>
    list_sessions();

    // List sessions for a given callsign.
    tl::expected<std::vector<llm::SessionMeta>, llm::Error>
    list_sessions_by_callsign(std::string_view callsign);

    // Delete a session and all its messages (cascade).
    tl::expected<void, llm::Error>
    delete_session(std::string_view session_id);

    // --- Messages ---

    // Insert a message into a session. Returns the auto-incremented message ID.
    tl::expected<int64_t, llm::Error>
    insert_message(const llm::SessionMessage& msg);

    // Get all messages for a session, ordered by seq ASC.
    tl::expected<std::vector<llm::SessionMessage>, llm::Error>
    get_messages(std::string_view session_id);

    // Update a message's content (e.g., for partial → committed).
    tl::expected<void, llm::Error>
    update_message_content(int64_t message_id, std::string_view content);

    // --- Schema migration ---

    // Current schema version. Increment when schema changes.
    static constexpr int kCurrentSchemaVersion = 1;

    // Get the current schema version from the database.
    tl::expected<int, llm::Error> get_schema_version();

private:
    sqlite3* db_{nullptr};

    // Create tables if they don't exist (idempotent).
    tl::expected<void, llm::Error> create_tables();

    // Run schema migration from the current version to latest.
    tl::expected<void, llm::Error> migrate_schema(int current_version);

    // Execute a single SQL statement with no result.
    tl::expected<void, llm::Error> exec(std::string_view sql);

    // Get the database file path.
    [[nodiscard]] static std::string db_path();
};

}  // namespace traveler::persist
