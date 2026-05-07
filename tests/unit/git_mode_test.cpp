// GATE-P0-2 Git Mode Unit Tests (PC-8 scripted evidence)
// Reference: Traveler_Phase0_Spec_v0.1.md §6.7 REQ-GIT-1, REQ-GIT-3
// Reference: Traveler_Phase0_Spec_v0.1.md PC-8
//
// IMPORTANT: All tests operate in a temporary git repo created by this test.
// They MUST NOT mutate the Traveler repository itself.
// Non-interactive git commands only.
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef TRAVELER_PROJECT_DIR
#define TRAVELER_PROJECT_DIR fs::current_path()
#endif

#include "git/backend.h"

namespace fs = std::filesystem;
using namespace traveler::git;

static int g_passed = 0;
static int g_failed = 0;

#define TEST_RUN(name, expr) do { \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << name << std::endl; \
        g_failed++; \
    } else { \
        std::cout << "  PASS: " << name << std::endl; \
        g_passed++; \
    } \
} while(0)

// ============================================================================
// Test helpers
// ============================================================================

/// Create a file inside the temp repo and write content
static void write_file(const fs::path& dir, const std::string& filename,
                        const std::string& content) {
    std::ofstream f(dir / filename);
    f << content;
}

/// Run a command in the temp repo directory, return exit code
static int run_in_repo(const fs::path& dir, const std::string& cmd) {
    std::string full_cmd = "cd \"" + dir.string() + "\" && " + cmd + " 2>&1";
    return std::system(full_cmd.c_str());
}

/// Set up a temporary git repo. Returns the path.
/// Caller must clean up.
static fs::path create_temp_repo(const std::string& test_name) {
    fs::path tmpdir = fs::temp_directory_path() / ("traveler_git_test_" + test_name);
    // Clean up any previous run residue
    if (fs::exists(tmpdir)) {
        fs::remove_all(tmpdir);
    }
    fs::create_directories(tmpdir);
    run_in_repo(tmpdir, "git init");
    // Configure git user for commit tests (needed in CI/containers without global config)
    run_in_repo(tmpdir, "git config user.email 'test@traveler.local'");
    run_in_repo(tmpdir, "git config user.name 'Traveler Test'");
    return tmpdir;
}

/// Clean up a temporary repo
static void cleanup_temp_repo(const fs::path& dir) {
    if (fs::exists(dir)) {
        fs::remove_all(dir);
    }
}

// ============================================================================
// Test cases
// ============================================================================

static void test_is_git_available() {
    std::cout << "\n--- Test: is_git_available() ---" << std::endl;
    TEST_RUN("git is available on PATH", is_git_available());
}

static void test_current_branch() {
    std::cout << "\n--- Test: current_branch() ---" << std::endl;
    auto repo = create_temp_repo("branch");

    // Default branch after git init is "main" or "master" depending on git version
    auto branch_result = current_branch(repo.string());
    TEST_RUN("current_branch succeeds", branch_result.has_value());

    if (branch_result) {
        std::string branch = *branch_result;
        TEST_RUN("current_branch is not empty", !branch.empty());
        std::cout << "    branch = \"" << branch << "\"" << std::endl;

        // Create a new branch and switch to it
        run_in_repo(repo, "git checkout -b test-feature");
        auto branch2 = current_branch(repo.string());
        TEST_RUN("current_branch after checkout = test-feature",
                 branch2.has_value() && *branch2 == "test-feature");
    }

    cleanup_temp_repo(repo);
}

static void test_list_branches() {
    std::cout << "\n--- Test: list_branches() ---" << std::endl;
    auto repo = create_temp_repo("list");

    // Create initial commit so we can branch
    write_file(repo, "README.md", "# Test");
    run_in_repo(repo, "git add README.md && git commit -m 'initial'");

    // Create a few branches
    run_in_repo(repo, "git branch feature-a");
    run_in_repo(repo, "git branch feature-b");

    auto branches = list_branches(repo.string());
    TEST_RUN("list_branches succeeds", branches.has_value());

    if (branches) {
        TEST_RUN("list_branches returns >= 3 branches (main + feature-a + feature-b)",
                 branches->size() >= 3);

        // Verify exactly one branch is marked current
        int current_count = 0;
        for (const auto& b : *branches) {
            if (b.is_current) current_count++;
        }
        TEST_RUN("exactly one branch is current (*)", current_count == 1);

        // Verify the current branch has is_current = true and name matches
        auto current = current_branch(repo.string());
        if (current) {
            bool found_current = false;
            for (const auto& b : *branches) {
                if (b.is_current) {
                    TEST_RUN("* branch name matches --show-current",
                             b.name == *current);
                    found_current = true;
                }
            }
            TEST_RUN("found the *-marked branch", found_current);
        }
    }

    cleanup_temp_repo(repo);
}

