// Reference: Traveler_Phase0_Spec_v0.1.md §6.7 (Git Mode — lazygit DNA)
// Reference: Traveler_Phase0_Spec_v0.1.md REQ-GIT-1, REQ-GIT-3
// Reference: Traveler_Phase0_Spec_v0.1.md PC-8
//
// All git operations are subprocess invocations via `git -C <path>`.
// Non-interactive only. No destructive commands against the host repo.
#include "git/backend.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace traveler::git {

// ============================================================================
// Platform: _popen / _pclose (Windows) vs popen / pclose (POSIX)
// ============================================================================
namespace {
#ifdef _WIN32
inline FILE* popen_platform(const char* cmd, const char* mode) { return _popen(cmd, mode); }
inline int pclose_platform(FILE* pipe) { return _pclose(pipe); }
#else
inline FILE* popen_platform(const char* cmd, const char* mode) { return popen(cmd, mode); }
inline int pclose_platform(FILE* pipe) { return pclose(pipe); }
#endif
}  // namespace

// ============================================================================
// Internal: subprocess runner
// ============================================================================

namespace {

/// Run a command and capture stdout. Returns (exit_code, stdout, stderr).
/// Uses popen() for portability. Non-interactive only.
struct SubprocessResult {
    int exit_code{0};
    std::string stdout_str;
    std::string stderr_str;
};

SubprocessResult run_command(const std::string& cmd) {
    SubprocessResult result;

    // Append 2>&1 to combine stderr into stdout for simpler parsing
    std::string full_cmd = cmd + " 2>&1";

    std::unique_ptr<FILE, decltype(&pclose_platform)> pipe(
        popen_platform(full_cmd.c_str(), "r"), pclose_platform);
    if (!pipe) {
        result.exit_code = -1;
        result.stdout_str = "popen failed";
        return result;
    }

    std::array<char, 4096> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result.stdout_str += buffer.data();
    }

    int rc = pclose_platform(pipe.release());
    // WIFEXITED/WEXITSTATUS pattern
    if (rc == -1) {
        result.exit_code = -1;
    } else {
        result.exit_code = rc >> 8;  // WEXITSTATUS equivalent
    }

    return result;
}

/// Centralized shell quoting for repo path, file path, and commit message.
/// Wraps in double quotes, escapes internal \ and " for both POSIX and cmd.exe.
std::string quote_shell_arg(const std::string& arg) {
    std::string result = "\"";
    for (char c : arg) {
        if (c == '\\' || c == '"') result += '\\';
        result += c;
    }
    result += '"';
    return result;
}

/// Run a git command with repo path
SubprocessResult run_git(const std::string& repo_path, const std::string& git_args) {
    std::string cmd = "git -C " + quote_shell_arg(repo_path) + " " + git_args;
    return run_command(cmd);
}

/// Trim leading/trailing whitespace
std::string trim(std::string s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Check if a string starts with a prefix
bool starts_with(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

}  // namespace

// ============================================================================
// Public API
// ============================================================================

bool is_git_available() {
    auto result = run_command("git --version 2>&1");
    return result.exit_code == 0;
}

tl::expected<std::vector<GitBranch>, GitError>
list_branches(const std::string& repo_path) {
    auto result = run_git(repo_path, "branch --list");

    if (result.exit_code != 0) {
        return tl::make_unexpected(GitError::Subprocess(
            "git branch failed: " + result.stdout_str));
    }

    std::vector<GitBranch> branches;
    std::istringstream stream(result.stdout_str);
    std::string line;

    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty()) continue;

        GitBranch branch;
        if (starts_with(line, "* ")) {
            branch.is_current = true;
            branch.name = trim(line.substr(2));
        } else {
            // Lines may have 2-space indent before branch name
            branch.name = trim(line);
        }
        if (!branch.name.empty()) {
            branches.push_back(std::move(branch));
        }
    }

    return branches;
}

