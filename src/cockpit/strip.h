// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 (Cockpit Mount Lifecycle, Strip)
// Reference: Traveler_Phase0_Spec_v0.1.md REQ-COCKPIT-2, REQ-COCKPIT-4 step 4
// Reference: Traveler_Phase0_Spec_v0.1.md §9.2 REQ-COCKPIT-BG-1 (status indicator)
#pragma once

#include <ftxui/dom/elements.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace traveler::cockpit {

// ============================================================================
// Cockpit Strip bar — renders the callsign roster at the top of the Cockpit
//
// Shows mounted callsigns with the focused one highlighted. Each callsign seat
// shows: callsign label, status indicator (ready/running/done/error).
//
// REQ-COCKPIT-2: exactly one focused highlighted.
// REQ-COCKPIT-4 step 4: render four-callsign Strip with @B highlighted.
// REQ-COCKPIT-BG-1: status indicator on Strip seat.
// ============================================================================

// Renders the full callsign Strip bar as an FTXUI Element.
//
// Args:
//   mounted_callsigns: list of currently mounted callsign names
//   focused_callsign: the currently focused callsign (if any, "" if none)
//   statuses: per-callsign status strings ("ready", "running", "done", "error")
//             Must match mounted_callsigns in size.
[[nodiscard]] ftxui::Element render_strip(
    const std::vector<std::string>& mounted_callsigns,
    std::string_view focused_callsign,
    const std::vector<std::string>& statuses);

// Renders a compact one-line Strip bar (just callsign labels)
[[nodiscard]] ftxui::Element render_strip_compact(
    const std::vector<std::string>& mounted_callsigns,
    std::string_view focused_callsign);

// Format a single callsign seat for display
[[nodiscard]] std::string format_seat(std::string_view callsign,
                                      std::string_view status,
                                      bool focused);

}  // namespace traveler::cockpit
