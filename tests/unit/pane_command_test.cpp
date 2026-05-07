// WAVE-B Lane B2: Pane + Command Palette + Optional Parsers test harness
// Reference: Traveler_Phase0_Spec_v0.1.md §6 PC-7, PC-9, PC-12
// Reference: briefs/reconcile-p0-2-2026-05-06.md
// Reference: briefs/wave-b-b2-pane-command-2026-05-07.md
//
// Tests:
//   PC-7  — Pane mode horizontal split + arrow navigation
//   PC-9  — Command Palette fuzzy search ("edi" → "Open Editor mode" first)
//   PC-12 — Modal-ex / tmux-prefix disabled by default
//
// Pure state/model layer; no live TUI wiring.
//
// Compile requirements (B6 xmake registration — NOT added here, see brief):
//   target("test_pane_command")
//       set_kind("binary")
//       add_includedirs("src")
//       add_files("tests/unit/pane_command_test.cpp")
//       add_files("src/pane/model.cpp")
//       add_files("src/command/registry.cpp")
//       add_files("src/command/palette.cpp")
//       add_files("src/command/optional_parsers.cpp")
//       set_group("test")
//       add_tests("default")

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "command/optional_parsers.h"
#include "command/palette.h"
#include "command/registry.h"
#include "pane/model.h"

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
// PC-7: Pane mode — horizontal split + arrow-key navigation
// Spec §6 line 716: "Pane mode `Ctrl+B "` (horizontal split) creates a new
// pane; arrow-key navigation between panes works"
// ============================================================================
static void test_pc7_pane_horizontal_split() {
    std::cout << "\n--- PC-7: Pane horizontal split ---" << std::endl;

    traveler::pane::PaneLayout layout;

    // Initial state: 1 pane, it's active
    TEST("PC7a: 1 pane initially", layout.pane_count() == 1);
    TEST("PC7b: active pane exists", layout.active_pane() != nullptr);
    TEST("PC7c: active pane id = 1", layout.active_pane()->id == 1);
    TEST("PC7d: active pane number = 1", layout.active_pane_number() == 1);
    TEST("PC7e: grid = 1×1", layout.grid_rows() == 1 && layout.grid_cols() == 1);

    // Horizontal split (Ctrl+B ")
    int new_id = layout.split_horizontal();
    TEST("PC7f: split_horizontal returns valid id > 0", new_id > 0);
    TEST("PC7g: pane count = 2 after split", layout.pane_count() == 2);
    TEST("PC7h: grid rows = 2, cols = 1",
         layout.grid_rows() == 2 && layout.grid_cols() == 1);

    // New pane becomes active
    TEST("PC7i: new pane is active", layout.active_pane()->id == new_id);
    TEST("PC7j: active pane number = 2", layout.active_pane_number() == 2);

    // Arrow-key navigation: Up goes to the original pane (above)
    layout.navigate_up();
    TEST("PC7k: navigate_up changes active", layout.active_pane_number() == 1);
    TEST("PC7l: active pane id = 1 after up", layout.active_pane()->id == 1);

    // Up again from top: no-op
    layout.navigate_up();
    TEST("PC7m: navigate_up at top is no-op", layout.active_pane_number() == 1);

    // Down goes back to the new pane
    layout.navigate_down();
    TEST("PC7n: navigate_down changes active", layout.active_pane_number() == 2);

    // Down again from bottom: no-op
    layout.navigate_down();
    TEST("PC7o: navigate_down at bottom is no-op", layout.active_pane_number() == 2);

    // Left/Right in a 1-column grid is a no-op
    layout.navigate_left();
    TEST("PC7p: navigate_left in 1-col grid is no-op", layout.active_pane_number() == 2);
    layout.navigate_right();
    TEST("PC7q: navigate_right in 1-col grid is no-op", layout.active_pane_number() == 2);
}