tl::expected<std::string, GitError>
current_branch(const std::string& repo_path) {
    auto result = run_git(repo_path, "branch --show-current");

    if (result.exit_code != 0) {
        return tl::make_unexpected(GitError::Subprocess(
            "git branch --show-current failed: " + result.stdout_str));
    }

    return trim(result.stdout_str);
}

tl::expected<GitStatus, GitError>
status(const std::string& repo_path) {
    auto result = run_git(repo_path, "status --porcelain --branch");

    if (result.exit_code != 0) {
        return tl::make_unexpected(GitError::Subprocess(
            "git status --porcelain failed: " + result.stdout_str));
    }

    GitStatus status;
    std::istringstream stream(result.stdout_str);
    std::string line;

    while (std::getline(stream, line)) {
        // First line is branch info: "## branch_name" or "## branch_name...origin/branch [ahead N]"
        if (starts_with(line, "## ")) {
            std::string branch_info = line.substr(3);
            // Strip tracking info after "..."
            auto dots_pos = branch_info.find("...");
            if (dots_pos != std::string::npos) {
                branch_info = branch_info.substr(0, dots_pos);
            }
            // Strip ahead/behind info after " ["
            auto bracket_pos = branch_info.find(" [");
            if (bracket_pos != std::string::npos) {
                branch_info = branch_info.substr(0, bracket_pos);
            }
            status.current_branch = trim(branch_info);
            continue;
        }

        if (line.empty()) continue;

        // Porcelain format: XY PATH, where X=index status, Y=worktree status
        // Lines starting with "??" are untracked
        if (line.size() >= 2) {
            if (starts_with(line, "??")) {
                status.untracked_count++;
            } else {
                status.modified_count++;
                // Check index status column (first char)
                char index_status = line[0];
                if (index_status != ' ' && index_status != '?') {
                    status.staged_count++;
                }
            }
        }
    }

    return status;
}

tl::expected<GitCommitResult, GitError>
commit(const std::string& repo_path, std::string_view message) {
    std::string escaped_msg = quote_shell_arg(std::string(message));
    std::string cmd = "git -C " + quote_shell_arg(repo_path) + " commit -m " + escaped_msg;
    auto result = run_command(cmd);

    GitCommitResult commit_result;
    commit_result.message = std::string(message);

    if (result.exit_code != 0) {
        commit_result.success = false;
        commit_result.error_hint = result.stdout_str;
        return commit_result;
    }

    commit_result.success = true;

    // Get the commit hash for verification
    auto hash_result = last_commit_hash(repo_path);
    if (hash_result) {
        commit_result.commit_hash = *hash_result;
    }

    return commit_result;
}

tl::expected<std::string, GitError>
last_commit_message(const std::string& repo_path) {
    auto result = run_git(repo_path, "log -1 --format=%s");

    if (result.exit_code != 0) {
        return tl::make_unexpected(GitError::Subprocess(
            "git log -1 failed: " + result.stdout_str));
    }

    return trim(result.stdout_str);
}

tl::expected<std::string, GitError>
last_commit_hash(const std::string& repo_path) {
    auto result = run_git(repo_path, "log -1 --format=%h");

    if (result.exit_code != 0) {
        return tl::make_unexpected(GitError::Subprocess(
            "git log -1 failed: " + result.stdout_str));
    }

    return trim(result.stdout_str);
}

tl::expected<GitDiff, GitError>
diff_file(const std::string& repo_path, std::string_view file_path) {
    std::string escaped_path = quote_shell_arg(std::string(file_path));
    auto result = run_git(repo_path, "diff -- " + escaped_path);

    if (result.exit_code != 0) {
        return tl::make_unexpected(GitError::Subprocess(
            "git diff failed: " + result.stdout_str));
    }

    GitDiff diff;
    diff.file_path = std::string(file_path);
    diff.diff_content = result.stdout_str;
    return diff;
}

tl::expected<std::string, GitError>
diff_all(const std::string& repo_path) {
    auto result = run_git(repo_path, "diff");

    if (result.exit_code != 0) {
        return tl::make_unexpected(GitError::Subprocess(
            "git diff failed: " + result.stdout_str));
    }

    return result.stdout_str;
}

}  // namespace traveler::git
