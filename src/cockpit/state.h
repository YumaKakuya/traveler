// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 (Cockpit Mount Lifecycle)
// Reference: Traveler_Phase0_Spec_v0.1.md REQ-COCKPIT-1, REQ-COCKPIT-2, REQ-COCKPIT-3
#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace traveler::cockpit {

// ============================================================================
// CallsignStatus — state machine states per §9.1 diagram
// ============================================================================
enum class CallsignStatus {
    Unmounted,
    Snapshot,   // Mounted, non-focused (frozen snapshot loaded)
    Focused,    // Mounted, focused (full state in memory)
};

[[nodiscard]] const char* to_string(CallsignStatus s);

// ============================================================================
// MountedEntry — per-callsign mount metadata
// ============================================================================
struct MountedEntry {
    CallsignStatus status{CallsignStatus::Snapshot};
    std::chrono::system_clock::time_point mounted_at;
};

// ============================================================================
// CockpitState — top-level cockpit state machine
//
// Holds the mount table and the currently focused callsign (if any).
// REQ-COCKPIT-1: max 4 mounts in cloud mode, 1 in offline mode.
// REQ-COCKPIT-2: exactly one focused callsign when any are mounted.
// ============================================================================
struct CockpitState {
    std::unordered_map<std::string, MountedEntry> mounts;
    std::optional<std::string> focused;
    bool cloud_mode{true};
};

// ============================================================================
// Query helpers
// ============================================================================

[[nodiscard]] bool is_mounted(const CockpitState& state, std::string_view callsign);

[[nodiscard]] std::optional<std::string> focused_callsign(const CockpitState& state);

[[nodiscard]] std::size_t mount_count(const CockpitState& state);

[[nodiscard]] std::size_t max_callsigns(const CockpitState& state);

[[nodiscard]] std::vector<std::string> mounted_callsigns(const CockpitState& state);

[[nodiscard]] CallsignStatus callsign_status(const CockpitState& state,
                                              std::string_view callsign);

}  // namespace traveler::cockpit
