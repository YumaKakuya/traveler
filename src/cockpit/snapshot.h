// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 REQ-COCKPIT-3, REQ-COCKPIT-4, REQ-COCKPIT-6
// Reference: Traveler_Phase0_Spec_v0.1.md §9.2 REQ-COCKPIT-BG-1 (status indicator)
#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

namespace traveler::cockpit {

// ============================================================================
// CockpitSnapshot — frozen snapshot for non-focused callsigns (REQ-COCKPIT-3)
//
// Non-focused mounts hold: model name, last assistant message head (first 256
// chars), status indicator, last_active_at.  No incremental memory/CPU.
//
// REQ-COCKPIT-6: per-callsign snapshot ≤ 256 KB.
// ============================================================================
struct CockpitSnapshot {
    std::string model_name;
    std::string last_message_head;  // first 256 chars of last assistant message
    std::string status;             // "ready", "running", "done", "error", "interrupted"
    std::chrono::system_clock::time_point last_active_at;

    // Constants per spec
    [[nodiscard]] static constexpr std::size_t max_message_head() {
        return 256;
    }
    [[nodiscard]] static constexpr std::size_t max_snapshot_bytes() {
        return 256 * 1024;  // 256 KB
    }
    [[nodiscard]] static constexpr std::size_t max_focused_state_bytes() {
        return 16 * 1024 * 1024;  // 16 MB
    }
};

// ============================================================================
// Snapshot operations (REQ-COCKPIT-4)
// ============================================================================

// Capture a snapshot from current state
[[nodiscard]] CockpitSnapshot capture_snapshot(std::string_view model_name,
                                                std::string_view last_message,
                                                std::string_view status);

// Create a default "ready" snapshot for a newly mounted callsign
[[nodiscard]] CockpitSnapshot make_default_snapshot(std::string_view model_name);

// Estimate the memory footprint of a snapshot in bytes
[[nodiscard]] std::size_t snapshot_size_bytes(const CockpitSnapshot& snapshot);

// Check if a snapshot fits within the 256 KB budget
[[nodiscard]] bool is_snapshot_within_budget(const CockpitSnapshot& snapshot);

}  // namespace traveler::cockpit
