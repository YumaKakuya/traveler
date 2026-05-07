#include "pane/model.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace traveler::pane {

PaneLayout::PaneLayout() {
    // Start with a single root pane at (0, 0).
    panes_.push_back({next_id(), 0, 0, "Cockpit"});
    active_index_ = 0;
}

// ---- split ----

int PaneLayout::split_horizontal() {
    if (panes_.empty()) return -1;

    const PaneInfo& active = panes_[static_cast<std::size_t>(active_index_)];

    // Grow the grid: new row at the bottom.
    ++rows_;

    // New pane at the new bottom row, same column as the active pane.
    const int new_row = rows_ - 1;
    const int new_id = next_id();
    panes_.push_back({new_id, new_row, active.col, active.mode});

    // Make the new pane active.
    active_index_ = static_cast<int>(panes_.size()) - 1;
    return new_id;
}

int PaneLayout::split_vertical() {
    if (panes_.empty()) return -1;

    const PaneInfo& active = panes_[static_cast<std::size_t>(active_index_)];

    // Grow the grid: new column at the right.
    ++cols_;

    // New pane at the new rightmost column, same row as active.
    const int new_col = cols_ - 1;
    const int new_id = next_id();
    panes_.push_back({new_id, active.row, new_col, active.mode});

    active_index_ = static_cast<int>(panes_.size()) - 1;
    return new_id;
}

// ---- navigation ----
// Find the nearest pane in the given direction using Manhattan distance.
// If no pane exists in that direction, this is a no-op.

namespace {

int manhattan_distance(int r1, int c1, int r2, int c2) {
    return std::abs(r1 - r2) + std::abs(c1 - c2);
}

}  // namespace

void PaneLayout::navigate_up() {
    if (panes_.empty()) return;
    const PaneInfo& active = panes_[static_cast<std::size_t>(active_index_)];

    int best_idx = -1;
    int best_dist = std::numeric_limits<int>::max();
    for (int i = 0; i < static_cast<int>(panes_.size()); ++i) {
        const auto& p = panes_[static_cast<std::size_t>(i)];
        if (p.row >= active.row) continue;  // must be above
        int d = manhattan_distance(p.row, p.col, active.row, active.col);
        // Prefer same column
        if (p.col == active.col) d -= 100;
        if (d < best_dist) {
            best_dist = d;
            best_idx = i;
        }
    }
    if (best_idx >= 0) {
        active_index_ = best_idx;
    }
}

void PaneLayout::navigate_down() {
    if (panes_.empty()) return;
    const PaneInfo& active = panes_[static_cast<std::size_t>(active_index_)];

    int best_idx = -1;
    int best_dist = std::numeric_limits<int>::max();
    for (int i = 0; i < static_cast<int>(panes_.size()); ++i) {
        const auto& p = panes_[static_cast<std::size_t>(i)];
        if (p.row <= active.row) continue;  // must be below
        int d = manhattan_distance(p.row, p.col, active.row, active.col);
        if (p.col == active.col) d -= 100;
        if (d < best_dist) {
            best_dist = d;
            best_idx = i;
        }
    }
    if (best_idx >= 0) {
        active_index_ = best_idx;
    }
}

void PaneLayout::navigate_left() {
    if (panes_.empty()) return;
    const PaneInfo& active = panes_[static_cast<std::size_t>(active_index_)];

    int best_idx = -1;
    int best_dist = std::numeric_limits<int>::max();
    for (int i = 0; i < static_cast<int>(panes_.size()); ++i) {
        const auto& p = panes_[static_cast<std::size_t>(i)];
        if (p.col >= active.col) continue;  // must be to the left
        int d = manhattan_distance(p.row, p.col, active.row, active.col);
        if (p.row == active.row) d -= 100;
        if (d < best_dist) {
            best_dist = d;
            best_idx = i;
        }
    }
    if (best_idx >= 0) {
        active_index_ = best_idx;
    }
}

void PaneLayout::navigate_right() {
    if (panes_.empty()) return;
    const PaneInfo& active = panes_[static_cast<std::size_t>(active_index_)];

    int best_idx = -1;
    int best_dist = std::numeric_limits<int>::max();
    for (int i = 0; i < static_cast<int>(panes_.size()); ++i) {
        const auto& p = panes_[static_cast<std::size_t>(i)];
        if (p.col <= active.col) continue;  // must be to the right
        int d = manhattan_distance(p.row, p.col, active.row, active.col);
        if (p.row == active.row) d -= 100;
        if (d < best_dist) {
            best_dist = d;
            best_idx = i;
        }
    }
    if (best_idx >= 0) {
        active_index_ = best_idx;
    }
}

// ---- accessors ----

PaneInfo* PaneLayout::active_pane() {
    if (panes_.empty()) return nullptr;
    return &panes_[static_cast<std::size_t>(active_index_)];
}

const PaneInfo* PaneLayout::active_pane() const {
    if (panes_.empty()) return nullptr;
    return &panes_[static_cast<std::size_t>(active_index_)];
}

int PaneLayout::active_pane_number() const {
    // 1-indexed display number.
    return active_index_ + 1;
}

// ---- private ----

int PaneLayout::find_pane_at(int row, int col) const {
    for (int i = 0; i < static_cast<int>(panes_.size()); ++i) {
        if (panes_[static_cast<std::size_t>(i)].row == row &&
            panes_[static_cast<std::size_t>(i)].col == col) {
            return i;
        }
    }
    return -1;
}

}  // namespace traveler::pane
