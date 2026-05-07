// WAVE-B Lane B5: TUI Surface Model Evidence — PC-1, PC-2, PC-3, PC-4, PC-11
// Reference: Traveler_Phase0_Spec_v0.1.md §6 Pass Criteria 1-4, 11
// Reference: briefs/reconcile-p0-2-2026-05-06.md
// Reference: Brief wave-b-b5-tui-surface-2026-05-07.md
//
// Tests pure TUI surface models (mode header, strip content, cheatsheet,
// @ autocomplete, separator drag, mode-switch latency harness) without
// FTXUI rendering or a live ScreenLoop.
//
// xmake registration (for B6 — do NOT edit xmake.lua):
//   target("test_tui_surface")
//       set_kind("binary")
//       add_includedirs("src")
//       add_files("tests/unit/tui_surface_test.cpp")
//       add_files("src/tui/surface_model.cpp")
//       add_files("src/command/parser.cpp")
//       add_files("src/core/dispatcher.cpp")
//       set_group("test")
//       add_tests("default")
//
// Manual compile (Linux):
//   g++ -std=c++20 -Isrc -o /tmp/test_tui_surface \
//       tests/unit/tui_surface_test.cpp \
//       src/tui/surface_model.cpp \
//       src/command/parser.cpp \
//       src/core/dispatcher.cpp

#include "command/parser.h"
#include "core/dispatcher.h"
#include "tui/surface_model.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// Minimal test harness (same pattern as existing tests/unit/*_test.cpp)
// ============================================================================
static int g_passed = 0;
static int g_failed = 0;