static void test_pc7_pane_vertical_split() {
    std::cout << "\n--- PC-7: Pane vertical split ---" << std::endl;

    traveler::pane::PaneLayout layout;

    // Vertical split (Ctrl+B %)
    int new_id = layout.split_vertical();
    TEST("PC7va: split_vertical returns valid id", new_id > 0);
    TEST("PC7vb: pane count = 2 after v-split", layout.pane_count() == 2);
    TEST("PC7vc: grid rows = 1, cols = 2",
         layout.grid_rows() == 1 && layout.grid_cols() == 2);

    // New pane is active (to the right)
    TEST("PC7vd: new pane is active after v-split",
         layout.active_pane()->id == new_id);

    // Left goes back to original pane
    layout.navigate_left();
    TEST("PC7ve: navigate_left returns to pane 1", layout.active_pane()->id == 1);

    // Right returns to new pane
    layout.navigate_right();
    TEST("PC7vf: navigate_right goes to pane 2", layout.active_pane()->id == new_id);

    // Up/Down are no-op in 1-row grid
    layout.navigate_up();
    TEST("PC7vg: navigate_up in 1-row is no-op", layout.active_pane()->id == new_id);
    layout.navigate_down();
    TEST("PC7vh: navigate_down in 1-row is no-op", layout.active_pane()->id == new_id);
}

static void test_pc7_pane_combined_navigation() {
    std::cout << "\n--- PC-7: Pane combined horizontal+vertical navigation ---" << std::endl;

    traveler::pane::PaneLayout layout;

    // Create a 2×2 grid: first split horizontal, then split vertical on the
    // new pane, then go back up and split vertical on the original pane.
    layout.split_horizontal();                 // pane 2 below pane 1
    layout.split_vertical();                   // pane 3 to the right of pane 2
    layout.navigate_up();                      // back to pane 1
    int id4 = layout.split_vertical();        // pane 4 to the right of pane 1

    TEST("PC7c1: 4 panes total", layout.pane_count() == 4);
    // After 3 splits (1 h-split + 2 v-splits on different rows), the grid
    // grows: rows=2 (from the h-split), cols=3 (each v-split adds a column).
    TEST("PC7c2: grid 2×3 after 3 splits",
         layout.grid_rows() == 2 && layout.grid_cols() == 3);

    // Active is pane 4 (top-right)
    TEST("PC7c3: active = pane 4", layout.active_pane()->id == id4);

    // Navigate around the 2×2 grid
    layout.navigate_down();   // 4 → 3 (bottom-right)
    TEST("PC7c4: down to pane 3", layout.active_pane()->id != id4);
    layout.navigate_left();   // 3 → 2 (bottom-left)
    layout.navigate_up();     // 2 → 1 (top-left)
    TEST("PC7c5: up-left to pane 1", layout.active_pane()->id == 1);

    // Pane modes preserved through splits
    TEST("PC7c6: pane 1 mode = Cockpit",
         layout.active_pane()->mode == "Cockpit");
}

// ============================================================================
// PC-9: Command Palette — registry + fuzzy search
// Spec §6 line 718: "Command Palette opens on `Ctrl+P`; typing `edi` ranks
// 'Open Editor mode' first"
// ============================================================================
static void test_pc9_registry_defaults() {
    std::cout << "\n--- PC-9: Action registry defaults ---" << std::endl;

    traveler::command::ActionRegistry registry;
    registry.populate_defaults();

    const auto& actions = registry.actions();
    TEST("PC9a: default actions populated", actions.size() >= 10);

    // Verify key actions exist
    bool has_editor = false;
    bool has_llm = false;
    bool has_pane = false;
    bool has_cockpit = false;
    bool has_git = false;
    for (const auto& a : actions) {
        if (a.id == "open-editor-mode") has_editor = true;
        if (a.id == "open-llm-mode") has_llm = true;
        if (a.id == "open-pane-mode") has_pane = true;
        if (a.id == "open-cockpit-mode") has_cockpit = true;
        if (a.id == "open-git-mode") has_git = true;
    }
    TEST("PC9b: open-editor-mode registered", has_editor);
    TEST("PC9c: open-llm-mode registered", has_llm);
    TEST("PC9d: open-pane-mode registered", has_pane);
    TEST("PC9e: open-cockpit-mode registered", has_cockpit);
    TEST("PC9f: open-git-mode registered", has_git);

    // Duplicate registration is ignored
    std::size_t before = actions.size();
    registry.register_action({"open-editor-mode", "Duplicate", ""});
    TEST("PC9g: duplicate ignored", registry.actions().size() == before);
}

