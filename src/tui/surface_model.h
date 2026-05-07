#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace traveler::tui {

// ===========================================================================
// PC-1 Evidence — Mode Header Model
// ===========================================================================
// Tracks the visible mode header state. Updates when dispatcher transitions
// modes, providing the "Cockpit" → "Editor" display change without a live
// ScreenLoop.

struct ModeHeader {
    std::string mode_name{"Cockpit"};
    std::string previous_mode_name;
    bool transitioning{false};

    /// Apply a dispatcher-level mode change to the header.
    void transition_to(std::string_view new_mode);

    /// True when the header just changed — UI can use this for a one-frame
    /// transition hint.
    bool just_changed() const noexcept { return transitioning; }

    /// Call after rendering to clear the transition flag.
    void clear_transition() noexcept { transitioning = false; }
};

// ===========================================================================
// PC-1 Evidence — Mode-Dependent Strip Content Model
// ===========================================================================
// Provides the data that the Strip renderer consumes. In Cockpit mode this
// is callsign seat cards; in Editor mode this is a folder-tree +
// breadcrumb placeholder; other modes have empty/default content in Phase 0.

struct CallsignSeat {
    std::string callsign;          // e.g. "@vega"
    std::string model_name;        // e.g. "claude-opus-4-7"
    std::string status;            // e.g. "ready", "streaming"
    std::string last_message_head; // first ~60 chars of last assistant msg
    bool focused{false};
};

struct FolderEntry {
    std::string name;              // file or directory name
    bool is_dir{false};            // true = directory (expandable)
    bool expanded{false};          // only meaningful when is_dir
    int depth{0};                  // nesting level for indentation
};

struct StripContent {
    // --- Mode tag: which mode owns this content ---
    std::string mode{"Cockpit"};

    // --- Cockpit-mode content ---
    std::vector<CallsignSeat> cockpit_seats;
    int focused_seat_index{0};

    // --- Editor-mode content (placeholder — real data is Phase 1+) ---
    std::vector<FolderEntry> folder_tree;
    std::string file_breadcrumb;   // e.g. "src/tui/surface_model.h"

    /// Populate content for the given mode.  Cockpit seats are sourced from
    /// the callsign roster; Editor content is a placeholder tree.
    void set_mode(std::string_view new_mode,
                  const std::vector<std::string>& callsign_roster);

    /// True when the Strip is in Cockpit layout.
    bool is_cockpit() const noexcept { return mode == "Cockpit"; }

    /// True when the Strip is in Editor layout.
    bool is_editor() const noexcept { return mode == "Editor"; }

    /// Number of visible cockpit seats.
    std::size_t seat_count() const noexcept { return cockpit_seats.size(); }

    /// Number of visible folder-tree entries.
    std::size_t tree_entry_count() const noexcept { return folder_tree.size(); }
};

// ===========================================================================
// PC-2 Evidence — Leader Cheatsheet Model
// ===========================================================================
// Pure-data model for the leader cheatsheet overlay.  The TUI opens this
// overlay on `space` and renders keys + display-names.  Pressing a key
// dispatches the corresponding command.

struct LeaderCheatsheetEntry {
    char key;                      // single keystroke
    std::string command;           // dispatch command (e.g. "Editor")
    std::string display_name;      // human label (e.g. "Editor mode")
};

struct LeaderCheatsheet {
    bool visible{false};
    std::vector<LeaderCheatsheetEntry> entries;

    void open() noexcept;
    void close() noexcept;

    /// Populate with the canonical P0-2 binding set:
    ///   e Editor  l LLM  c Cockpit  p Pane  g Git
    /// These match Spec §6.2 leader parser + Brief B5 item 3.
    void load_defaults();

    /// Look up an entry by key.  Returns nullptr when not found.
    const LeaderCheatsheetEntry* find(char key) const noexcept;

    /// True when the cheatsheet is currently visible.
    bool is_visible() const noexcept { return visible; }

    std::size_t entry_count() const noexcept { return entries.size(); }
};

// ===========================================================================
// PC-3 Evidence — @ Autocomplete Model
// ===========================================================================
// Ranks callsign completions from the fixed four-roster.  @vega MUST appear
// first when the query fragment is "@v".