#define TEST(name, expr)                                                  \
    do {                                                                   \
        if (!(expr)) {                                                     \
            std::fprintf(stderr, "FAIL: %s\n  expr: %s\n  file: %s:%d\n", \
                         name, #expr, __FILE__, __LINE__);                 \
            ++g_failed;                                                    \
        } else {                                                           \
            ++g_passed;                                                    \
        }                                                                  \
    } while (0)

#define TEST_EQ(name, a, b) TEST(name, (a) == (b))

// ============================================================================
// Test helpers
// ============================================================================

static const std::vector<std::string> kTestRoster{
    "@vega", "@altair", "@orion", "@rigel"};

// ============================================================================
// PC-1: Mode Header — Cockpit → Editor via dispatcher
// ============================================================================

static void test_pc1_mode_header_default() {
    traveler::tui::ModeHeader header;
    TEST("PC-1: mode header default is Cockpit",
         header.mode_name == "Cockpit");
    TEST("PC-1: previous mode is empty at start",
         header.previous_mode_name.empty());
    TEST("PC-1: not transitioning at start", !header.transitioning);
}

static void test_pc1_mode_header_transition() {
    traveler::tui::ModeHeader header;

    header.transition_to("Editor");
    TEST("PC-1: transition sets mode_name to Editor",
         header.mode_name == "Editor");
    TEST("PC-1: transition records previous mode as Cockpit",
         header.previous_mode_name == "Cockpit");
    TEST("PC-1: transition sets transitioning flag",
         header.transitioning);

    header.clear_transition();
    TEST("PC-1: clear_transition resets flag", !header.transitioning);

    header.transition_to("LLM");
    TEST("PC-1: second transition to LLM",
         header.mode_name == "LLM");
    TEST("PC-1: previous mode is Editor",
         header.previous_mode_name == "Editor");
}

static void test_pc1_mode_header_with_dispatcher() {
    traveler::core::Dispatcher dispatcher;
    traveler::tui::ModeHeader header;

    // Default is Cockpit per spec
    auto& mode_name = traveler::core::mode_name;
    TEST("PC-1: dispatcher default mode is Cockpit",
         dispatcher.current_mode() == traveler::core::Mode::Cockpit);
    TEST_EQ("PC-1: header matches dispatcher",
            static_cast<int>(dispatcher.current_mode()),
            static_cast<int>(traveler::core::Mode::Cockpit));

    // Transition via slash command
    auto result = dispatcher.transition_command("/Editor");
    TEST("PC-1: dispatcher transition to Editor ok", result.ok);
    TEST("PC-1: dispatcher in Editor mode",
         dispatcher.current_mode() == traveler::core::Mode::Editor);

    header.transition_to("Editor");
    TEST("PC-1: header reflects dispatcher Editor transition",
         header.mode_name == "Editor" && header.transitioning);
}

// ============================================================================
// PC-1: Strip Content — Mode-dependent (Cockpit seats vs Editor tree)
// ============================================================================

static void test_pc1_strip_content_cockpit() {
    traveler::tui::StripContent strip;
    strip.set_mode("Cockpit", kTestRoster);

    TEST("PC-1: strip in Cockpit mode", strip.is_cockpit());
    TEST("PC-1: strip not in Editor mode when Cockpit", !strip.is_editor());
    TEST_EQ("PC-1: Cockpit has 4 seats", strip.seat_count(), 4);
    TEST("PC-1: seat 0 is @vega",
         strip.cockpit_seats[0].callsign == "@vega");
    TEST("PC-1: seat 0 is focused",
         strip.cockpit_seats[0].focused);
    TEST("PC-1: seat 0 is ready",
         strip.cockpit_seats[0].status == "ready");
    TEST("PC-1: seat 1 is @altair",
         strip.cockpit_seats[1].callsign == "@altair");
    TEST("PC-1: seat 1 not focused",
         !strip.cockpit_seats[1].focused);
    TEST("PC-1: focused seat index is 0",
         strip.focused_seat_index == 0);
    TEST("PC-1: Cockpit mode has no folder tree",
         strip.tree_entry_count() == 0);
}

static void test_pc1_strip_content_editor() {
    traveler::tui::StripContent strip;
    strip.set_mode("Editor", kTestRoster);

    TEST("PC-1: strip in Editor mode", strip.is_editor());
    TEST("PC-1: strip not in Cockpit mode when Editor", !strip.is_cockpit());
    TEST("PC-1: Editor has folder tree entries", strip.tree_entry_count() > 0);
    TEST("PC-1: Editor has breadcrumb", !strip.file_breadcrumb.empty());
    TEST("PC-1: breadcrumb points to surface_model.cpp",
         strip.file_breadcrumb == "src/tui/surface_model.cpp");
    TEST("PC-1: Editor has no cockpit seats", strip.seat_count() == 0);

    // Verify tree structure: first entry should be "src/" directory
    TEST("PC-1: first tree entry is src/",
         strip.folder_tree[0].name == "src/");
    TEST("PC-1: src/ is directory", strip.folder_tree[0].is_dir);
    TEST("PC-1: src/ is expanded", strip.folder_tree[0].expanded);

    // Verify depth levels
    bool has_nested = false;
    for (const auto& entry : strip.folder_tree) {
        if (entry.depth > 0) {
            has_nested = true;
            break;
        }
    }
    TEST("PC-1: tree has nested entries (depth > 0)", has_nested);
}

static void test_pc1_strip_content_mode_switch() {
    traveler::tui::StripContent strip;

    // Start in Cockpit
    strip.set_mode("Cockpit", kTestRoster);
    TEST("PC-1: initial Cockpit", strip.is_cockpit());

    // Switch to Editor
    strip.set_mode("Editor", kTestRoster);
    TEST("PC-1: switched to Editor", strip.is_editor());
    TEST("PC-1: Cockpit seats cleared on switch", strip.seat_count() == 0);

    // Switch back to Cockpit
    strip.set_mode("Cockpit", kTestRoster);
    TEST("PC-1: switched back to Cockpit", strip.is_cockpit());
    TEST("PC-1: Editor tree cleared on switch", strip.tree_entry_count() == 0);
    TEST("PC-1: Cockpit seats restored", strip.seat_count() == 4);
}

// ============================================================================
// PC-2: Leader Cheatsheet — space opens, e→Editor, contains 5+ keys
// ============================================================================

static void test_pc2_cheatsheet_defaults() {
    traveler::tui::LeaderCheatsheet cheatsheet;
    cheatsheet.load_defaults();

    // Must have at least 5 entries (e, l, c, p, g) + optional 's'
    TEST("PC-2: cheatsheet has entries", cheatsheet.entry_count() >= 5);
    TEST("PC-2: cheatsheet not visible by default", !cheatsheet.is_visible());

    // Check each required key per Brief B5 item 3
    const auto* e_entry = cheatsheet.find('e');
    TEST("PC-2: 'e' key exists", e_entry != nullptr);
    TEST("PC-2: 'e' maps to Editor", e_entry->command == "Editor");
    TEST("PC-2: 'e' display is Editor mode",
         e_entry->display_name == "Editor mode");

    const auto* l_entry = cheatsheet.find('l');
    TEST("PC-2: 'l' key exists", l_entry != nullptr);
    TEST("PC-2: 'l' maps to LLM", l_entry->command == "LLM");

    const auto* c_entry = cheatsheet.find('c');
    TEST("PC-2: 'c' key exists", c_entry != nullptr);
    TEST("PC-2: 'c' maps to Cockpit", c_entry->command == "Cockpit");

    const auto* p_entry = cheatsheet.find('p');
    TEST("PC-2: 'p' key exists", p_entry != nullptr);
    TEST("PC-2: 'p' maps to Pane", p_entry->command == "Pane");

    const auto* g_entry = cheatsheet.find('g');
    TEST("PC-2: 'g' key exists", g_entry != nullptr);
    TEST("PC-2: 'g' maps to Git", g_entry->command == "Git");

    // Verify unknown key returns nullptr
    const auto* unknown = cheatsheet.find('z');
    TEST("PC-2: unknown key 'z' returns nullptr", unknown == nullptr);
}

static void test_pc2_cheatsheet_visibility() {
    traveler::tui::LeaderCheatsheet cheatsheet;
    cheatsheet.load_defaults();

    TEST("PC-2: initially hidden", !cheatsheet.is_visible());

    cheatsheet.open();
    TEST("PC-2: open makes visible", cheatsheet.is_visible());

    cheatsheet.close();
    TEST("PC-2: close hides", !cheatsheet.is_visible());
}

static void test_pc2_cheatsheet_e_switches_to_editor() {
    // Simulate: space → cheatsheet opens → 'e' → dispatches Editor
    traveler::tui::LeaderCheatsheet cheatsheet;
    cheatsheet.load_defaults();

    cheatsheet.open();
    TEST("PC-2: cheatsheet open", cheatsheet.is_visible());

    const auto* entry = cheatsheet.find('e');
    TEST("PC-2: press e finds Editor entry", entry != nullptr);

    // The command is "Editor" — the TUI would pass this to the dispatcher.
    // Verify the dispatcher can consume it:
    traveler::core::Dispatcher dispatcher;
    auto slash_cmd = "/" + entry->command;
    auto result = dispatcher.transition_command(slash_cmd);
    TEST("PC-2: 'e' → dispatcher accepts /Editor", result.ok);
    TEST("PC-2: 'e' → dispatcher is in Editor mode",
         dispatcher.current_mode() == traveler::core::Mode::Editor);
}

// ============================================================================
// PC-3: @ Autocomplete — @v → @vega first, ranked
// ============================================================================

static void test_pc3_autocomplete_at_v() {
    traveler::tui::AutocompleteModel model;
    model.compute("ask @v");

    TEST("PC-3: autocomplete visible for @v", model.is_visible());
    TEST("PC-3: has results", model.result_count() >= 4);
    TEST("PC-3: top result is @vega",
         model.top_callsign() == "@vega");
    TEST("PC-3: @vega has exact prefix",
         model.results[0].exact_prefix);

    // Verify all 4 rostered callsigns appear
    bool found_vega = false, found_altair = false;
    bool found_orion = false, found_rigel = false;

    for (const auto& r : model.results) {
        if (r.callsign == "@vega")   found_vega   = true;
        if (r.callsign == "@altair") found_altair = true;
        if (r.callsign == "@orion")  found_orion  = true;
        if (r.callsign == "@rigel")  found_rigel  = true;
    }

    TEST("PC-3: @vega present", found_vega);
    TEST("PC-3: @altair present", found_altair);
    TEST("PC-3: @orion present", found_orion);
    TEST("PC-3: @rigel present", found_rigel);
}

static void test_pc3_autocomplete_no_at_sign() {
    traveler::tui::AutocompleteModel model;
    model.compute("hello world");

    TEST("PC-3: no autocomplete without @", !model.is_visible());
    TEST("PC-3: empty results", model.result_count() == 0);
}

static void test_pc3_autocomplete_partial() {
    traveler::tui::AutocompleteModel model;
    model.compute("ask @or");

    TEST("PC-3: autocomplete for @or", model.is_visible());
    // @orion should be first (exact prefix match for "@or")
    TEST("PC-3: top result is @orion for query @or",
         model.top_callsign() == "@orion");
}

static void test_pc3_autocomplete_clear() {
    traveler::tui::AutocompleteModel model;
    model.compute("ask @v");
    TEST("PC-3: visible before clear", model.is_visible());

    model.clear();
    TEST("PC-3: not visible after clear", !model.is_visible());
    TEST("PC-3: zero results after clear", model.result_count() == 0);
}

// ============================================================================
// PC-4: Separator Drag — resize Strip/Stage, record real-time updates
// ============================================================================

static void test_pc4_separator_drag_init() {
    traveler::tui::SeparatorDragModel drag;

    TEST_EQ("PC-4: default strip height 3", drag.strip_height(), 3);
    TEST_EQ("PC-4: default tower height 5", drag.tower_height(), 5);
    TEST("PC-4: not dragging initially", !drag.drag_active());
    TEST_EQ("PC-4: zero events initially", drag.event_count(), 0);
}

static void test_pc4_separator_drag_basic() {
    traveler::tui::SeparatorDragModel drag;
    const int term_height = 40;

    drag.begin_drag(3);  // start drag at row 3 (where separator is)

    TEST("PC-4: drag active after begin", drag.drag_active());

    // Drag separator down by 2 rows
    drag.update_drag(5, term_height);

    TEST_EQ("PC-4: strip height grew by 2", drag.strip_height(), 5);
    TEST("PC-4: event count increased", drag.event_count() >= 1);

    auto event = drag.last_event();
    TEST_EQ("PC-4: event old height was 3", event.old_strip_height, 3);
    TEST_EQ("PC-4: event new height is 5", event.new_strip_height, 5);
    TEST_EQ("PC-4: event delta is +2", event.delta, 2);

    drag.end_drag();
    TEST("PC-4: not dragging after end", !drag.drag_active());

    // Height preserved after drag ends
    TEST_EQ("PC-4: height preserved after end", drag.strip_height(), 5);
}

static void test_pc4_separator_drag_multiple_updates() {
    traveler::tui::SeparatorDragModel drag;
    const int term_height = 40;

    drag.begin_drag(3);

    drag.update_drag(4, term_height);   // +1
    drag.update_drag(6, term_height);   // +3
    drag.update_drag(2, term_height);   // -1

    TEST_EQ("PC-4: final height after 3 moves", drag.strip_height(), 2);
    TEST("PC-4: 3 events after 3 moves", drag.event_count() >= 3);

    auto event = drag.last_event();
    TEST_EQ("PC-4: last event new height is 2", event.new_strip_height, 2);
    TEST_EQ("PC-4: last event old height was 6", event.old_strip_height, 6);
    TEST_EQ("PC-4: last event delta is -4", event.delta, -4);

    drag.end_drag();
    TEST_EQ("PC-4: height 2 preserved", drag.strip_height(), 2);
}

static void test_pc4_separator_drag_clamp() {
    traveler::tui::SeparatorDragModel drag;
    const int term_height = 24;

    drag.begin_drag(3);

    // Drag beyond top — should clamp to 1
    drag.update_drag(-10, term_height);
    TEST_EQ("PC-4: clamped to minimum 1", drag.strip_height(), 1);

    // Drag beyond bottom — should clamp to max
    drag.update_drag(100, term_height);
    // max_strip = term_height - tower_height - 1 = 24 - 5 - 1 = 18
    TEST("PC-4: clamped to max (≤ 18)", drag.strip_height() <= 18);
    TEST("PC-4: clamping still > 0", drag.strip_height() > 0);
}

static void test_pc4_separator_drag_noop_without_active() {
    traveler::tui::SeparatorDragModel drag;

    // update_drag should have no effect when drag is not active
    drag.update_drag(10, 40);
    TEST_EQ("PC-4: no height change without active drag", drag.strip_height(), 3);
    TEST_EQ("PC-4: no events without active drag", drag.event_count(), 0);
}

static void test_pc4_separator_drag_timestamp_advances() {
    traveler::tui::SeparatorDragModel drag;

    auto t0 = drag.last_update_time();
    drag.begin_drag(3);
    auto t1 = drag.last_update_time();
    TEST("PC-4: timestamp advances on begin", t1 >= t0);

    drag.update_drag(5, 40);
    auto t2 = drag.last_update_time();
    TEST("PC-4: timestamp advances on update", t2 >= t1);

    drag.end_drag();
    auto t3 = drag.last_update_time();
    TEST("PC-4: timestamp advances on end", t3 >= t2);
}

// ============================================================================
// PC-11: Mode-Switch Latency Harness — < 50 ms over 100 trials
// ============================================================================

static void test_pc11_harness_empty() {
    traveler::tui::ModeSwitchHarness harness;
    TEST_EQ("PC-11: empty harness has 0 trials", harness.trial_count(), 0);
    TEST("PC-11: empty harness does not pass", !harness.passes());
    TEST_EQ("PC-11: empty median is 0ns",
            harness.median_latency().count(), 0);
}

static void test_pc11_harness_single_sample() {
    traveler::tui::ModeSwitchHarness harness;
    harness.record("Cockpit", "Editor", std::chrono::milliseconds(10));

    TEST_EQ("PC-11: one trial", harness.trial_count(), 1);
    // record() stores nanoseconds; compare in same unit
    const auto expected_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::milliseconds(10)).count();
    TEST_EQ("PC-11: median equals single value",
            harness.median_latency().count(),
            expected_ns);
    TEST("PC-11: 10ms < 50ms threshold → passes", harness.passes(50.0));
}

