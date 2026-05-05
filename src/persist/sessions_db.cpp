// Reference: Traveler_Phase0_Spec_v0.1.md §7.7 (SQLite Session Store)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.7 (REQ-SQLITE-1, REQ-SQLITE-2, REQ-SQLITE-3)
#include "sessions_db.h"

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include <shlobj.h>
#else
#include <pwd.h>
#endif

namespace traveler::persist {

// ============================================================================
// SQL schema (embedded as string constant)
// ============================================================================

static const char* SCHEMA_SQL = R"SQL(
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions (
    id TEXT PRIMARY KEY,
    callsign TEXT NOT NULL,
    model TEXT NOT NULL,
    started_at INTEGER NOT NULL,
    last_active_at INTEGER NOT NULL,
    title TEXT
);

CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT NOT NULL REFERENCES sessions(id),
    seq INTEGER NOT NULL,
    role TEXT NOT NULL,
    content TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    UNIQUE(session_id, seq)
);

CREATE INDEX IF NOT EXISTS idx_messages_session ON messages(session_id, seq);
CREATE INDEX IF NOT EXISTS idx_sessions_callsign ON sessions(callsign, last_active_at DESC);
)SQL";

// ============================================================================
// Helpers
// ============================================================================

static std::string home_directory() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PROFILE, nullptr, 0, path))) {
        return path;
    }
    return ".";
#else
    const char* home = getenv("HOME");
    if (home) return home;
    struct passwd* pw = getpwuid(getuid());
    if (pw) return pw->pw_dir;
    return ".";
#endif
}

std::string SessionsDb::db_path() {
    // ~/.local/state/traveler/sessions.db
    return home_directory() + "/.local/state/traveler/sessions.db";
}

static tl::expected<void, llm::Error> ensure_db_dir() {
    std::string dir = home_directory() + "/.local/state/traveler";
    int rc = mkdir(dir.c_str(), 0700);
    // Recursively create parent dirs
    std::string local = home_directory() + "/.local";
    mkdir(local.c_str(), 0700);
    std::string state = home_directory() + "/.local/state";
    mkdir(state.c_str(), 0700);

    if (rc != 0 && errno != EEXIST) {
        // Try again after creating parents
        rc = mkdir(dir.c_str(), 0700);
        if (rc != 0 && errno != EEXIST) {
            return tl::make_unexpected(
                llm::Error::Provider("Cannot create database directory: " + dir));
        }
    }
    return {};
}

// ============================================================================
// SessionsDb
// ============================================================================

SessionsDb::SessionsDb() = default;

SessionsDb::~SessionsDb() {
    close();
}

tl::expected<void, llm::Error> SessionsDb::open() {
    if (db_) {
        close();
    }

    auto dir_result = ensure_db_dir();
    if (!dir_result) return dir_result;

    std::string path = db_path();
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        return tl::make_unexpected(
            llm::Error::Provider("Cannot open database: " + error));
    }

    // Enable WAL mode for better concurrent read safety (though single-thread)
    auto wal_result = exec("PRAGMA journal_mode=WAL");
    (void)wal_result;  // best-effort; not critical if fails

    // Run schema migration
    int current_version = 0;
    auto version_result = get_schema_version();
    if (version_result) {
        current_version = *version_result;
    }

    if (current_version == 0) {
        return create_tables();
    }

    return migrate_schema(current_version);
}

void SessionsDb::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

tl::expected<void, llm::Error> SessionsDb::exec(std::string_view sql) {
    if (!db_) {
        return tl::make_unexpected(
            llm::Error::Provider("Database not open"));
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, std::string(sql).c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "unknown error";
        sqlite3_free(err_msg);
        return tl::make_unexpected(
            llm::Error::Provider("SQL error: " + error));
    }
    return {};
}

tl::expected<void, llm::Error> SessionsDb::create_tables() {
    auto result = exec(SCHEMA_SQL);
    if (!result) return result;

    // Set initial schema version
    std::string set_version = "INSERT OR REPLACE INTO schema_version (id, version) VALUES (1, "
                              + std::to_string(kCurrentSchemaVersion) + ")";
    return exec(set_version);
}