struct AutocompleteEntry {
    std::string callsign;          // e.g. "@vega"
    bool exact_prefix{false};      // true when query is an exact prefix match
};

struct AutocompleteModel {
    std::string query_fragment;    // the fragment after '@'
    std::vector<AutocompleteEntry> results;
    bool visible{false};

    /// Compute ranked completions for a Tower input string.  Internally
    /// calls into command::complete_callsign().
    void compute(std::string_view tower_input);

    /// Reset to empty.
    void clear() noexcept;

    /// First result — must be @vega when query is "@v".
    std::string_view top_callsign() const noexcept;

    std::size_t result_count() const noexcept { return results.size(); }

    bool is_visible() const noexcept { return visible; }
};

// ===========================================================================
// PC-4 Evidence — Separator Drag Model
// ===========================================================================
// Records drag-resize state and update semantics.  Each drag event
// captures heights + timestamps so the TUI layer can verify real-time
// responsiveness.

struct DragEvent {
    int old_strip_height;          // height before this event
    int new_strip_height;          // height after this event
    int delta;                     // change (positive = taller Strip)
    std::chrono::steady_clock::time_point timestamp;
};

class SeparatorDragModel {
public:
    SeparatorDragModel();

    int strip_height() const noexcept { return strip_height_; }
    int tower_height() const noexcept { return tower_height_; }
    bool drag_active() const noexcept { return drag_active_; }
    int event_count() const noexcept { return event_count_; }

    /// Start a drag operation.  cursor_y is the current mouse/cursor row.
    void begin_drag(int cursor_y);

    /// Continue a drag.  terminal_height is the total terminal rows.
    void update_drag(int cursor_y, int terminal_height);

    /// Finish the drag.  Leaves heights at their last-updated values.
    void end_drag();

    /// Return a copy of the most recent drag event.
    /// If no events have occurred, returns a zeroed event.
    DragEvent last_event() const noexcept;

    /// All recorded events (for batch inspection).
    const std::vector<DragEvent>& events() const noexcept { return events_; }

    /// Timestamp of the most recent update (begin/update/end).
    std::chrono::steady_clock::time_point last_update_time() const noexcept {
        return last_update_;
    }

private:
    int strip_height_{3};
    int tower_height_{5};
    bool drag_active_{false};
    int drag_origin_y_{0};
    int drag_origin_strip_height_{3};
    int event_count_{0};
    std::chrono::steady_clock::time_point last_update_;
    std::vector<DragEvent> events_;
};

// ===========================================================================
// PC-11 Evidence — Mode-Switch Latency Harness
// ===========================================================================
// Collects timing samples from mode transitions and reports PASS / FAIL
// against the 50 ms median threshold.  The harness is pure C++ (no FTXUI)
// so it works without a live ScreenLoop.

struct LatencySample {
    std::string from_mode;
    std::string to_mode;
    std::chrono::nanoseconds duration_ns;
};

class ModeSwitchHarness {
public:
    ModeSwitchHarness();

    void clear() noexcept;

    /// Record a single transition.
    void record(std::string_view from, std::string_view to,
                std::chrono::nanoseconds duration);

    std::size_t trial_count() const noexcept { return samples_.size(); }

    /// Median latency across all recorded trials.
    std::chrono::nanoseconds median_latency() const;

    /// Worst-case latency.
    std::chrono::nanoseconds max_latency() const;

    /// Best-case latency.
    std::chrono::nanoseconds min_latency() const;

    /// True when median < threshold (default 50 ms = PC-11 threshold).
    bool passes(double threshold_ms = 50.0) const;

    /// The Spec §6 PC-11 description string for Core-i3 deferral.
    static constexpr std::string_view core_i3_deferral_note() {
        return "Core-i3 external evidence unavailable — benchmarked on "
               "current (non-Core-i3) hardware only.";
    }

    const std::vector<LatencySample>& samples() const noexcept {
        return samples_;
    }

private:
    std::vector<LatencySample> samples_;
};

}  // namespace traveler::tui