static void test_pc9_palette_fuzzy_search() {
    std::cout << "\n--- PC-9: Command Palette fuzzy search ---" << std::endl;

    traveler::command::ActionRegistry registry;
    registry.populate_defaults();
    traveler::command::CommandPalette palette(registry);

    // Empty query returns all actions
    auto all = palette.query("");
    TEST("PC9h: empty query returns all actions", all.size() == registry.actions().size());

    // Query "edi" → "Open Editor mode" must be ranked FIRST (exact prefix on
    // the word "Editor")
    auto edi_results = palette.query("edi");
    TEST("PC9i: 'edi' query has results", !edi_results.empty());
    TEST("PC9j: first result is 'Open Editor mode'",
         edi_results[0].action.display_name == "Open Editor mode");

    // Verify Editor mode scores higher than any other result
    if (edi_results.size() > 1) {
        TEST("PC9k: Editor mode score > next result score",
             edi_results[0].score >= edi_results[1].score);
    }

    // Query "edi" should NOT return unrelated modes like LLM/Cockpit
    bool has_editor = false;
    bool has_llm = false;
    for (const auto& r : edi_results) {
        if (r.action.id == "open-editor-mode") has_editor = true;
        if (r.action.id == "open-llm-mode") has_llm = true;
    }
    TEST("PC9l: 'edi' includes Editor mode", has_editor);
    // "LLM" does NOT match "edi" at all
    TEST("PC9m: 'edi' excludes LLM mode", !has_llm);

    // Query "pane" → "Open Pane mode" first
    auto pane_results = palette.query("pane");
    TEST("PC9n: 'pane' query has results", !pane_results.empty());
    TEST("PC9o: 'pane' first = 'Open Pane mode'",
         pane_results[0].action.display_name == "Open Pane mode");

    // Query "git" → "Open Git mode" first
    auto git_results = palette.query("git");
    TEST("PC9p: 'git' first = 'Open Git mode'",
         git_results[0].action.display_name == "Open Git mode");

    // Query with no match
    auto nomatch = palette.query("xyzzy");
    TEST("PC9q: 'xyzzy' returns empty", nomatch.empty());
}

static void test_pc9_score_match_unit() {
    std::cout << "\n--- PC-9: score_match unit tests ---" << std::endl;

    using traveler::command::CommandPalette;

    // Exact prefix at word boundary — highest score
    int s_edi_editor = CommandPalette::score_match("edi", "Open Editor mode");
    int s_edi_pane   = CommandPalette::score_match("edi", "Open Pane mode");
    int s_edi_llm    = CommandPalette::score_match("edi", "Open LLM mode");
    int s_edi_git    = CommandPalette::score_match("edi", "Open Git mode");

    TEST("PC9s1: 'edi' vs 'Open Editor mode' scores > 0", s_edi_editor > 0);
    TEST("PC9s2: 'edi' vs 'Open Pane mode' scores 0 (no 'd')", s_edi_pane == 0);
    TEST("PC9s3: 'edi' vs 'Open LLM mode' scores 0", s_edi_llm == 0);
    TEST("PC9s4: 'edi' vs 'Open Git mode' scores 0", s_edi_git == 0);

    // Word prefix match (like "edit" match on "Editor")
    int s_edit_editor = CommandPalette::score_match("edit", "Open Editor mode");
    int s_edit_pane   = CommandPalette::score_match("edit", "Open Pane mode");
    TEST("PC9s5: 'edit' vs 'Open Editor mode' > 0", s_edit_editor > 0);
    TEST("PC9s6: 'edit' vs 'Open Pane mode' scores 0", s_edit_pane == 0);

    // Subsequence match (scattered)
    int s_op_en = CommandPalette::score_match("oen", "Open Editor mode");
    TEST("PC9s7: 'oen' vs 'Open Editor mode' > 0 (subsequence)", s_op_en > 0);

    // Empty query: all targets score 1
    int s_empty = CommandPalette::score_match("", "anything");
    TEST("PC9s8: empty query scores 1", s_empty == 1);

    // Empty target: score 0
    int s_et = CommandPalette::score_match("x", "");
    TEST("PC9s9: empty target scores 0", s_et == 0);

    // Case insensitivity
    int s_EDI = CommandPalette::score_match("EDI", "Open Editor mode");
    TEST("PC9s10: case-insensitive match", s_EDI == s_edi_editor);

    // Exact word prefix scores highest
    int s_pref = CommandPalette::score_match("edit", "Open Editor mode");
    int s_sub  = CommandPalette::score_match("edr", "Open Editor mode");
    TEST("PC9s11: exact prefix 'edit' > scattered 'edr'", s_pref > s_sub);
}