tl::expected<void, llm::Error> SessionsDb::migrate_schema(int current_version) {
    if (current_version >= kCurrentSchemaVersion) return {};

    // Phase 0: no migrations needed yet (v1 only)
    // Future: add migration steps for each version increment
    for (int v = current_version + 1; v <= kCurrentSchemaVersion; ++v) {
        // Placeholder for future migrations
        std::string update =
            "INSERT OR REPLACE INTO schema_version (version) VALUES ("
            + std::to_string(v) + ")";
        auto result = exec(update);
        if (!result) return result;
    }
    return {};
}

tl::expected<int, llm::Error> SessionsDb::get_schema_version() {
    if (!db_) return 0;  // not open yet

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_,
                                "SELECT version FROM schema_version LIMIT 1",
                                -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        // Table might not exist yet
        return 0;
    }

    rc = sqlite3_step(stmt);
    int version = 0;
    if (rc == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return version;
}

// ============================================================================
// Sessions CRUD
// ============================================================================

tl::expected<std::string, llm::Error>
SessionsDb::insert_session(const llm::SessionMeta& meta) {
    if (!db_) {
        return tl::make_unexpected(llm::Error::Provider("Database not open"));
    }

    const char* sql = R"(
        INSERT INTO sessions (id, callsign, model, started_at, last_active_at, title)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to prepare insert session: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    sqlite3_bind_text(stmt, 1, meta.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, meta.callsign.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, meta.model.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, meta.started_at);
    sqlite3_bind_int64(stmt, 5, meta.last_active_at);
    if (meta.title.empty()) {
        sqlite3_bind_null(stmt, 6);
    } else {
        sqlite3_bind_text(stmt, 6, meta.title.c_str(), -1, SQLITE_TRANSIENT);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to insert session: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    return meta.id;
}

tl::expected<void, llm::Error>
SessionsDb::update_session(std::string_view session_id,
                            int64_t last_active_at,
                            std::optional<std::string_view> title) {
    if (!db_) {
        return tl::make_unexpected(llm::Error::Provider("Database not open"));
    }

    std::string sql = "UPDATE sessions SET last_active_at = ?";
    if (title) sql += ", title = ?";
    sql += " WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to prepare update session: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    int param = 1;
    sqlite3_bind_int64(stmt, param++, last_active_at);
    if (title) {
        sqlite3_bind_text(stmt, param++, std::string(*title).c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_text(stmt, param, std::string(session_id).c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to update session: " +
                                 std::string(sqlite3_errmsg(db_))));
    }
    return {};
}

tl::expected<std::optional<llm::SessionMeta>, llm::Error>
SessionsDb::get_session(std::string_view session_id) {
    if (!db_) {
        return tl::make_unexpected(llm::Error::Provider("Database not open"));
    }

    const char* sql = "SELECT id, callsign, model, started_at, last_active_at, title "
                      "FROM sessions WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to prepare get session: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    sqlite3_bind_text(stmt, 1, std::string(session_id).c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    std::optional<llm::SessionMeta> result;
    if (rc == SQLITE_ROW) {
        llm::SessionMeta meta;
        meta.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        meta.callsign = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        meta.model = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        meta.started_at = sqlite3_column_int64(stmt, 3);
        meta.last_active_at = sqlite3_column_int64(stmt, 4);
        auto title_text = sqlite3_column_text(stmt, 5);
        if (title_text) meta.title = reinterpret_cast<const char*>(title_text);
        result = std::move(meta);
    }
    sqlite3_finalize(stmt);
    return result;
}

tl::expected<std::vector<llm::SessionMeta>, llm::Error>
SessionsDb::list_sessions() {
    if (!db_) {
        return tl::make_unexpected(llm::Error::Provider("Database not open"));
    }

    const char* sql = "SELECT id, callsign, model, started_at, last_active_at, title "
                      "FROM sessions ORDER BY last_active_at DESC";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to prepare list sessions: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    std::vector<llm::SessionMeta> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        llm::SessionMeta meta;
        meta.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        meta.callsign = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        meta.model = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        meta.started_at = sqlite3_column_int64(stmt, 3);
        meta.last_active_at = sqlite3_column_int64(stmt, 4);
        auto title_text = sqlite3_column_text(stmt, 5);
        if (title_text) meta.title = reinterpret_cast<const char*>(title_text);
        result.push_back(std::move(meta));
    }
    sqlite3_finalize(stmt);
    return result;
}