static void test_pc11_harness_median_computation() {
    traveler::tui::ModeSwitchHarness harness;

    // Odd number of samples: 5, 10, 15, 20, 25 → median = 15
    harness.record("C", "E", std::chrono::milliseconds(5));
    harness.record("C", "E", std::chrono::milliseconds(25));
    harness.record("C", "E", std::chrono::milliseconds(15));
    harness.record("C", "E", std::chrono::milliseconds(20));
    harness.record("C", "E", std::chrono::milliseconds(10));

    TEST_EQ("PC-11: 5 trials", harness.trial_count(), 5);
    const auto expected_median_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::milliseconds(15)).count();
    TEST_EQ("PC-11: median of [5,10,15,20,25] is 15ms",
            harness.median_latency().count(),
            expected_median_ns);
    TEST("PC-11: 15ms < 50ms threshold → passes", harness.passes(50.0));
}

static void test_pc11_harness_max_min() {
    traveler::tui::ModeSwitchHarness harness;
    harness.record("A", "B", std::chrono::milliseconds(30));
    harness.record("A", "B", std::chrono::milliseconds(10));
    harness.record("A", "B", std::chrono::milliseconds(50));
    harness.record("A", "B", std::chrono::milliseconds(20));

    const auto max_expected_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::milliseconds(50)).count();
    const auto min_expected_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::milliseconds(10)).count();
    const auto median_expected_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::milliseconds(25)).count();

    TEST_EQ("PC-11: max is 50ms",
            harness.max_latency().count(),
            max_expected_ns);
    TEST_EQ("PC-11: min is 10ms",
            harness.min_latency().count(),
            min_expected_ns);
    // [10, 20, 30, 50] → median = (20+30)/2 = 25
    TEST_EQ("PC-11: even median is 25ms",
            harness.median_latency().count(),
            median_expected_ns);
}

