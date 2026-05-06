// WAVE-A Lane A5: Cockpit Test Harness — P0-5 PC-1 through PC-5
// Reference: Traveler_Phase0_Spec_v0.1.md §9 Pass Criteria 1-5
// Reference: briefs/reconcile-p0-5-2026-05-06.md
//
// Tests cockpit state machine (mount / focus / snapshot / budget / bg_stream)
// without real provider calls, llama downloads, or TUI rendering.
// If a PC cannot PASS without provider threading/offline assets, this is noted
// as a remaining GAP in the final brief.
//
// Compile requirements (A6 xmake registration — NOT added here, see brief):
//   target("test_cockpit")
//       set_kind("binary")
//       add_includedirs("src")
//       add_files("tests/unit/cockpit_test.cpp")
//       add_files("src/cockpit/state.cpp")
//       add_files("src/cockpit/mount.cpp")
//       add_files("src/cockpit/snapshot.cpp")
//       add_files("src/cockpit/budget_check.cpp")
//       add_files("src/cockpit/bg_stream.cpp")
//       add_files("src/safety/event.cpp")
//       add_packages("tl_expected")
//       set_group("test")

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "cockpit/bg_stream.h"
#include "cockpit/budget_check.h"
#include "cockpit/mount.h"
#include "cockpit/snapshot.h"
#include "cockpit/state.h"

using namespace traveler::cockpit;

// ============================================================================
// Minimal test harness (same pattern as existing tests/unit/*_test.cpp)
// ============================================================================
static int g_passed = 0;
static int g_failed = 0;

#define TEST(name, expr)                                              \
    do {                                                               \
        if (!(expr)) {                                                 \
            std::cerr << "  FAIL: " << (name) << std::endl;            \
            g_failed++;                                                \
        } else {                                                       \
            std::cout << "  PASS: " << (name) << std::endl;            \
            g_passed++;                                                \
        }                                                              \
    } while (0)

// ============================================================================
// PC-1: Cloud mode mounts @vega/@altair/@orion/@rigel without error
// Spec §9 line 1333: "Cockpit mounts @vega, @altair, @orion, @rigel
// (4 callsigns) in cloud mode without error"
// ============================================================================
static void test_pc1_cloud_mount_4_callsigns() {
    std::cout << "\n--- PC-1: Cloud mode mounts 4 callsigns ---" << std::endl;

    CockpitState state;
    state.cloud_mode = true;

    // Mount @vega (also becomes auto-focused as first mount)
    auto r1 = mount(state, "@vega");
    TEST("PC1a: mount @vega succeeds", r1.has_value());
    TEST("PC1b: @vega focused (auto-focus first mount)",
         state.focused.has_value() && *state.focused == "@vega");
    TEST("PC1c: mount_count = 1 after @vega", mount_count(state) == 1);

    // Mount @altair
    auto r2 = mount(state, "@altair");
    TEST("PC1d: mount @altair succeeds", r2.has_value());
    TEST("PC1e: mount_count = 2 after @altair", mount_count(state) == 2);

    // Mount @orion
    auto r3 = mount(state, "@orion");
    TEST("PC1f: mount @orion succeeds", r3.has_value());
    TEST("PC1g: mount_count = 3 after @orion", mount_count(state) == 3);

    // Mount @rigel (4th — max in cloud mode)
    auto r4 = mount(state, "@rigel");
    TEST("PC1h: mount @rigel succeeds", r4.has_value());
    TEST("PC1i: mount_count = 4 after @rigel", mount_count(state) == 4);

    // Verify all 4 are mounted
    TEST("PC1j: @vega is mounted", is_mounted(state, "@vega"));
    TEST("PC1k: @altair is mounted", is_mounted(state, "@altair"));
    TEST("PC1l: @orion is mounted", is_mounted(state, "@orion"));
    TEST("PC1m: @rigel is mounted", is_mounted(state, "@rigel"));

    // Verify max_callsigns = 4 in cloud mode
    TEST("PC1n: max_callsigns = 4 in cloud mode", max_callsigns(state) == 4);

    // Verify still exactly one focused
    TEST("PC1o: exactly one focused after 4 mounts",
         state.focused.has_value());

    // Verify mounted_callsigns() returns all 4
    auto mounted = mounted_callsigns(state);
    TEST("PC1p: mounted_callsigns() count = 4", mounted.size() == 4);
}

