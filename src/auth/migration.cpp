// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-5: TB-052 migration)
// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/token.ts (ensureMigration)
#include "migration.h"
#include "credentials.h"
#include "oauth.h"

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/stat.h>
#include <tuple>

#ifdef _WIN32
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace traveler::auth {

using json = nlohmann::json;

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

static std::string hatch_credentials_path() {
    return home_directory() + "/.config/hatch/credentials.json";
}

bool detect_hatch_credentials() {
    struct stat st;
    return stat(hatch_credentials_path().c_str(), &st) == 0;
}

bool traveler_credentials_exist() {
    struct stat st;
    return stat(credentials_file_path().c_str(), &st) == 0;
}

// ============================================================================
// Migration
// ============================================================================

tl::expected<MigrationResult, Error>
migrate_hatch_credentials() {
    MigrationResult result;

    // Check if Hatch credentials exist
    if (!detect_hatch_credentials()) {
        result.status = MigrationStatus::not_needed;
        result.message = "No Hatch. credentials found — nothing to migrate.";
        return result;
    }

    // Check if Traveler credentials already exist
    if (traveler_credentials_exist()) {
        result.status = MigrationStatus::not_needed;
        result.message = "Traveler. credentials already exist — migration skipped.";
        return result;
    }

    // Read Hatch credentials file
    std::string hatch_path = hatch_credentials_path();
    json hatch_data;
    try {
        std::ifstream ifs(hatch_path);
        if (!ifs.is_open()) {
            result.status = MigrationStatus::failed;
            result.message = "Cannot open Hatch. credentials file.";
            return result;
        }
        ifs >> hatch_data;
    } catch (const json::parse_error& e) {
        result.status = MigrationStatus::failed;
        result.message =
            std::string("Failed to parse Hatch. credentials: ") + e.what();
        return result;
    }

    // Extract the OAuth credentials from Hatch format
    // Hatch uses claudeAiOauth key; Traveler uses anthropic key
    // Also check for legacy "claudeAiOauth" format
    std::string access_token;
    std::string refresh_token;
    int64_t expires_at{0};

    auto extract_oauth = [](const json& obj) -> std::tuple<std::string, std::string, int64_t> {
        if (obj.contains("accessToken") && obj.contains("refreshToken") &&
            obj.contains("expiresAt")) {
            return {
                obj["accessToken"].get<std::string>(),
                obj["refreshToken"].get<std::string>(),
                obj["expiresAt"].get<int64_t>()
            };
        }
        if (obj.contains("access_token") && obj.contains("refresh_token") &&
            obj.contains("expires_at")) {
            return {
                obj["access_token"].get<std::string>(),
                obj["refresh_token"].get<std::string>(),
                obj["expires_at"].get<int64_t>()
            };
        }
        return {"", "", 0};
    };

    // Try Hatch format: {claudeAiOauth: {...}}
    if (hatch_data.contains("claudeAiOauth") && hatch_data["claudeAiOauth"].is_object()) {
        auto [at, rt, ea] = extract_oauth(hatch_data["claudeAiOauth"]);
        access_token = std::move(at);
        refresh_token = std::move(rt);
        expires_at = ea;
    }
    // Try Traveler format: {anthropic: {...}}
    if (access_token.empty() && hatch_data.contains("anthropic") &&
        hatch_data["anthropic"].is_object()) {
        auto [at, rt, ea] = extract_oauth(hatch_data["anthropic"]);
        access_token = std::move(at);
        refresh_token = std::move(rt);
        expires_at = ea;
    }

    if (refresh_token.empty()) {
        result.status = MigrationStatus::failed;
        result.message = "Hatch. credentials file exists but contains no valid OAuth tokens.";
        return result;
    }

    // Use the Hatch refresh_token to obtain an independent pair
    auto refresh_result = refresh_access_token(refresh_token);
    if (!refresh_result) {
        // Case 1: Refresh HTTP call failed — pass through error, do NOT do shared-pair migration
        result.status = MigrationStatus::failed;
        result.message = "Failed to obtain independent token pair: " + refresh_result.error().message;
        return result;
    }

    auto& new_tokens = *refresh_result;

    // Check if the same refresh_token was returned (no rotation)
    if (new_tokens.refresh_token == refresh_token) {
        OAuthCredentials creds;
        creds.access_token = new_tokens.access_token;
        creds.refresh_token = new_tokens.refresh_token;
        creds.expires_at = time(nullptr) + new_tokens.expires_in;

        auto write_result = write_credentials("anthropic", creds);
        if (!write_result) {
            result.status = MigrationStatus::failed;
            result.message = "Failed to write Traveler. credentials.";
            return result;
        }

        result.status = MigrationStatus::success_shared;
        result.message =
            "Hatch. credentials migrated to Traveler. as a shared refresh-token pair "
            "(Anthropic backend did not issue a new refresh token). Token refresh by "
            "either Hatch. or Traveler. may invalidate the other tool's session.\n\n"
            "To avoid session conflicts, use either Hatch. or Traveler. — not both — "
            "for Anthropic OAuth-authenticated sessions until the next manual "
            "re-authentication.\n\n"
            "Recovery: run 'traveler providers logout anthropic && "
            "traveler providers login anthropic' to obtain a fresh independent token pair.";
        return result;
    }

    // Success with independent pair
    OAuthCredentials creds;
    creds.access_token = new_tokens.access_token;
    creds.refresh_token = new_tokens.refresh_token;
    creds.expires_at = time(nullptr) + new_tokens.expires_in;

    auto write_result = write_credentials("anthropic", creds);
    if (!write_result) {
        result.status = MigrationStatus::failed;
        result.message = "Failed to write Traveler. credentials.";
        return result;
    }

    result.status = MigrationStatus::success_independent;
    result.message = "Hatch. credentials successfully migrated. Traveler. now has "
                     "an independent OAuth token pair.";
    return result;
}

}  // namespace traveler::auth