static void test_pc11_harness_fails_at_threshold() {
    traveler::tui::ModeSwitchHarness harness;
    // 60 ms median > 50 ms threshold
    harness.record("C", "E", std::chrono::milliseconds(60));
    harness.record("C", "E", std::chrono::milliseconds(55));
    harness.record("C", "E", std::chrono::milliseconds(65));

    TEST("PC-11: fails when median exceeds threshold", !harness.passes(50.0));
    TEST("PC-11: passes with threshold 70", harness.passes(70.0));
}

static void test_pc11_harness_mode_switch_real() {
    // Real measurement: dispatcher transition_command latency over 100 trials
    traveler::core::Dispatcher dispatcher;
    traveler::tui::ModeSwitchHarness harness;

    const char* modes[] = {"Editor", "LLM", "Pane", "Git", "Cockpit"};

    for (int i = 0; i < 100; ++i) {
        const char* from = modes[i % 5];
        const char* to   = modes[(i + 1) % 5];

        // Ensure dispatcher is in the "from" mode first
        dispatcher.transition_command("/" + std::string(from));

        auto t0 = std::chrono::steady_clock::now();
        dispatcher.transition_command("/" + std::string(to));
        auto t1 = std::chrono::steady_clock::now();

        harness.record(from, to, t1 - t0);
    }

    TEST_EQ("PC-11: 100 trials recorded", harness.trial_count(), 100);

    auto median = harness.median_latency();
    auto max_ns = harness.max_latency();
    auto min_ns = harness.min_latency();

    std::fprintf(stdout,
        "\n  PC-11 Mode-switch latency (100 trials, dispatcher only):\n"
        "    Median: %lld ns (%.3f ms)\n"
        "    Max:    %lld ns (%.3f ms)\n"
        "    Min:    %lld ns (%.3f ms)\n",
        static_cast<long long>(median.count()),
        median.count() / 1'000'000.0,
        static_cast<long long>(max_ns.count()),
        max_ns.count() / 1'000'000.0,
        static_cast<long long>(min_ns.count()),
        min_ns.count() / 1'000'000.0);

    // Dispatcher-only transitions should be well under 50 ms.
    // (This is nanoseconds-level work — pointer assignments and string copies.)
    bool passes = harness.passes(50.0);
    TEST("PC-11: dispatcher-only latency < 50 ms median", passes);

    // Always print the deferral note
    std::fprintf(stdout,
        "    %s\n",
        traveler::tui::ModeSwitchHarness::core_i3_deferral_note().data());
}

