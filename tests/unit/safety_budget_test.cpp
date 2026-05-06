// GATE-P0-4 Cockpit Budget Enforcement Test (PC-17: TB-F boundary)
// Verifies that a synthetic callsign with snapshot > 256 KB:
//   1. Produces BudgetCheckResult with ok=false and detail mentioning exceed
//   2. Emits a CRITICAL safety event (visible in safety_events.jsonl)
//   3. Does NOT auto-unmount the callsign (callsign remains in mount table)
//
// Spec PC-17: "TB-F boundary: inject synthetic callsign with snapshot >
// 256 KB; verify CRITICAL safety event in safety_events.jsonl; verify
// callsign NOT auto-unmounted"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "cockpit/budget_check.h"
#include "cockpit/snapshot.h"
#include "cockpit/state.h"

namespace fs = std::filesystem;
using namespace traveler::cockpit;

static int g_passed = 0;
static int g_failed = 0;

#define TEST_RUN(name, expr) do {                                      \
    if (!(expr)) {                                                      \
        std::cerr << "  FAIL: " << name << std::endl;                   \
        g_failed++;                                                     \
    } else {                                                            \
        std::cout << "  PASS: " << name << std::endl;                   \
        g_passed++;                                                     \
    }                                                                   \
} while(0)

static std::string readFile(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

int main() {
    std::cout << "=== GATE-P0-4 Cockpit Budget Enforcement Test (PC-17) ==="
              << std::endl;

    // Set up isolated XDG_STATE_HOME for safety event file capture
    // Must happen before any emit_safety_event() call.
    fs::path tmpdir;
    for (int attempt = 0; attempt < 100; ++attempt) {
        std::string path =
            fs::temp_directory_path() /
            ("traveler_test_pc17_" + std::to_string(attempt));
        if (fs::create_directory(path)) {
            tmpdir = path;
            break;
        }
    }
    if (tmpdir.empty()) {
        std::cerr << "FATAL: could not create temp directory" << std::endl;
        return 1;
    }
    setenv("XDG_STATE_HOME", tmpdir.c_str(), 1);
    fs::path events_path = tmpdir / "traveler" / "safety_events.jsonl";
    std::cout << "  JSONL path: " << events_path << std::endl;

    // ======================================================================
    // Test 1: Enforce snapshot budget check with oversized snapshot
    // ======================================================================
    std::cout << "\n--- Test 1: check_snapshot_budget with >256 KB snapshot"
              << " ---" << std::endl;
    {
        CockpitSnapshot large_snap;
        // Fill model_name with 300 KB data to exceed 256 KB budget
        large_snap.model_name = std::string(300 * 1024, 'X');
        large_snap.last_message_head = "test message head";
        large_snap.status = "ready";
        large_snap.last_active_at = std::chrono::system_clock::now();

        std::size_t snap_bytes = snapshot_size_bytes(large_snap);
        std::size_t budget = CockpitSnapshot::max_snapshot_bytes();
        std::cout << "  Snapshot size: " << snap_bytes
                  << " bytes (budget: " << budget << ")" << std::endl;

        auto result = check_snapshot_budget(large_snap, "@vega");

        TEST_RUN("PC-17a: budget check fails for >256 KB snapshot",
                 !result.ok);
        TEST_RUN("PC-17b: detail mentions size exceeded",
                 result.detail.find("exceeds") != std::string::npos ||
                 result.detail.find("exceeded") != std::string::npos);
        TEST_RUN("PC-17c: detail mentions callsign @vega",
                 result.detail.find("@vega") != std::string::npos);
    }

    // ======================================================================
    // Test 2: Snapshot within budget passes
    // ======================================================================
    std::cout << "\n--- Test 2: check_snapshot_budget with small snapshot"
              << " ---" << std::endl;
    {
        CockpitSnapshot small_snap;
        small_snap.model_name = "test-model-v1";
        small_snap.last_message_head = "short message";
        small_snap.status = "ready";
        small_snap.last_active_at = std::chrono::system_clock::now();

        auto result = check_snapshot_budget(small_snap, "@altair");
        TEST_RUN("PC-17d: budget check passes for small snapshot",
                 result.ok);
    }

    // ======================================================================
    // Test 3: check_cockpit_budget with over-budget snapshot in mount table
    // ======================================================================
    std::cout << "\n--- Test 3: check_cockpit_budget with oversized mount"
              << " ---" << std::endl;
    {
        CockpitState state;

        // Create oversized snapshot
        CockpitSnapshot large_snap;
        large_snap.model_name = std::string(300 * 1024, 'X');
        large_snap.last_message_head = "head";
        large_snap.status = "ready";
        large_snap.last_active_at = std::chrono::system_clock::now();

        // Insert as mounted entry for @vega
        MountedEntry entry;
        entry.status = CallsignStatus::Snapshot;
        entry.snapshot = large_snap;
        entry.mounted_at = std::chrono::system_clock::now();
        state.mounts["@vega"] = std::move(entry);

        // Run budget check
        auto result = check_cockpit_budget(state, 0);

        TEST_RUN("PC-17e: cockpit budget check fails for oversized mount",
                 !result.ok);
        TEST_RUN("PC-17f: detail describes violation",
                 result.detail.find("exceeds") != std::string::npos ||
                 result.detail.find("snapshot") != std::string::npos);

        // PC-17 spec: "verify callsign NOT auto-unmounted"
        // Budget check is read-only; it does not modify state.mounts.
        TEST_RUN("PC-17g: callsign still mounted after budget check",
                 state.mounts.find("@vega") != state.mounts.end());
        TEST_RUN("PC-17h: mount table unchanged (size = 1)",
                 state.mounts.size() == 1);
    }

    // ======================================================================
    // Test 4: Verify CRITICAL safety event was emitted to JSONL
    // ======================================================================
    std::cout << "\n--- Test 4: Verify CRITICAL event in safety_events.jsonl"
              << " ---" << std::endl;
    {
        if (fs::exists(events_path)) {
            std::string content = readFile(events_path);
            TEST_RUN("PC-17i: safety_events.jsonl is non-empty",
                     !content.empty());

            // Verify severity field is CRITICAL
            TEST_RUN("PC-17j: severity field present",
                     content.find("\"severity\"") != std::string::npos);
            TEST_RUN("PC-17k: severity is CRITICAL",
                     content.find("CRITICAL") != std::string::npos);

            // Verify category field is BUDGET
            TEST_RUN("PC-17l: category field present",
                     content.find("\"category\"") != std::string::npos);
            TEST_RUN("PC-17m: category is BUDGET",
                     content.find("BUDGET") != std::string::npos);

            // Verify callsign referenced in detail
            TEST_RUN("PC-17n: detail contains @vega",
                     content.find("@vega") != std::string::npos);

            std::cout << "  JSONL content:\n" << content << std::endl;
        } else {
            // CRITICAL event may have been written to non-test path if
            // XDG_STATE_HOME was already checked before setenv(). This is
            // a build-order concern, not a logic failure.
            std::cout << "  WARN: safety_events.jsonl not found at "
                      << events_path << std::endl;
            std::cout << "  (This may occur if ensure_path() was initialized"
                      << " before setenv() in a prior test binary.)"
                      << std::endl;
            // Still fail — the spec requires JSONL evidence
            TEST_RUN("PC-17i: safety_events.jsonl found at expected path",
                     false);
        }
    }

    // ======================================================================
    // Test 5: Full state budget (focused callsign within 16 MB budget)
    // ======================================================================
    std::cout << "\n--- Test 5: check_full_state_budget ---" << std::endl;
    {
        std::size_t budget = CockpitSnapshot::max_focused_state_bytes();

        // Within budget
        auto result_ok = check_full_state_budget(budget / 2, "@vega");
        TEST_RUN("PC-17o: full state within 16 MB passes",
                 result_ok.ok);

        // Over budget
        auto result_fail = check_full_state_budget(budget + 1, "@vega");
        TEST_RUN("PC-17p: full state exceeding 16 MB fails",
                 !result_fail.ok);
    }

    // Cleanup temp directory
    std::error_code ec;
    fs::remove_all(tmpdir, ec);

    // Summary
    std::cout << "\n=== Summary: " << g_passed << " passed, "
              << g_failed << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