tl::expected<std::vector<llm::SessionMeta>, llm::Error>
SessionsDb::list_sessions_by_callsign(std::string_view callsign) {
    if (!db_) {
        return tl::make_unexpected(llm::Error::Provider("Database not open"));
    }

    const char* sql = "SELECT id, callsign, model, started_at, last_active_at, title "
                      "FROM sessions WHERE callsign = ? ORDER BY last_active_at DESC";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to prepare list sessions: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    sqlite3_bind_text(stmt, 1, std::string(callsign).c_str(), -1, SQLITE_TRANSIENT);

    std::vector<llm::SessionMeta> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        llm::SessionMeta meta;
        meta.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        meta.callsign = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        meta.model = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        meta.started_at = sqlite3_column_int64(stmt, 3);
        meta.last_active_at = sqlite3_column_int64(stmt, 4);
        auto title_text = sqlite3_column_text(stmt, 5);
        if (title_text) meta.title = reinterpret_cast<const char*>(title_text);
        result.push_back(std::move(meta));
    }
    sqlite3_finalize(stmt);
    return result;
}

tl::expected<void, llm::Error>
SessionsDb::delete_session(std::string_view session_id) {
    // Delete messages first (foreign key cascade not guaranteed without PRAGMA)
    {
        const char* sql = "DELETE FROM messages WHERE session_id = ?";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, std::string(session_id).c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    const char* sql = "DELETE FROM sessions WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to prepare delete session: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    sqlite3_bind_text(stmt, 1, std::string(session_id).c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to delete session: " +
                                 std::string(sqlite3_errmsg(db_))));
    }
    return {};
}

// ============================================================================
// Messages CRUD
// ============================================================================

tl::expected<int64_t, llm::Error>
SessionsDb::insert_message(const llm::SessionMessage& msg) {
    if (!db_) {
        return tl::make_unexpected(llm::Error::Provider("Database not open"));
    }

    const char* sql = R"(
        INSERT INTO messages (session_id, seq, role, content, created_at)
        VALUES (?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to prepare insert message: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    sqlite3_bind_text(stmt, 1, msg.session_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, msg.seq);
    sqlite3_bind_text(stmt, 3, msg.role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, msg.created_at);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to insert message: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    return sqlite3_last_insert_rowid(db_);
}

tl::expected<std::vector<llm::SessionMessage>, llm::Error>
SessionsDb::get_messages(std::string_view session_id) {
    if (!db_) {
        return tl::make_unexpected(llm::Error::Provider("Database not open"));
    }

    const char* sql = "SELECT id, session_id, seq, role, content, created_at "
                      "FROM messages WHERE session_id = ? ORDER BY seq ASC";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to prepare get messages: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    sqlite3_bind_text(stmt, 1, std::string(session_id).c_str(), -1, SQLITE_TRANSIENT);

    std::vector<llm::SessionMessage> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        llm::SessionMessage msg;
        int64_t id = sqlite3_column_int64(stmt, 0);
        msg.id = std::to_string(id);
        // session_id from column 1 is already known
        msg.seq = sqlite3_column_int(stmt, 2);
        msg.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        msg.created_at = sqlite3_column_int64(stmt, 5);
        result.push_back(std::move(msg));
    }
    sqlite3_finalize(stmt);
    return result;
}

tl::expected<void, llm::Error>
SessionsDb::update_message_content(int64_t message_id, std::string_view content) {
    if (!db_) {
        return tl::make_unexpected(llm::Error::Provider("Database not open"));
    }

    const char* sql = "UPDATE messages SET content = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to prepare update message: " +
                                 std::string(sqlite3_errmsg(db_))));
    }

    sqlite3_bind_text(stmt, 1, std::string(content).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, message_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return tl::make_unexpected(
            llm::Error::Provider("Failed to update message: " +
                                 std::string(sqlite3_errmsg(db_))));
    }
    return {};
}

}  // namespace traveler::persist
