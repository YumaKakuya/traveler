// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/token.ts (readCredentialsData, writeBackCredentials)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-3: credentials.json 0600)
#include "credentials.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#include <shlobj.h>
#include <sys/stat.h>
#include <windows.h>
#include <aclapi.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
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

static int make_private_dir(const std::string& dir) {
#ifdef _WIN32
    return _mkdir(dir.c_str());
#else
    return mkdir(dir.c_str(), 0700);
#endif
}

static int current_process_id() {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

// Returns true if the file's access was successfully restricted to the
// current user.  On POSIX this means mode 0600 (owner rw only).  On Windows
// this sets an explicit DACL that limits access to the current user via an
// ACL and also applies the DOS read/write attribute as a belt-and-suspenders
// measure.
static bool set_private_file_mode(const std::string& path) {
#ifdef _WIN32
    // Belt-and-suspenders: set DOS read/write attribute for the owner.
    ::_chmod(path.c_str(), _S_IREAD | _S_IWRITE);

    // Get the current process token to identify the current user SID.
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return false;
    }

    // Retrieve the token user SID.
    DWORD dwSize = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &dwSize);
    if (dwSize == 0) {
        CloseHandle(hToken);
        return false;
    }

    BYTE* tokenBuffer = static_cast<BYTE*>(std::malloc(dwSize));
    if (!tokenBuffer) {
        CloseHandle(hToken);
        return false;
    }
    PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(tokenBuffer);
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
        std::free(tokenBuffer);
        CloseHandle(hToken);
        return false;
    }

    // Build an EXPLICIT_ACCESS entry granting full control to the current user
    // only.  SET_ACCESS with a null existing ACL creates a fresh ACL containing
    // only this single ACE (no inherited entries).
    EXPLICIT_ACCESSA ea = {};
    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
    ea.Trustee.ptstrName = reinterpret_cast<LPSTR>(pTokenUser->User.Sid);

    PACL pNewAcl = nullptr;
    DWORD dwResult = SetEntriesInAclA(1, &ea, nullptr, &pNewAcl);
    std::free(tokenBuffer);
    CloseHandle(hToken);

    if (dwResult != ERROR_SUCCESS) {
        return false;
    }

    // Apply the new DACL, replacing any existing or inherited entries.
    // Owner, group, and SACL are left unchanged (nullptr).
    dwResult = SetNamedSecurityInfoA(
        const_cast<LPSTR>(path.c_str()),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,   // owner (unchanged)
        nullptr,   // group (unchanged)
        pNewAcl,   // new DACL
        nullptr    // SACL (unchanged)
    );

    LocalFree(pNewAcl);
    return (dwResult == ERROR_SUCCESS);
#else
    return (::chmod(path.c_str(), 0600) == 0);
#endif
}

std::string credentials_file_path() {
    return config_dir() + "/credentials.json";
}

static tl::expected<std::string, Error> ensure_config_dir() {
    std::string home = home_directory();
    if (home == ".") {
        return tl::make_unexpected(
            Error::Provider("Cannot determine home directory for credentials storage"));
    }

    // Create intermediate directories one level at a time so that a missing
    // parent (e.g. ~/.config) does not cause the whole chain to fail.
    // On Windows _mkdir only creates the last path component, same as POSIX mkdir.
    std::string config_parent = home + "/.config";
    int rc = make_private_dir(config_parent);
    if (rc != 0 && errno != EEXIST) {
        return tl::make_unexpected(
            Error::Provider("Cannot create directory: " + config_parent));
    }

    std::string traveler_dir = config_parent + "/traveler";
    rc = make_private_dir(traveler_dir);
    if (rc != 0 && errno != EEXIST) {
        return tl::make_unexpected(
            Error::Provider("Cannot create directory: " + traveler_dir));
    }
    return traveler_dir;
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
    std::string tmp_path = path + ".tmp." + std::to_string(current_process_id()) + "."
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
    // Restrict file access to the current user (0600 on POSIX,
    // explicit ACL on Windows).  Fail hard if the OS cannot enforce
    // the restriction — never write credentials with broad access.
    if (!set_private_file_mode(tmp_path)) {
#ifdef _WIN32
        DeleteFileA(tmp_path.c_str());
        return tl::make_unexpected(
            Error::Provider("Cannot restrict ACL on credentials temp file"));
#else
        std::remove(tmp_path.c_str());
        return tl::make_unexpected(
            Error::Provider("Cannot set private mode on credentials temp file: " + tmp_path));
#endif
    }

    // Atomic rename.  POSIX rename() atomically replaces the target.
    // Windows rename() fails if the target already exists, so use
    // MoveFileEx with MOVEFILE_REPLACE_EXISTING (rewrite-on-open).
#ifdef _WIN32
    if (!MoveFileExA(tmp_path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp_path.c_str());
        return tl::make_unexpected(
            Error::Provider("Cannot finalize credentials file: " + path));
    }
    // Re-apply ACL on the final file: MoveFileExA within the same volume
    // preserves the ACL, but if the config directory spans volumes the
    // destination may inherit parent ACLs.  Belt-and-suspenders.
    if (!set_private_file_mode(path)) {
        return tl::make_unexpected(
            Error::Provider("Cannot restrict ACL on credentials file"));
    }
#else
    if (::rename(tmp_path.c_str(), path.c_str()) != 0) {
        std::remove(tmp_path.c_str());
        return tl::make_unexpected(
            Error::Provider("Cannot finalize credentials file: " + path));
    }
#endif
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