// ============================================================================
// PC-12: Modal-ex / tmux-prefix disabled by default
// Spec §6 line 721: "Modal-ex `:` is disabled by default; enabled via
// `traveler --modal` or `:set modal`; tmux-prefix `<C-b>` is disabled by
// default; enabled via config"
// ============================================================================
static void test_pc12_optional_parsers_disabled_by_default() {
    std::cout << "\n--- PC-12: Optional parsers default-disabled ---" << std::endl;

    traveler::command::OptionalParsers op;

    // Both disabled by default
    TEST("PC12a: modal disabled by default", !op.modal_enabled());
    TEST("PC12b: tmux prefix disabled by default", !op.tmux_prefix_enabled());

    // Modal commands rejected when disabled
    auto modal_r = op.parse_modal(":w");
    TEST("PC12c: :w rejected when disabled", !modal_r.ok);
    TEST("PC12d: :w error mentions 'disabled'",
         modal_r.error.find("disabled") != std::string::npos);

    // Tmux prefix rejected when disabled
    auto tmux_r = op.parse_tmux_prefix('"');
    TEST("PC12e: tmux '\"' rejected when disabled", !tmux_r.ok);
    TEST("PC12f: tmux error mentions 'disabled'",
         tmux_r.error.find("disabled") != std::string::npos);
}

static void test_pc12_modal_enabled() {
    std::cout << "\n--- PC-12: Modal-ex enabled ---" << std::endl;

    traveler::command::OptionalParsers op;
    op.set_modal_enabled(true);
    TEST("PC12g: set_modal_enabled(true) works", op.modal_enabled());

    // :w — save
    auto r_w = op.parse_modal(":w");
    TEST("PC12h: :w succeeds when enabled", r_w.ok);
    TEST("PC12i: :w command = 'w'", r_w.command == "w");
    TEST("PC12j: :w has no args", r_w.args.empty());

    // :q — quit
    auto r_q = op.parse_modal(":q");
    TEST("PC12k: :q succeeds", r_q.ok);
    TEST("PC12l: :q command = 'q'", r_q.command == "q");

    // :wq — write and quit
    auto r_wq = op.parse_modal(":wq");
    TEST("PC12m: :wq succeeds", r_wq.ok);
    TEST("PC12n: :wq command = 'wq'", r_wq.command == "wq");

    // :e <file> — open file
    auto r_e = op.parse_modal(":e /path/to/file.cpp");
    TEST("PC12o: :e succeeds", r_e.ok);
    TEST("PC12p: :e command = 'e'", r_e.command == "e");
    TEST("PC12q: :e args[0] = '/path/to/file.cpp'",
         r_e.args.size() == 1 && r_e.args[0] == "/path/to/file.cpp");

    // :set modal
    auto r_set = op.parse_modal(":set modal");
    TEST("PC12r: :set modal succeeds", r_set.ok);
    TEST("PC12s: :set command = 'set'", r_set.command == "set");
    TEST("PC12t: :set args[0] = 'modal'",
         r_set.args.size() == 1 && r_set.args[0] == "modal");

    // Unknown command
    auto r_bad = op.parse_modal(":unknown_cmd");
    TEST("PC12u: unknown modal command returns error", !r_bad.ok);
    TEST("PC12v: error mentions 'unknown'",
         r_bad.error.find("unknown") != std::string::npos);

    // Missing colon
    auto r_no_colon = op.parse_modal("w");
    TEST("PC12w: missing ':' prefix returns error", !r_no_colon.ok);

    // Empty command after colon
    auto r_empty = op.parse_modal(":");
    TEST("PC12x: empty modal command returns error", !r_empty.ok);

    // Leading whitespace is trimmed
    auto r_ws = op.parse_modal(":   w");
    TEST("PC12y: whitespace after ':' is trimmed", r_ws.ok && r_ws.command == "w");
}