// ============================================================================
// PC-2: 5th mount returns exact error without crash
// Spec §9 line 1334: "Attempting to mount a 5th callsign in cloud mode
// surfaces error 'Maximum 4 callsigns in cloud mode' without crashing"
// ============================================================================
static void test_pc2_fifth_mount_error() {
    std::cout << "\n--- PC-2: 5th mount returns exact error ---" << std::endl;

    CockpitState state;
    state.cloud_mode = true;

    // Mount 4 callsigns
    mount(state, "@vega");
    mount(state, "@altair");
    mount(state, "@orion");
    mount(state, "@rigel");
    TEST("PC2a: 4 mounts succeeded", mount_count(state) == 4);

    // Attempt 5th mount with a non-roster name (should still hit limit)
    auto r5 = mount(state, "test5");
    TEST("PC2b: 5th mount returns error", !r5.has_value());
    TEST("PC2c: error message matches Spec",
         r5.error().message == "Maximum 4 callsigns in cloud mode");
    TEST("PC2d: mount_count still 4 after rejected 5th",
         mount_count(state) == 4);

    // Verify no crash (we got here)
    TEST("PC2e: no crash on 5th mount attempt", true);

    // Also test duplicate mount rejection
    auto r_dup = mount(state, "@vega");
    TEST("PC2f: duplicate @vega mount returns error", !r_dup.has_value());
    TEST("PC2g: mount_count unchanged after duplicate reject",
         mount_count(state) == 4);
}