static void test_status_porcelain() {
    std::cout << "\n--- Test: status() — porcelain count match (PC-8) ---" << std::endl;
    auto repo = create_temp_repo("status");

    // Empty repo — no modified files
    {
        auto st = status(repo.string());
        TEST_RUN("status succeeds on empty repo", st.has_value());
        if (st) {
            // current_branch might be empty before first commit in some git versions
            TEST_RUN("modified_count == 0 on empty repo", st->modified_count == 0);
            TEST_RUN("untracked_count == 0 on empty repo", st->untracked_count == 0);
        }
    }

    // Create an untracked file
    write_file(repo, "untracked.txt", "hello");
    {
        auto st = status(repo.string());
        TEST_RUN("status after untracked file", st.has_value());
        if (st) {
            TEST_RUN("untracked_count == 1", st->untracked_count == 1);
            TEST_RUN("modified_count == 0 (untracked != modified)", st->modified_count == 0);
        }
    }

    // Stage and commit initial file
    write_file(repo, "committed.txt", "base");
    run_in_repo(repo, "git add committed.txt && git commit -m 'initial'");

    // Create a modified file
    write_file(repo, "modified.txt", "v1");
    run_in_repo(repo, "git add modified.txt && git commit -m 'add modified.txt'");
    write_file(repo, "modified.txt", "v2");  // modify after commit

    // Create a staged file
    write_file(repo, "staged.txt", "content");
    run_in_repo(repo, "git add staged.txt");

    // Create another untracked file
    write_file(repo, "untracked2.txt", "more");

    {
        auto st = status(repo.string());
        TEST_RUN("status succeeds with mixed state", st.has_value());
        if (st) {
            // git status --porcelain | wc -l should match modified_count
            // Porcelain lines: " M modified.txt", "A  staged.txt" (or "?? untracked2.txt")
            // Portable: use std::system + temp file + std::ifstream instead of popen/pclose
            // Temp file must be OUTSIDE the repo to avoid polluting git status output.
            int porcelain_count = -1;
            {
                fs::path porcelain_tmp = fs::temp_directory_path() /
                    (repo.filename().string() + "_porcelain.tmp");
                std::string dump_cmd = "git -C \"" + repo.string() +
                    "\" status --porcelain > \"" + porcelain_tmp.string() + "\"";
                if (std::system(dump_cmd.c_str()) == 0) {
                    std::ifstream ifs(porcelain_tmp);
                    if (ifs) {
                        std::string line;
                        int cnt = 0;
                        while (std::getline(ifs, line)) {
                            ++cnt;
                        }
                        porcelain_count = cnt;
                    }
                }
                std::error_code ec;
                fs::remove(porcelain_tmp, ec);
            }

            std::cout << "    git status --porcelain | wc -l = "
                      << porcelain_count << std::endl;
            std::cout << "    status.modified_count = " << st->modified_count << std::endl;
            std::cout << "    status.untracked_count = " << st->untracked_count << std::endl;
            std::cout << "    status.staged_count = " << st->staged_count << std::endl;

            // PC-8: modified count == git status --porcelain | wc -l
            // modified_count is our count of non-untracked porcelain lines
            // porcelain_count includes everything (modified + staged + untracked)
            int total_porcelain = st->modified_count + st->untracked_count;
            TEST_RUN("modified + untracked == porcelain total",
                     total_porcelain == porcelain_count);

            // Verify staged file is detected
            TEST_RUN("staged_count > 0", st->staged_count > 0);

            // Verify branch is set (after first commit)
            TEST_RUN("current_branch is populated", !st->current_branch.empty());
        }
    }

    cleanup_temp_repo(repo);
}

static void test_commit_and_log() {
    std::cout << "\n--- Test: commit() + last_commit_message() + last_commit_hash() (PC-8) ---" << std::endl;
    auto repo = create_temp_repo("commit");

    // Create and stage a file
    write_file(repo, "test.cpp", "int main() { return 0; }");
    run_in_repo(repo, "git add test.cpp");

    // Commit with a message
    auto result = commit(repo.string(), "feat: add test.cpp with main");
    TEST_RUN("commit succeeds", result.has_value());
    if (result) {
        TEST_RUN("commit.success is true", result->success);
        TEST_RUN("commit.commit_hash is not empty", !result->commit_hash.empty());
        std::cout << "    commit hash = " << result->commit_hash << std::endl;
    }

    // PC-8: Verify in git log -1
    auto msg = last_commit_message(repo.string());
    TEST_RUN("last_commit_message succeeds", msg.has_value());
    if (msg) {
        std::cout << "    last commit message = \"" << *msg << "\"" << std::endl;
        TEST_RUN("last_commit_message matches", *msg == "feat: add test.cpp with main");
    }

    auto hash = last_commit_hash(repo.string());
    TEST_RUN("last_commit_hash succeeds", hash.has_value());
    if (hash && result) {
        TEST_RUN("last_commit_hash matches commit result",
                 *hash == result->commit_hash);
    }

    // Second commit
    write_file(repo, "test.cpp", "int main() { return 42; }");
    run_in_repo(repo, "git add test.cpp");
    auto result2 = commit(repo.string(), "fix: change return value");
    TEST_RUN("second commit succeeds", result2.has_value() && result2->success);

    auto msg2 = last_commit_message(repo.string());
    TEST_RUN("second commit message observable in git log -1",
             msg2.has_value() && *msg2 == "fix: change return value");

    // Verify the new hash is different
    auto hash2 = last_commit_hash(repo.string());
    TEST_RUN("second commit hash differs from first",
             hash.has_value() && hash2.has_value() && *hash2 != *hash);

    cleanup_temp_repo(repo);
}

