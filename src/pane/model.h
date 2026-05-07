#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace traveler::pane {

/// A single pane in the pane layout tree.
///
/// Each pane is a mode container (REQ-PANE-3); the `mode` field stores the
/// name of the mode currently hosted in this pane.
struct PaneInfo {
    int id{0};
    int row{0};
    int col{0};
    std::string mode;  // e.g. "Editor", "Cockpit", "LLM", "Pane", "Git"
};

/// The pane layout model — pure state, no TUI rendering.
///
/// Phase 0 scope (REQ-PANE-1):
///   - horizontal split (Ctrl+B ")
///   - vertical split   (Ctrl+B %)
///   - arrow-key navigation between panes
///
/// The layout tracks panes in a row×col grid. Splitting grows the grid and
/// repositions existing panes to fill the new shape.
class PaneLayout {
public:
    PaneLayout();

    // ---- split ----

    /// Create a new pane below the active pane (horizontal split).
    /// The new pane becomes active.
    /// Returns the new pane's id.
    int split_horizontal();

    /// Create a new pane to the right of the active pane (vertical split).
    /// The new pane becomes active.
    /// Returns the new pane's id.
    int split_vertical();

    // ---- navigation ----

    /// Move active focus to the pane nearest in the given direction.
    /// If no pane exists in that direction, this is a no-op.
    void navigate_up();
    void navigate_down();
    void navigate_left();
    void navigate_right();

    // ---- accessors ----

    /// Pointer to the currently active pane, or nullptr if the layout is empty.
    PaneInfo* active_pane();
    const PaneInfo* active_pane() const;

    /// Number of panes in the layout.
    std::size_t pane_count() const noexcept { return panes_.size(); }

    /// The 1-indexed active pane number (for display).
    int active_pane_number() const;

    /// All panes.
    const std::vector<PaneInfo>& panes() const noexcept { return panes_; }

    /// Grid dimensions.
    int grid_rows() const noexcept { return rows_; }
    int grid_cols() const noexcept { return cols_; }

private:
    /// Find the index of a pane by (row, col) or return -1.
    int find_pane_at(int row, int col) const;

    /// Assign a new unique id.
    int next_id() { return next_id_++; }

    std::vector<PaneInfo> panes_;
    int active_index_{0};
    int next_id_{1};
    int rows_{1};
    int cols_{1};
};

}  // namespace traveler::pane
