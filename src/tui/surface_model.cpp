#include "tui/surface_model.h"

#include "command/parser.h"

#include <algorithm>

namespace traveler::tui {

// ===========================================================================
// ModeHeader
// ===========================================================================

void ModeHeader::transition_to(std::string_view new_mode) {
    previous_mode_name = mode_name;
    mode_name = std::string(new_mode);
    transitioning = true;
}

// ===========================================================================
// StripContent
// ===========================================================================

void StripContent::set_mode(std::string_view new_mode,
                            const std::vector<std::string>& callsign_roster) {
    mode = std::string(new_mode);

    // --- Cockpit: populate callsign seat cards ---
    if (mode == "Cockpit") {
        cockpit_seats.clear();
        for (std::size_t i = 0; i < callsign_roster.size() && i < 4; ++i) {
            CallsignSeat seat;
            seat.callsign = callsign_roster[i];
            seat.model_name = (i == 0) ? "claude-opus-4-7" : "claude-sonnet-4-7";
            seat.status = (i == 0) ? "ready" : "standby";
            seat.last_message_head = "Last message will appear here.";
            seat.focused = (i == 0);
            cockpit_seats.push_back(std::move(seat));
        }
        focused_seat_index = 0;

        // Clear editor data so the renderer can check is_editor()/is_cockpit().
        folder_tree.clear();
        file_breadcrumb.clear();
        return;
    }

    // --- Editor: placeholder folder tree + breadcrumb ---
    if (mode == "Editor") {
        cockpit_seats.clear();
        focused_seat_index = -1;

        folder_tree = {
            {"src/",            true,  true,  0},
            {"  core/",         true,  false, 1},
            {"  tui/",          true,  false, 1},
            {"  command/",      true,  false, 1},
            {"  editor/",       true,  false, 1},
            {"docs/",           true,  false, 0},
            {"  spec/",         true,  false, 1},
            {"tests/",          true,  false, 0},
            {"CMakeLists.txt",  false, false, 0},
        };
        file_breadcrumb = "src/tui/surface_model.cpp";
        return;
    }

    // --- Other modes: empty/default content ---
    cockpit_seats.clear();
    folder_tree.clear();
    file_breadcrumb.clear();
}

// ===========================================================================
// LeaderCheatsheet
// ===========================================================================

void LeaderCheatsheet::open() noexcept {
    visible = true;
}

void LeaderCheatsheet::close() noexcept {
    visible = false;
}

void LeaderCheatsheet::load_defaults() {
    entries.clear();

    // Canonical P0-2 binding set per Spec §6.2 / Brief B5 item 3:
    // e Editor, l LLM, c Cockpit, p Pane, g Git
    entries.push_back({'e', "Editor",  "Editor mode"});
    entries.push_back({'l', "LLM",     "LLM mode"});
    entries.push_back({'c', "Cockpit", "Cockpit mode"});
    entries.push_back({'p', "Pane",    "Pane mode"});
    entries.push_back({'g', "Git",     "Git mode"});

    // Extra binding from the canonical parser roster (non-Brief but present):
    entries.push_back({'s', "Split",   "Horizontal split"});
}

const LeaderCheatsheetEntry* LeaderCheatsheet::find(char key) const noexcept {
    for (const auto& entry : entries) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

// ===========================================================================
// AutocompleteModel
// ===========================================================================

void AutocompleteModel::compute(std::string_view tower_input) {
    // Delegate to the command-layer parser for correct roster logic.
    // The parser already ranks @vega first for prefix "@v".
    auto raw = command::complete_callsign(tower_input);

    results.clear();
    for (const auto& callsign : raw) {
        AutocompleteEntry entry;
        entry.callsign = callsign;
        // An entry is an exact-prefix match when the callsign starts with the
        // query fragment.  Re-derive from the complete_callsign result rather
        // than duplicating parser logic.
        std::string fragment;
        auto at = tower_input.rfind('@');
        if (at != std::string_view::npos) {
            fragment = tower_input.substr(at);
        }
        entry.exact_prefix =
            fragment.empty() ? false
                             : (callsign.size() >= fragment.size() &&
                                callsign.compare(0, fragment.size(), fragment) == 0);
        results.push_back(std::move(entry));
    }

    query_fragment = std::string(tower_input);
    visible = !results.empty();
}

void AutocompleteModel::clear() noexcept {
    query_fragment.clear();
    results.clear();
    visible = false;
}

std::string_view AutocompleteModel::top_callsign() const noexcept {
    if (results.empty()) {
        return {};
    }
    return results.front().callsign;
}

// ===========================================================================
// SeparatorDragModel
// ===========================================================================

SeparatorDragModel::SeparatorDragModel()
    : last_update_(std::chrono::steady_clock::now()) {}

void SeparatorDragModel::begin_drag(int cursor_y) {
    drag_active_ = true;
    drag_origin_y_ = cursor_y;
    drag_origin_strip_height_ = strip_height_;
    last_update_ = std::chrono::steady_clock::now();
}

void SeparatorDragModel::update_drag(int cursor_y, int terminal_height) {
    if (!drag_active_) {
        return;
    }

    const int delta = cursor_y - drag_origin_y_;
    int new_strip = drag_origin_strip_height_ + delta;

    // Clamp to reasonable bounds.
    if (new_strip < 1) new_strip = 1;
    const int max_strip = terminal_height - tower_height_ - 1;
    if (new_strip > max_strip && max_strip > 0) new_strip = max_strip;

    const int old_strip = strip_height_;
    strip_height_ = new_strip;

    DragEvent event;
    event.old_strip_height = old_strip;
    event.new_strip_height = strip_height_;
    event.delta = strip_height_ - old_strip;
    event.timestamp = std::chrono::steady_clock::now();

    events_.push_back(event);
    ++event_count_;
    last_update_ = event.timestamp;
}

void SeparatorDragModel::end_drag() {
    drag_active_ = false;
    last_update_ = std::chrono::steady_clock::now();
}

DragEvent SeparatorDragModel::last_event() const noexcept {
    if (events_.empty()) {
        return DragEvent{};
    }
    return events_.back();
}

// ===========================================================================
// ModeSwitchHarness
// ===========================================================================

ModeSwitchHarness::ModeSwitchHarness() = default;

void ModeSwitchHarness::clear() noexcept {
    samples_.clear();
}

void ModeSwitchHarness::record(std::string_view from, std::string_view to,
                               std::chrono::nanoseconds duration) {
    samples_.push_back(LatencySample{
        std::string(from), std::string(to), duration});
}

std::chrono::nanoseconds ModeSwitchHarness::median_latency() const {
    if (samples_.empty()) {
        return std::chrono::nanoseconds{0};
    }

    // Copy durations for sorting.
    std::vector<std::chrono::nanoseconds> durations;
    durations.reserve(samples_.size());
    for (const auto& s : samples_) {
        durations.push_back(s.duration_ns);
    }
    std::sort(durations.begin(), durations.end());

    const std::size_t n = durations.size();
    if (n % 2 == 1) {
        return durations[n / 2];
    }
    // Even count: average of two middle elements.
    const auto a = durations[n / 2 - 1];
    const auto b = durations[n / 2];
    return std::chrono::nanoseconds((a.count() + b.count()) / 2);
}

std::chrono::nanoseconds ModeSwitchHarness::max_latency() const {
    if (samples_.empty()) {
        return std::chrono::nanoseconds{0};
    }
    auto it = std::max_element(
        samples_.begin(), samples_.end(),
        [](const LatencySample& a, const LatencySample& b) {
            return a.duration_ns < b.duration_ns;
        });
    return it->duration_ns;
}

std::chrono::nanoseconds ModeSwitchHarness::min_latency() const {
    if (samples_.empty()) {
        return std::chrono::nanoseconds{0};
    }
    auto it = std::min_element(
        samples_.begin(), samples_.end(),
        [](const LatencySample& a, const LatencySample& b) {
            return a.duration_ns < b.duration_ns;
        });
    return it->duration_ns;
}

bool ModeSwitchHarness::passes(double threshold_ms) const {
    if (samples_.empty()) {
        return false;
    }
    const auto threshold_ns =
        std::chrono::nanoseconds(static_cast<long long>(threshold_ms * 1'000'000.0));
    return median_latency() < threshold_ns;
}

}  // namespace traveler::tui