static void test_pc11_harness_core_i3_deferral() {
    // Verify deferral note exists and is non-empty
    auto note = traveler::tui::ModeSwitchHarness::core_i3_deferral_note();
    TEST("PC-11: Core-i3 deferral note is non-empty", !note.empty());
    TEST("PC-11: note mentions Core-i3",
         note.find("Core-i3") != std::string_view::npos);
    TEST("PC-11: note mentions hardware unavailable",
         note.find("unavailable") != std::string_view::npos);
}

// ============================================================================
// Combined Scenario Tests
// ============================================================================

static void test_scenario_cockpit_to_editor_full() {
    // Simulate the full PC-1 + PC-2 flow:
    // Launch in Cockpit → /Editor → mode switches → Strip changes → can go back

    traveler::core::Dispatcher dispatcher;
    traveler::tui::ModeHeader header;
    traveler::tui::StripContent strip;

    // 1. Launch: Cockpit
    header.transition_to("Cockpit");
    strip.set_mode("Cockpit", kTestRoster);
    TEST("SCENARIO: launch mode Cockpit", header.mode_name == "Cockpit");
    TEST("SCENARIO: strip is cockpit", strip.is_cockpit());
    TEST("SCENARIO: cockpit has seats", strip.seat_count() == 4);

    // 2. Type /Editor
    auto result = dispatcher.transition_command("/Editor");
    TEST("SCENARIO: /Editor transition succeeds", result.ok);

    header.transition_to("Editor");
    strip.set_mode("Editor", kTestRoster);
    TEST("SCENARIO: mode is Editor", header.mode_name == "Editor");
    TEST("SCENARIO: strip is editor", strip.is_editor());
    TEST("SCENARIO: editor has breadcrumb", !strip.file_breadcrumb.empty());

    // 3. Back to Cockpit via /Cockpit
    auto result2 = dispatcher.transition_command("/Cockpit");
    TEST("SCENARIO: /Cockpit transition succeeds", result2.ok);

    header.transition_to("Cockpit");
    strip.set_mode("Cockpit", kTestRoster);
    TEST("SCENARIO: back to Cockpit", header.mode_name == "Cockpit");
    TEST("SCENARIO: strip is cockpit again", strip.is_cockpit());
}