static void test_diff() {
    std::cout << "\n--- Test: diff_file() + diff_all() ---" << std::endl;
    auto repo = create_temp_repo("diff");

    // Create and commit a file
    write_file(repo, "hello.cpp", "#include <iostream>\nint main() { return 0; }\n");
    run_in_repo(repo, "git add hello.cpp && git commit -m 'initial'");

    // Modify it
    write_file(repo, "hello.cpp", "#include <iostream>\nint main() { return 42; }\n");

    // Test diff_all
    auto all_diff = diff_all(repo.string());
    TEST_RUN("diff_all succeeds", all_diff.has_value());
    if (all_diff) {
        TEST_RUN("diff_all is not empty (file modified)", !all_diff->empty());
        // Should contain the file path in the diff header
        TEST_RUN("diff_all mentions hello.cpp",
                 all_diff->find("hello.cpp") != std::string::npos);
    }

    // Test diff_file
    auto file_diff = diff_file(repo.string(), "hello.cpp");
    TEST_RUN("diff_file succeeds", file_diff.has_value());
    if (file_diff) {
        TEST_RUN("diff_file.file_path is hello.cpp", file_diff->file_path == "hello.cpp");
        TEST_RUN("diff_file.diff_content contains diff output",
                 !file_diff->diff_content.empty());
    }

    cleanup_temp_repo(repo);
}

static void test_error_handling() {
    std::cout << "\n--- Test: error handling ---" << std::endl;

    // Non-existent repo path
    auto branch = current_branch("/nonexistent/path/12345");
    TEST_RUN("current_branch on nonexistent path returns error", !branch.has_value());

    auto status_result = status("/nonexistent/path/12345");
    TEST_RUN("status on nonexistent path returns error", !status_result.has_value());

    auto commit_result = commit("/nonexistent/path/12345", "test");
    TEST_RUN("commit returns result (not expected-to-error, may succeed=false)",
             commit_result.has_value());
    if (commit_result) {
        TEST_RUN("commit on nonexistent path has success=false", !commit_result->success);
    }
}

static void test_temp_repo_isolation() {
    std::cout << "\n--- Test: isolation from Traveler repo ---" << std::endl;

    // Verify we're NOT inside the Traveler repo's git directory
    fs::path project_root(TRAVELER_PROJECT_DIR);
    TEST_RUN("TRAVELER_PROJECT_DIR exists", fs::exists(project_root));

    // Our temp repo path should NOT be the Traveler repo
    auto repo = create_temp_repo("isolation");
    bool is_different = (fs::canonical(repo) != fs::canonical(project_root));
    TEST_RUN("temp repo is NOT the Traveler project dir", is_different);

    // Verify the Traveler repo is untouched by checking its git log
    auto traveler_branch = current_branch(project_root.string());
    if (traveler_branch) {
        std::cout << "    Traveler repo current branch = " << *traveler_branch << std::endl;
        TEST_RUN("Traveler repo is still a valid git repo", true);

        // Verify important project files still exist
        TEST_RUN("Traveler repo xmake.lua still exists",
                 fs::exists(project_root / "xmake.lua"));
        TEST_RUN("Traveler repo src/git/backend.h still exists",
                 fs::exists(project_root / "src" / "git" / "backend.h"));
    } else {
        TEST_RUN("Traveler repo is a valid git repo (branch query succeeded)",
                 false);
    }

    cleanup_temp_repo(repo);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== GATE-P0-2 Git Mode Unit Tests (PC-8) ===" << std::endl;

    test_is_git_available();
    test_current_branch();
    test_list_branches();
    test_status_porcelain();
    test_commit_and_log();
    test_diff();
    test_error_handling();
    test_temp_repo_isolation();

    std::cout << "\n=== Results: " << g_passed << "/" << (g_passed + g_failed)
              << " PASS ===" << std::endl;

    return (g_failed == 0) ? 0 : 1;
}