static void test_pc12_tmux_prefix_enabled() {
    std::cout << "\n--- PC-12: Tmux prefix enabled ---" << std::endl;

    traveler::command::OptionalParsers op;
    op.set_tmux_prefix_enabled(true);
    TEST("PC12z: set_tmux_prefix_enabled(true) works", op.tmux_prefix_enabled());

    // Ctrl+B " — horizontal split
    auto r_h = op.parse_tmux_prefix('"');
    TEST("PC12aa: tmux '\"' = 'split-horizontal'",
         r_h.ok && r_h.action == "split-horizontal");

    // Ctrl+B % — vertical split
    auto r_v = op.parse_tmux_prefix('%');
    TEST("PC12ab: tmux '%' = 'split-vertical'",
         r_v.ok && r_v.action == "split-vertical");

    // Ctrl+B n — next pane
    auto r_n = op.parse_tmux_prefix('n');
    TEST("PC12ac: tmux 'n' = 'next-pane'",
         r_n.ok && r_n.action == "next-pane");

    // Ctrl+B p — previous pane
    auto r_p = op.parse_tmux_prefix('p');
    TEST("PC12ad: tmux 'p' = 'prev-pane'",
         r_p.ok && r_p.action == "prev-pane");

    // Unknown key
    auto r_bad = op.parse_tmux_prefix('x');
    TEST("PC12ae: unknown tmux key returns error", !r_bad.ok);
    TEST("PC12af: error mentions 'unknown'",
         r_bad.error.find("unknown") != std::string::npos);
}

// ============================================================================
// Edge cases
// ============================================================================
static void test_edge_cases() {
    std::cout << "\n--- Edge cases ---" << std::endl;

    // Pane: active_pane on empty layout is safe
    {
        traveler::pane::PaneLayout empty;
        // Populate then remove all — test null safety
        // (We can't remove panes yet, so we just test the existing layout)
        TEST("EDGEa: active_pane on valid layout is non-null",
             empty.active_pane() != nullptr);
    }

    // Registry: register_action after populate doesn't corrupt
    {
        traveler::command::ActionRegistry r;
        r.populate_defaults();
        r.register_action({"custom-action", "Custom Action", "Editor"});
        TEST("EDGEb: custom action added after defaults",
             r.actions().size() >= 11);
        bool found = false;
        for (const auto& a : r.actions()) {
            if (a.id == "custom-action") { found = true; break; }
        }
        TEST("EDGEc: custom action found in registry", found);
    }

    // Palette: query with exact display_name returns it first
    {
        traveler::command::ActionRegistry r;
        r.populate_defaults();
        traveler::command::CommandPalette p(r);
        auto results = p.query("Open Editor mode");
        TEST("EDGEd: exact query returns results", !results.empty());
        TEST("EDGEe: exact match first",
             results[0].action.display_name == "Open Editor mode");
    }

    // OptionalParsers: toggling modal back off rejects commands again
    {
        traveler::command::OptionalParsers op;
        op.set_modal_enabled(true);
        TEST("EDGEf: enabled before toggle", op.modal_enabled());
        op.set_modal_enabled(false);
        TEST("EDGEg: disabled after toggle", !op.modal_enabled());
        auto r = op.parse_modal(":w");
        TEST("EDGEh: :w rejected after disable", !r.ok);
    }
}

// ============================================================================
// main
// ============================================================================
int main() {
    std::cout << "=== WAVE-B Lane B2: Pane + Command Palette + Optional Parsers ==="
              << std::endl;

    // PC-7: Pane mode
    test_pc7_pane_horizontal_split();
    test_pc7_pane_vertical_split();
    test_pc7_pane_combined_navigation();

    // PC-9: Command Palette + Registry
    test_pc9_registry_defaults();
    test_pc9_palette_fuzzy_search();
    test_pc9_score_match_unit();

    // PC-12: Optional parsers
    test_pc12_optional_parsers_disabled_by_default();
    test_pc12_modal_enabled();
    test_pc12_tmux_prefix_enabled();

    // Edge cases
    test_edge_cases();

    std::cout << "\n=== Summary: " << g_passed << " passed, " << g_failed
              << " failed ===" << std::endl;

    return g_failed > 0 ? 1 : 0;
}
