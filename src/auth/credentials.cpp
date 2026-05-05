// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/token.ts (readCredentialsData, writeBackCredentials)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-3: credentials.json 0600)
#include "credentials.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <pwd.h>
#endif

namespace traveler::auth {

using json = nlohmann::json;

// ============================================================================
// helpers
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
    const struct passwd* pw = getpwuid(getuid());
    if (pw) return pw->pw_dir;
    return ".";
#endif
}

static std::string config_dir() {
    return home_directory() + "/.config/traveler";
}

std::string credentials_file_path() {
    return config_dir() + "/credentials.json";
}

static tl::expected<std::string, Error> ensure_config_dir() {
    std::string dir = config_dir();
    // mode 0700 for config directory
    int rc = mkdir(dir.c_str(), 0700);
    if (rc != 0 && errno != EEXIST) {
        return tl::make_unexpected(
            Error::Provider("Cannot create config directory: " + dir));
    }
    return dir;
}

static tl::expected<json, Error> read_json_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        if (errno == ENOENT) {
            return json::object();  // empty file → empty object
        }
        return tl::make_unexpected(
            Error::Provider("Cannot open credentials file: " + path));
    }
    try {
        json j;
        ifs >> j;
        return j;
    } catch (const json::parse_error& e) {
        return json::object();  // corrupted → treat as empty
    }
}

static tl::expected<void, Error> write_json_file(
    const std::string& path, const json& data) {
    // Atomic write: write to temp path, then rename
    std::string tmp_path = path + ".tmp." + std::to_string(getpid()) + "."
                           + std::to_string(rand());
    {
        std::ofstream ofs(tmp_path, std::ios::trunc);
        if (!ofs.is_open()) {
            return tl::make_unexpected(
                Error::Provider("Cannot write credentials file: " + tmp_path));
        }
        ofs << data.dump(2) << "\n";
        ofs.close();
    }
    // Set mode 0600
    chmod(tmp_path.c_str(), 0600);
    // Atomic rename
    if (rename(tmp_path.c_str(), path.c_str()) != 0) {
        unlink(tmp_path.c_str());
        return tl::make_unexpected(
            Error::Provider("Cannot finalize credentials file: " + path));
    }
    return {};
}

// ============================================================================
// OAuthCredentials
// ============================================================================

bool OAuthCredentials::expired() const noexcept {
    return expires_at > 0 &&
           static_cast<int64_t>(time(nullptr)) >= expires_at;
}

// ============================================================================
// read / write / delete credentials
// ============================================================================

tl::expected<std::optional<OAuthCredentials>, Error>
read_credentials(std::string_view provider) {
    auto path = credentials_file_path();
    auto data = read_json_file(path);
    if (!data) return tl::make_unexpected(data.error());

    auto& j = *data;
    auto it = j.find(std::string(provider));
    if (it == j.end() || !it->is_object()) {
        return std::nullopt;
    }

    auto& obj = *it;
    OAuthCredentials creds;
    if (obj.contains("access_token") && obj["access_token"].is_string()) {
        creds.access_token = obj["access_token"].get<std::string>();
    } else {
        return std::nullopt;
    }
    if (obj.contains("refresh_token") && obj["refresh_token"].is_string()) {
        creds.refresh_token = obj["refresh_token"].get<std::string>();
    }
    if (obj.contains("expires_at") && obj["expires_at"].is_number()) {
        creds.expires_at = obj["expires_at"].get<int64_t>();
    }
    return creds;
}

tl::expected<void, Error>
write_credentials(std::string_view provider, const OAuthCredentials& creds) {
    auto dir_result = ensure_config_dir();
    if (!dir_result) return tl::make_unexpected(dir_result.error());

    auto path = credentials_file_path();
    auto data_result = read_json_file(path);
    if (!data_result) return tl::make_unexpected(data_result.error());

    json j = *data_result;
    json entry;
    entry["type"] = "oauth";
    entry["access_token"] = creds.access_token;
    entry["refresh_token"] = creds.refresh_token;
    entry["expires_at"] = creds.expires_at;

    j[std::string(provider)] = entry;
    return write_json_file(path, j);
}

tl::expected<void, Error>
delete_credentials(std::string_view provider) {
    auto path = credentials_file_path();
    auto data_result = read_json_file(path);
    if (!data_result) return tl::make_unexpected(data_result.error());

    json j = *data_result;
    j.erase(std::string(provider));
    return write_json_file(path, j);
}

}  // namespace traveler::auth
