// Reference: Traveler_Phase0_Spec_v0.1.md §9.5 (Offline Mode Concurrency Cap)
// Reference: Traveler_Phase0_Spec_v0.1.md REQ-OFFLINE-CAP-1, REQ-OFFLINE-CAP-2
// Reference: briefs/wave-c-c1-offline-ayane-dispatch-2026-05-07.md
//
// Offline mode state/control: enforces single-callsign cap and
// cloud-to-offline retain-one behavior at state level.
#pragma once

#include "cockpit/mount.h"
#include "cockpit/state.h"

#include <optional>
#include <string>
#include <string_view>
#include <tl/expected.hpp>

namespace traveler::cockpit {

// ============================================================================
// OfflineModeStatus — tracks the offline/cloud mode and related constraints
// ============================================================================
enum class OfflineMode {
    Cloud,     // Cloud mode: up to 4 callsigns
    Offline,   // Offline mode: exactly 1 callsign (hardware budget)
};

[[nodiscard]] constexpr const char* to_string(OfflineMode m) noexcept {
    switch (m) {
        case OfflineMode::Cloud:   return "cloud";
        case OfflineMode::Offline: return "offline";
    }
    return "unknown";
}

// ============================================================================
// OfflineModeError — error type for offline mode operations
// ============================================================================
struct OfflineModeError {
    std::string message;

    [[nodiscard]] static OfflineModeError CapExceeded(std::string msg) {
        return {std::move(msg)};
    }
    [[nodiscard]] static OfflineModeError ModeSwitchConflict(std::string msg) {
        return {std::move(msg)};
    }
};

// ============================================================================
// OfflineModeTracker — immutable-in-result, tracks current operating mode
// ============================================================================
class OfflineModeTracker {
public:
    OfflineModeTracker() = default;
    explicit OfflineModeTracker(OfflineMode initial_mode) : mode_(initial_mode) {}

    // Current mode
    [[nodiscard]] OfflineMode mode() const noexcept { return mode_; }

    // Maximum allowed callsigns for current mode
    [[nodiscard]] std::size_t max_callsigns() const noexcept {
        return mode_ == OfflineMode::Cloud ? 4 : 1;
    }

    // Whether currently in offline mode
    [[nodiscard]] bool is_offline() const noexcept {
        return mode_ == OfflineMode::Offline;
    }

    // Whether currently in cloud mode
    [[nodiscard]] bool is_cloud() const noexcept {
        return mode_ == OfflineMode::Cloud;
    }

    // Set mode directly (for initialization / testing)
    void set_mode(OfflineMode m) noexcept { mode_ = m; }

private:
    OfflineMode mode_{OfflineMode::Cloud};
};

// ============================================================================
// Offline mode transition operations
// ============================================================================

// Attempt to switch CockpitState from cloud to offline mode.
//
// REQ-OFFLINE-CAP-2: If > 1 callsign is mounted, prompts the caller to choose
// which single callsign to retain. The `retain` parameter specifies the callsign
// to keep; all others are unmounted.
//
// Returns error if:
//   - The retain callsign is not mounted
//   - Mode is already offline
[[nodiscard]] tl::expected<void, OfflineModeError>
switch_to_offline(CockpitState& state,
                  OfflineModeTracker& tracker,
                  std::string_view retain_callsign);

// Switch from offline to cloud mode.
// Clears the single-callsign cap. All currently mounted callsigns remain.
[[nodiscard]] tl::expected<void, OfflineModeError>
switch_to_cloud(CockpitState& state,
                OfflineModeTracker& tracker);

// Check whether a new callsign mount is permitted under the current mode.
// REQ-OFFLINE-CAP-1: In offline mode, mounting a second callsign is forbidden.
[[nodiscard]] tl::expected<void, OfflineModeError>
check_mount_permitted(const CockpitState& state,
                      const OfflineModeTracker& tracker,
                      std::string_view callsign);

// Get the human-readable error message for offline mount cap exceeded.
[[nodiscard]] std::string offline_cap_exceeded_message();

// Synchronize the CockpitState cloud_mode flag with the tracker.
// Call after mode transitions to keep state consistent.
void sync_cockpit_state(CockpitState& state, const OfflineModeTracker& tracker);

// After a cloud-to-offline transition with retain choice, validate that
// exactly one callsign is mounted and it's the retained one.
[[nodiscard]] bool verify_offline_single_mount(const CockpitState& state,
                                                std::string_view retained);

}  // namespace traveler::cockpit
