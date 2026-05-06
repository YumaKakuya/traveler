// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 (Cockpit Mount Lifecycle)
#include "cockpit/mount.h"
#include "cockpit/snapshot.h"

#include <algorithm>
#include <chrono>

namespace traveler::cockpit {

tl::expected<void, MountError>
mount(CockpitState& state, std::string_view callsign) {
    std::string key(callsign);

    // Check if already mounted
    if (is_mounted(state, key)) {
        return tl::make_unexpected(
            MountError::AlreadyMounted(
                std::string(callsign) + " is already mounted"));
    }

    // Check mount limit (REQ-COCKPIT-1)
    const auto max = max_callsigns(state);
    if (mount_count(state) >= max) {
        return tl::make_unexpected(
            MountError::LimitExceeded(
                std::string("Cannot mount ") + std::string(callsign) +
                ": limit of " + std::to_string(max) + " callsign(s) reached"));
    }

    // Mount as Snapshot (non-focused)
    MountedEntry entry;
    entry.status = CallsignStatus::Snapshot;
    entry.mounted_at = std::chrono::system_clock::now();
    state.mounts[key] = entry;

    // If this is the first mount, auto-focus it
    if (!state.focused.has_value()) {
        state.focused = key;
        state.mounts[key].status = CallsignStatus::Focused;
    }

    return {};
}

tl::expected<void, MountError>
unmount(CockpitState& state, std::string_view callsign) {
    std::string key(callsign);

    // Check if mounted
    if (!is_mounted(state, key)) {
        return tl::make_unexpected(
            MountError::NotMounted(
                std::string(callsign) + " is not mounted"));
    }

    bool was_focused = (state.focused == key);

    // Mark as unmounted
    state.mounts[key].status = CallsignStatus::Unmounted;

    // REQ-COCKPIT-2: If the focused callsign was unmounted,
    // auto-focus another mounted callsign if any remain.
    if (was_focused) {
        state.focused.reset();
        auto remaining = mounted_callsigns(state);
        if (!remaining.empty()) {
            state.focused = remaining.front();
            state.mounts[remaining.front()].status = CallsignStatus::Focused;
        }
    }

    return {};
}

tl::expected<void, MountError>
focus(CockpitState& state, std::string_view callsign) {
    std::string key(callsign);

    // Check if mounted
    if (!is_mounted(state, key)) {
        return tl::make_unexpected(
            MountError::NotMounted(
                std::string(callsign) + " is not mounted"));
    }

    // Already focused — no error, just no-op
    if (state.focused == key) {
        return {};
    }

    // REQ-COCKPIT-4 step 1: Capture current snapshot before switching
    if (state.focused.has_value()) {
        std::string prev = *state.focused;
        if (is_mounted(state, prev)) {
            // Capture a snapshot of the current focused callsign
            (void)capture_snapshot(prev, "", to_string(CallsignStatus::Snapshot));
            // Transition to Snapshot status
            state.mounts[prev].status = CallsignStatus::Snapshot;
        }
    }

    // REQ-COCKPIT-4 step 2-3: Reconstruct target from snapshot+SQLite
    // Phase 0: invoke capture to simulate restore of the target's snapshot state
    (void)capture_snapshot(key, "", to_string(CallsignStatus::Focused));

    // REQ-COCKPIT-4 step 4: Render — set focus and status
    state.focused = key;
    state.mounts[key].status = CallsignStatus::Focused;

    return {};
}

tl::expected<void, MountError>
unfocus(CockpitState& state) {
    // Check if any callsign is focused
    if (!state.focused.has_value()) {
        return tl::make_unexpected(
            MountError::NotMounted("No callsign is focused"));
    }

    std::string prev = *state.focused;
    if (is_mounted(state, prev)) {
        state.mounts[prev].status = CallsignStatus::Snapshot;
    }
    state.focused.reset();

    // REQ-COCKPIT-2: If other callsigns remain mounted, auto-focus
    // the first non-current one. Do not leave zero-focused state when
    // mounts exist.
    auto remaining = mounted_callsigns(state);
    if (!remaining.empty()) {
        // Prefer a callsign other than the one we just unfocused
        for (const auto& cs : remaining) {
            if (cs != prev) {
                state.focused = cs;
                state.mounts[cs].status = CallsignStatus::Focused;
                break;
            }
        }
        // If only the previously-focused callsign is mounted, re-focus it
        if (!state.focused.has_value()) {
            state.focused = remaining.front();
            state.mounts[remaining.front()].status = CallsignStatus::Focused;
        }
    }

    return {};
}

}  // namespace traveler::cockpit
