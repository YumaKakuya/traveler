// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 (Cockpit Mount Lifecycle)
#include "cockpit/mount.h"

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

    // If this callsign is focused, clear the focus
    if (state.focused == key) {
        state.focused.reset();
    }

    // Mark as unmounted
    state.mounts[key].status = CallsignStatus::Unmounted;

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

    // Unfocus the previous callsign if any (REQ-COCKPIT-4 step 1-2)
    if (state.focused.has_value()) {
        std::string prev = *state.focused;
        if (is_mounted(state, prev)) {
            state.mounts[prev].status = CallsignStatus::Snapshot;
        }
    }

    // Focus the target (REQ-COCKPIT-4 step 3-4)
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

    return {};
}

}  // namespace traveler::cockpit
