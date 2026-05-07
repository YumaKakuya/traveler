// Reference: Traveler_Phase0_Spec_v0.1.md §6.7 (Git Mode — lazygit DNA)
// Reference: Traveler_Phase0_Spec_v0.1.md REQ-GIT-1, REQ-GIT-3
// Reference: Traveler_Phase0_Spec_v0.1.md PC-8
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <tl/expected.hpp>

namespace traveler::git {

// ============================================================================
// GitError — subprocess invocation or parse failure
// ============================================================================
struct GitError {
    std::string message;

    static GitError Subprocess(std::string msg) { return {std::move(msg)}; }
    static GitError Parse(std::string msg)      { return {std::move(msg)}; }
    static GitError Unavailable()               { return {"git binary not found"}; }
};

// ============================================================================
// GitBranch — a single branch entry
// ============================================================================
struct GitBranch {
    std::string name;
    bool is_current{false};
};

// ============================================================================
// GitStatus — porcelain status summary (PC-8)
// ============================================================================
struct GitStatus {
    std::string current_branch;       // branch name (with '*' decoration stripped off)
    int modified_count{0};            // M, MM, etc. — == `git status --porcelain | wc -l`
    int staged_count{0};             // lines containing "M " or "[MARC] "
    int untracked_count{0};          // lines starting with "?? "
};

// ============================================================================
// GitCommitResult — outcome of a commit attempt
// ============================================================================
struct GitCommitResult {
    bool success{false};
    std::string message;              // full commit message used
    std::string commit_hash;          // abbreviated hash from git log -1 --format=%h (empty on failure)
    std::string error_hint;           // stderr from git on failure (empty on success)
};

// ============================================================================
// GitDiff — diff output for a single file
// ============================================================================
struct GitDiff {
    std::string file_path;
    std::string diff_content;         // output of `git diff -- <file>`
};

// ============================================================================
// Check availability
// ============================================================================

/// Returns true if `git` is available on PATH (REQ-GIT-3).
[[nodiscard]] bool is_git_available();

// ============================================================================
// Branch operations (PC-8: branch list with '*')
// ============================================================================

/// List all branches. The current branch has `is_current = true`.
/// Equivalent to `git branch --list`.
[[nodiscard]] tl::expected<std::vector<GitBranch>, GitError>
list_branches(const std::string& repo_path);

/// Return the name of the currently checked-out branch.
/// Equivalent to `git branch --show-current`.
[[nodiscard]] tl::expected<std::string, GitError>
current_branch(const std::string& repo_path);

// ============================================================================
// Status operations (PC-8: modified file count == git status --porcelain | wc -l)
// ============================================================================

/// Return porcelain status summary.
/// Equivalent to parsing `git status --porcelain --branch`.
[[nodiscard]] tl::expected<GitStatus, GitError>
status(const std::string& repo_path);

// ============================================================================
// Commit operations (PC-8: commit with a message, observable in git log -1)
// ============================================================================

/// Commit staged changes with the given message.
/// Non-interactive: uses `git commit -m <message>`.
/// Returns the commit hash on success.
[[nodiscard]] tl::expected<GitCommitResult, GitError>
commit(const std::string& repo_path, std::string_view message);

/// Verify that the most recent commit has the expected message.
/// Equivalent to `git log -1 --format=%s`.
[[nodiscard]] tl::expected<std::string, GitError>
last_commit_message(const std::string& repo_path);

/// Return the abbreviated hash of the most recent commit.
/// Equivalent to `git log -1 --format=%h`.
[[nodiscard]] tl::expected<std::string, GitError>
last_commit_hash(const std::string& repo_path);

// ============================================================================
// Diff operations (REQ-GIT-1: diff view for focused file in Stage)
// ============================================================================

/// Return the diff output for a specific file.
/// Equivalent to `git diff -- <file_path>`.
[[nodiscard]] tl::expected<GitDiff, GitError>
diff_file(const std::string& repo_path, std::string_view file_path);

/// Return the unified diff for all modified files.
/// Equivalent to `git diff`.
[[nodiscard]] tl::expected<std::string, GitError>
diff_all(const std::string& repo_path);

}  // namespace traveler::git
