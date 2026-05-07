// Reference: Traveler_Phase0_Spec_v0.1.md §9.5 (Offline Mode Concurrency Cap)
// Reference: Traveler_Phase0_Spec_v0.1.md REQ-OFFLINE-CAP-1, REQ-OFFLINE-CAP-2
// Reference: briefs/wave-c-c1-offline-ayane-dispatch-2026-05-07.md
#include "cockpit/offline_mode.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace traveler::cockpit {

// ============================================================================
// offline_cap_exceeded_message — canonical error text
// ============================================================================

std::string offline_cap_exceeded_message() {
    return "Offline mode supports a single callsign mount due to hardware budget. "
           "Switch to cloud mode to use multiple callsigns.";
}

// ============================================================================
// check_mount_permitted — REQ-OFFLINE-CAP-1
// ============================================================================

tl::expected<void, OfflineModeError>
check_mount_permitted(const CockpitState& state,
                      const OfflineModeTracker& tracker,
                      std::string_view /*callsign*/) {

    if (!tracker.is_offline()) {
        // Cloud mode — delegate to standard mount limit (max 4)
        if (mount_count(state) >= max_callsigns(state)) {
            return tl::make_unexpected(OfflineModeError::CapExceeded(
                "Maximum " + std::to_string(max_callsigns(state)) +
                " callsigns in cloud mode"));
        }
        return {};
    }

    // Offline mode — REQ-OFFLINE-CAP-1: single callsign only
    if (mount_count(state) >= 1) {
        return tl::make_unexpected(OfflineModeError::CapExceeded(
            offline_cap_exceeded_message()));
    }

    return {};
}

// ============================================================================
// switch_to_offline — REQ-OFFLINE-CAP-2: cloud → offline retain-one
// ============================================================================

tl::expected<void, OfflineModeError>
switch_to_offline(CockpitState& state,
                  OfflineModeTracker& tracker,
                  std::string_view retain_callsign) {

    if (tracker.is_offline()) {
        return tl::make_unexpected(OfflineModeError::ModeSwitchConflict(
            "Already in offline mode"));
    }

    // Validate that the retain callsign is actually mounted
    std::string retain_key(retain_callsign);
    if (!is_mounted(state, retain_callsign)) {
        return tl::make_unexpected(OfflineModeError::ModeSwitchConflict(
            "Cannot retain callsign '" + retain_key +
            "': it is not currently mounted"));
    }

    // Unmount all callsigns except the retained one
    std::vector<std::string> to_unmount;
    for (const auto& [callsign, entry] : state.mounts) {
        if (callsign != retain_key && entry.status != CallsignStatus::Unmounted) {
            to_unmount.push_back(callsign);
        }
    }

    for (const auto& cs : to_unmount) {
        auto result = unmount(state, cs);
        if (!result) {
            return tl::make_unexpected(OfflineModeError::ModeSwitchConflict(
                "Failed to unmount callsign '" + cs +
                "' during cloud-to-offline transition: " + result.error().message));
        }
    }

    // Focus the retained callsign if anything is mounted
    if (is_mounted(state, retain_callsign)) {
        auto focus_result = focus(state, retain_callsign);
        if (!focus_result) {
            return tl::make_unexpected(OfflineModeError::ModeSwitchConflict(
                "Failed to focus retained callsign '" + retain_key +
                "': " + focus_result.error().message));
        }
    }

    // Update tracker and state
    tracker.set_mode(OfflineMode::Offline);
    sync_cockpit_state(state, tracker);

    return {};
}

// ============================================================================
// switch_to_cloud — offline → cloud transition
// ============================================================================

tl::expected<void, OfflineModeError>
switch_to_cloud(CockpitState& state,
                OfflineModeTracker& tracker) {

    if (tracker.is_cloud()) {
        return tl::make_unexpected(OfflineModeError::ModeSwitchConflict(
            "Already in cloud mode"));
    }

    // Transition to cloud — lift single-callsign cap
    tracker.set_mode(OfflineMode::Cloud);
    sync_cockpit_state(state, tracker);

    return {};
}

// ============================================================================
// sync_cockpit_state — keep CockpitState.cloud_mode in sync
// ============================================================================

void sync_cockpit_state(CockpitState& state, const OfflineModeTracker& tracker) {
    state.cloud_mode = tracker.is_cloud();
}

// ============================================================================
// verify_offline_single_mount — post-transition validation
// ============================================================================

bool verify_offline_single_mount(const CockpitState& state,
                                  std::string_view retained) {
    const auto count = mount_count(state);
    if (count != 1) return false;

    auto mounted = mounted_callsigns(state);
    if (mounted.empty()) return false;

    return mounted.front() == retained;
}

}  // namespace traveler::cockpit