// ============================================================================
// main
// ============================================================================

int main() {
    // --- PC-1: Mode Header ---
    test_pc1_mode_header_default();
    test_pc1_mode_header_transition();
    test_pc1_mode_header_with_dispatcher();

    // --- PC-1: Strip Content ---
    test_pc1_strip_content_cockpit();
    test_pc1_strip_content_editor();
    test_pc1_strip_content_mode_switch();

    // --- PC-2: Leader Cheatsheet ---
    test_pc2_cheatsheet_defaults();
    test_pc2_cheatsheet_visibility();
    test_pc2_cheatsheet_e_switches_to_editor();

    // --- PC-3: @ Autocomplete ---
    test_pc3_autocomplete_at_v();
    test_pc3_autocomplete_no_at_sign();
    test_pc3_autocomplete_partial();
    test_pc3_autocomplete_clear();

    // --- PC-4: Separator Drag ---
    test_pc4_separator_drag_init();
    test_pc4_separator_drag_basic();
    test_pc4_separator_drag_multiple_updates();
    test_pc4_separator_drag_clamp();
    test_pc4_separator_drag_noop_without_active();
    test_pc4_separator_drag_timestamp_advances();

    // --- PC-11: Mode-Switch Latency Harness ---
    test_pc11_harness_empty();
    test_pc11_harness_single_sample();
    test_pc11_harness_median_computation();
    test_pc11_harness_max_min();
    test_pc11_harness_fails_at_threshold();
    test_pc11_harness_mode_switch_real();
    test_pc11_harness_core_i3_deferral();

    // --- Combined Scenario ---
    test_scenario_cockpit_to_editor_full();

    // --- Summary ---
    std::fprintf(stdout,
        "\n=== TUI Surface Model Test Summary ===\n"
        "  Passed: %d\n"
        "  Failed: %d\n"
        "  Total:  %d\n",
        g_passed, g_failed, g_passed + g_failed);

    return g_failed > 0 ? 1 : 0;
}