// ============================================================================
// PC-3: Focus switch captures/restores state + timing harness
// Spec §9 line 1335: "Focus-switch from @vega to @altair: previous focus-state
// captured, new focus-state restored, Stage re-renders within 200 ms median
// (100 trials)"
//
// This test validates the state machine. Actual <200ms latency requires TUI
// rendering integration (GAP — see brief).
// ============================================================================
static void test_pc3_focus_switch_and_timing() {
    std::cout << "\n--- PC-3: Focus switch captures/restores state ---" << std::endl;

    // --- Sub-test A: State machine correctness ---
    {
        CockpitState state;
        state.cloud_mode = true;

        // Mount 2 callsigns
        mount(state, "@vega");   // auto-focused
        mount(state, "@altair");

        TEST("PC3a: @vega focused initially",
             state.focused.has_value() && *state.focused == "@vega");
        TEST("PC3b: @vega status = Focused",
             callsign_status(state, "@vega") == CallsignStatus::Focused);
        TEST("PC3c: @altair status = Snapshot",
             callsign_status(state, "@altair") == CallsignStatus::Snapshot);

        // Focus switch: @vega → @altair
        auto r_focus = focus(state, "@altair");
        TEST("PC3d: focus @altair succeeds", r_focus.has_value());
        TEST("PC3e: @altair now focused",
             state.focused.has_value() && *state.focused == "@altair");
        TEST("PC3f: @altair status = Focused after focus",
             callsign_status(state, "@altair") == CallsignStatus::Focused);
        TEST("PC3g: @vega status = Snapshot after focus switch (captured)",
             callsign_status(state, "@vega") == CallsignStatus::Snapshot);

        // Focus back: @altair → @vega
        auto r_focus2 = focus(state, "@vega");
        TEST("PC3h: focus back to @vega succeeds", r_focus2.has_value());
        TEST("PC3i: @vega focused again",
             state.focused.has_value() && *state.focused == "@vega");
        TEST("PC3j: @altair back to Snapshot",
             callsign_status(state, "@altair") == CallsignStatus::Snapshot);

        // Focus on already-focused is no-op (not an error)
        auto r_same = focus(state, "@vega");
        TEST("PC3k: focus already-focused is no-op (returns value)",
             r_same.has_value());

        // Focus non-mounted callsign returns error
        auto r_bad = focus(state, "@rigel");
        TEST("PC3l: focus non-mounted returns error", !r_bad.has_value());
        TEST("PC3m: @vega still focused after failed focus",
             state.focused.has_value() && *state.focused == "@vega");
    }

    // --- Sub-test B: Timing harness (micro-benchmark) ---
    // Measures focus() call overhead only (no TUI rendering).
    // True <200ms median requires TUI rendering pipeline (GAP).
    {
        std::cout << "\n  [Timing harness: focus-switch overhead, N=100]" << std::endl;

        CockpitState state;
        state.cloud_mode = true;
        mount(state, "@vega");
        mount(state, "@altair");

        constexpr int N = 100;
        std::vector<double> latencies_us;
        latencies_us.reserve(N);

        for (int i = 0; i < N; ++i) {
            // Toggle focus between @vega and @altair
            auto t0 = std::chrono::high_resolution_clock::now();
            if (i % 2 == 0) {
                focus(state, "@altair");
            } else {
                focus(state, "@vega");
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            latencies_us.push_back(
                std::chrono::duration<double, std::micro>(t1 - t0).count());
        }

        // Compute stats
        double min_us = latencies_us[0];
        double max_us = latencies_us[0];
        double sum_us = 0.0;
        for (auto v : latencies_us) {
            if (v < min_us) min_us = v;
            if (v > max_us) max_us = v;
            sum_us += v;
        }
        double avg_us = sum_us / N;

        // Sort for median
        std::sort(latencies_us.begin(), latencies_us.end());
        double median_us = (latencies_us[N / 2 - 1] + latencies_us[N / 2]) / 2.0;

        std::cout << "    N=" << N
                  << ", min=" << std::fixed << std::setprecision(1) << min_us << " us"
                  << ", median=" << median_us << " us"
                  << ", avg=" << avg_us << " us"
                  << ", max=" << max_us << " us" << std::endl;

        // State-machine overhead should be well under 200ms
        TEST("PC3n: median focus() overhead < 200000 us (200ms)",
             median_us < 200000.0);
        TEST("PC3o: max focus() overhead < 500000 us (500ms)",
             max_us < 500000.0);

        // Record: focus-switch state machine is fast; actual <200ms with
        // TUI rendering needs GATE-P0-2 layout integration (GAP).
    }

    // --- Sub-test C: Unmount with focus transfer ---
    {
        CockpitState state;
        state.cloud_mode = true;
        mount(state, "@vega");
        mount(state, "@altair");
        mount(state, "@orion");

        // Focus @altair first
        focus(state, "@altair");
        TEST("PC3p: @altair focused before unmount",
             state.focused.has_value() && *state.focused == "@altair");

        // Unmount @altair (focused) → auto-focus another
        auto r_unmount = unmount(state, "@altair");
        TEST("PC3q: unmount focused @altair succeeds", r_unmount.has_value());
        TEST("PC3r: @altair no longer mounted",
             !is_mounted(state, "@altair"));
        TEST("PC3s: auto-focus transferred to remaining callsign",
             state.focused.has_value() && *state.focused != "@altair");
        TEST("PC3t: mount_count = 2 after unmount",
             mount_count(state) == 2);
    }
}

// ============================================================================
// PC-4: Background stream tracker state checks
// Spec §9 line 1336: Background streaming state tracking.
// Tests BackgroundStreamer without real provider threads (Phase 0: tracker only).
// PC-4 full PASS requires provider threading + TUI strip integration (GAP).
// ============================================================================
static void test_pc4_background_streamer() {
    std::cout << "\n--- PC-4: Background streamer state tracker ---" << std::endl;

    BackgroundStreamer bs;

    // Initially no streams running
    TEST("PC4a: no streams initially", !bs.has_running());
    TEST("PC4b: unknown callsign status = Ready",
         bs.status("@vega") == BgStreamStatus::Ready);

    // Start a stream
    bs.start("@vega");
    TEST("PC4c: @vega is running after start", bs.is_running("@vega"));
    TEST("PC4d: @vega status = Running",
         bs.status("@vega") == BgStreamStatus::Running);
    TEST("PC4e: has_running() true", bs.has_running());

    // Start another stream
    bs.start("@altair");
    TEST("PC4f: @altair is running after start", bs.is_running("@altair"));
    TEST("PC4g: @vega still running", bs.is_running("@vega"));

    // Mark @vega done
    bs.mark_done("@vega");
    TEST("PC4h: @vega status = Done",
         bs.status("@vega") == BgStreamStatus::Done);
    TEST("PC4i: @vega not running after done", !bs.is_running("@vega"));
    TEST("PC4j: has_running() still true (@altair running)", bs.has_running());

    // Mark unknown callsign — no-op, not an error
    bs.mark_done("@rigel");
    TEST("PC4k: mark_done on unknown callsign is safe (no crash)", true);

    // Mark @altair error
    bs.mark_error("@altair");
    TEST("PC4l: @altair status = Error",
         bs.status("@altair") == BgStreamStatus::Error);
    TEST("PC4m: no streams running after both done/error", !bs.has_running());

    // Shutdown with running streams
    bs.start("@orion");
    bs.start("@rigel");
    int interrupt_count = 0;
    bs.shutdown([&interrupt_count](const std::string& cs) {
        interrupt_count++;
        (void)cs;
    });
    TEST("PC4n: shutdown interrupted 2 running streams", interrupt_count == 2);
    TEST("PC4o: @orion status = Interrupted after shutdown",
         bs.status("@orion") == BgStreamStatus::Interrupted);
    TEST("PC4p: @rigel status = Interrupted after shutdown",
         bs.status("@rigel") == BgStreamStatus::Interrupted);
    TEST("PC4q: has_running() false after shutdown", !bs.has_running());

    // to_string coverage
    TEST("PC4r: to_string Ready = 'ready'",
         std::string(to_string(BgStreamStatus::Ready)) == "ready");
    TEST("PC4s: to_string Running = 'running'",
         std::string(to_string(BgStreamStatus::Running)) == "running");
    TEST("PC4t: to_string Done = 'done'",
         std::string(to_string(BgStreamStatus::Done)) == "done");
    TEST("PC4u: to_string Error = 'error'",
         std::string(to_string(BgStreamStatus::Error)) == "error");
    TEST("PC4v: to_string Interrupted = 'interrupted'",
         std::string(to_string(BgStreamStatus::Interrupted)) == "interrupted");

    // PC-4 GAP note: full PASS requires provider threading + TUI strip integration
}

// ============================================================================
// PC-5: Snapshot budget checks (≤256 KB per snapshot, ≤16 MB full state)
// Spec §9 line 1337: "Per-callsign snapshot size ≤ 256 KB measured via
// instrumented build; focused full-state size ≤ 16 MB"
// ============================================================================
static void test_pc5_snapshot_budget() {
    std::cout << "\n--- PC-5: Snapshot budget checks ---" << std::endl;

    // --- Sub-test A: Snapshot within budget ---
    {
        // A small snapshot should easily fit
        auto snap = make_default_snapshot("@vega");
        TEST("PC5a: default snapshot within budget",
             is_snapshot_within_budget(snap));

        auto result = check_snapshot_budget(snap, "@vega");
        TEST("PC5b: check_snapshot_budget returns ok for small snapshot",
             result.ok);
    }

    // --- Sub-test B: Snapshot at boundary ---
    {
        CockpitSnapshot snap;
        snap.model_name = "@vega";
        snap.status = "ready";
        snap.last_active_at = std::chrono::system_clock::now();

        // Build a last_message_head near but within the 256 KB budget.
        // 256 KB = 262144 bytes. The struct overhead is ~80 bytes (sizeof),
        // model_name + status ~ 13 bytes. So we can safely use ~262000 chars.
        // Each char in std::string is 1 byte; capacity may be larger.
        // We adjust empirically to stay under 256 KB total.
        constexpr std::size_t target_chars = 260000;  // safely under
        snap.last_message_head = std::string(target_chars, 'x');

        TEST("PC5c: near-budget snapshot fits",
             is_snapshot_within_budget(snap));

        auto result = check_snapshot_budget(snap, "@vega");
        TEST("PC5d: check_snapshot_budget ok at boundary", result.ok);
    }

    // --- Sub-test C: Snapshot exceeding budget ---
    {
        CockpitSnapshot snap;
        snap.model_name = "@vega";
        snap.status = "ready";
        snap.last_active_at = std::chrono::system_clock::now();

        // Create a snapshot that exceeds 256 KB.
        // 256 KB = 262144 bytes. Use 270000 chars to force overflow.
        snap.last_message_head = std::string(270000, 'y');

        TEST("PC5e: oversized snapshot NOT within budget",
             !is_snapshot_within_budget(snap));

        auto result = check_snapshot_budget(snap, "@vega");
        TEST("PC5f: check_snapshot_budget reports violation", !result.ok);
        TEST("PC5g: violation detail contains 'exceeds 256 KB budget'",
             result.detail.find("exceeds 256 KB budget") != std::string::npos);
    }

    // --- Sub-test D: Full state budget (≤16 MB) ---
    {
        // Within budget (1 MB)
        auto result_ok = check_full_state_budget(1 * 1024 * 1024, "@vega");
        TEST("PC5h: 1 MB full state within budget", result_ok.ok);

        // Within budget (15 MB)
        auto result_15mb = check_full_state_budget(15 * 1024 * 1024, "@vega");
        TEST("PC5i: 15 MB full state within budget", result_15mb.ok);

        // At boundary (exactly 16 MB)
        auto result_16mb = check_full_state_budget(16 * 1024 * 1024, "@vega");
        TEST("PC5j: exactly 16 MB full state within budget", result_16mb.ok);

        // Exceeding budget (17 MB)
        auto result_over = check_full_state_budget(17 * 1024 * 1024, "@vega");
        TEST("PC5k: 17 MB full state exceeds budget", !result_over.ok);
        TEST("PC5l: violation detail contains 'exceeds 16 MB budget'",
             result_over.detail.find("exceeds 16 MB budget") != std::string::npos);
    }

    // --- Sub-test E: Cockpit-wide budget check ---
    {
        CockpitState state;
        state.cloud_mode = true;

        // Mount 2 callsigns with small snapshots and a reasonable focused state
        mount(state, "@vega");
        mount(state, "@altair");

        // Small focused state (will pass all checks)
        auto result_pass = check_cockpit_budget(state, 100 * 1024);
        TEST("PC5m: cockpit budget check passes for small state",
             result_pass.ok);

        // Large focused state (but still within 16 MB)
        auto result_ok2 = check_cockpit_budget(state, 15 * 1024 * 1024);
        TEST("PC5n: cockpit budget check passes for 15 MB focused state",
             result_ok2.ok);
    }

    // --- Sub-test F: Snapshot size_bytes helper ---
    {
        auto snap = make_default_snapshot("@vega");
        std::size_t sz = snapshot_size_bytes(snap);
        TEST("PC5o: snapshot_size_bytes returns non-zero", sz > 0);
        TEST("PC5p: snapshot_size_bytes at least sizeof(CockpitSnapshot)",
             sz >= sizeof(CockpitSnapshot));
    }

    // --- Sub-test G: Constants ---
    {
        TEST("PC5q: max_snapshot_bytes = 256 KB",
             CockpitSnapshot::max_snapshot_bytes() == 256 * 1024);
        TEST("PC5r: max_focused_state_bytes = 16 MB",
             CockpitSnapshot::max_focused_state_bytes() == 16 * 1024 * 1024);
        TEST("PC5s: max_message_head = 256 chars",
             CockpitSnapshot::max_message_head() == 256);
    }
}

// ============================================================================
// Additional: Mount/unmount lifecycle edge cases
// ============================================================================
static void test_mount_lifecycle_edge_cases() {
    std::cout << "\n--- Mount lifecycle edge cases ---" << std::endl;

    // --- Unmount last callsign (no auto-focus possible) ---
    {
        CockpitState state;
        state.cloud_mode = true;
        mount(state, "@vega");
        TEST("EDGEa: @vega mounted", is_mounted(state, "@vega"));

        auto r = unmount(state, "@vega");
        TEST("EDGEb: unmount @vega succeeds", r.has_value());
        TEST("EDGEc: @vega not mounted after unmount",
             !is_mounted(state, "@vega"));
        TEST("EDGEd: no focused callsign after last unmount",
             !state.focused.has_value());
        TEST("EDGEe: mount_count = 0 after last unmount",
             mount_count(state) == 0);
    }

    // --- Unmount non-focused callsign (focus stays) ---
    {
        CockpitState state;
        state.cloud_mode = true;
        mount(state, "@vega");    // auto-focused
        mount(state, "@altair");  // snapshot

        auto r = unmount(state, "@altair");
        TEST("EDGEf: unmount non-focused @altair succeeds", r.has_value());
        TEST("EDGEg: @vega still focused", state.focused == "@vega");
        TEST("EDGEh: mount_count = 1", mount_count(state) == 1);
    }

    // --- Unmount non-mounted callsign ---
    {
        CockpitState state;
        state.cloud_mode = true;
        auto r = unmount(state, "@vega");
        TEST("EDGEi: unmount non-mounted returns error", !r.has_value());
        TEST("EDGEj: error message mentions 'not mounted'",
             r.error().message.find("not mounted") != std::string::npos);
    }

    // --- Mount already-mounted callsign ---
    {
        CockpitState state;
        state.cloud_mode = true;
        mount(state, "@vega");
        auto r = mount(state, "@vega");
        TEST("EDGEk: duplicate mount returns error", !r.has_value());
        TEST("EDGEl: error message mentions 'already mounted'",
             r.error().message.find("already mounted") != std::string::npos);
        TEST("EDGEm: mount_count = 1 (unchanged)", mount_count(state) == 1);
    }

    // --- Offline mode: mount limit = 1 ---
    {
        CockpitState state;
        state.cloud_mode = false;

        TEST("EDGEn: max_callsigns = 1 in offline mode",
             max_callsigns(state) == 1);

        auto r1 = mount(state, "@vega");
        TEST("EDGEo: mount @vega in offline mode succeeds", r1.has_value());

        auto r2 = mount(state, "@altair");
        TEST("EDGEp: mount 2nd in offline mode returns error", !r2.has_value());
        TEST("EDGEq: offline error matches Spec ('Offline mode supports...')",
             r2.error().message.find("Offline mode supports a single callsign") !=
                 std::string::npos);
    }

    // --- to_string coverage ---
    TEST("EDGEr: to_string Unmounted", std::string(to_string(CallsignStatus::Unmounted)) == "unmounted");
    TEST("EDGEs: to_string Snapshot", std::string(to_string(CallsignStatus::Snapshot)) == "snapshot");
    TEST("EDGEt: to_string Focused", std::string(to_string(CallsignStatus::Focused)) == "focused");
}

// ============================================================================
// Additional: Snapshot capture and format_seat
// ============================================================================
static void test_snapshot_and_seat_formatting() {
    std::cout << "\n--- Snapshot capture and seat formatting ---" << std::endl;

    // --- capture_snapshot truncation ---
    {
        std::string long_msg(500, 'A');
        auto snap = capture_snapshot("@vega", long_msg, "running");

        TEST("SNAPa: model_name = @vega", snap.model_name == "@vega");
        TEST("SNAPb: status = running", snap.status == "running");
        TEST("SNAPc: message_head truncated to 256",
             snap.last_message_head.size() == 256);
        TEST("SNAPd: message_head is first 256 chars ('A')",
             snap.last_message_head == std::string(256, 'A'));
        TEST("SNAPe: last_active_at is set (not epoch)",
             snap.last_active_at != std::chrono::system_clock::time_point{});
    }

    // --- capture_snapshot with short message ---
    {
        auto snap = capture_snapshot("@altair", "short", "ready");
        TEST("SNAPf: message_head = 'short' (no truncation)",
             snap.last_message_head == "short");
        TEST("SNAPg: status = ready", snap.status == "ready");
    }
}

// ============================================================================
// main
// ============================================================================
int main() {
    std::cout << "=== WAVE-A Lane A5: Cockpit Test Harness (P0-5 PC-1〜PC-5) ==="
              << std::endl;

    test_pc1_cloud_mount_4_callsigns();
    test_pc2_fifth_mount_error();
    test_pc3_focus_switch_and_timing();
    test_pc4_background_streamer();
    test_pc5_snapshot_budget();
    test_mount_lifecycle_edge_cases();
    test_snapshot_and_seat_formatting();

    std::cout << "\n=== Summary: " << g_passed << " passed, " << g_failed
              << " failed ===" << std::endl;

    return g_failed > 0 ? 1 : 0;
}
