// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 (Cockpit Mount Lifecycle)
// Reference: Traveler_Phase0_Spec_v0.1.md REQ-COCKPIT-1, REQ-COCKPIT-4
#pragma once

#include "cockpit/state.h"

#include <string>
#include <string_view>
#include <tl/expected.hpp>

namespace traveler::cockpit {

// ============================================================================
// MountError — error type for mount/unmount/focus operations
// ============================================================================
struct MountError {
    std::string message;

    [[nodiscard]] static MountError LimitExceeded(std::string msg) {
        return {std::move(msg)};
    }
    [[nodiscard]] static MountError AlreadyMounted(std::string msg) {
        return {std::move(msg)};
    }
    [[nodiscard]] static MountError NotMounted(std::string msg) {
        return {std::move(msg)};
    }
    [[nodiscard]] static MountError InvalidCallsign(std::string msg) {
        return {std::move(msg)};
    }
};

// ============================================================================
// Mount operations — state machine transitions per §9.1
//
// mount:   [Unmounted] → [Mounted, Snapshot]
// unmount: [Mounted, *] → [Unmounted]
// focus:   [Mounted, Snapshot] → [Mounted, Focused]
// unfocus: [Mounted, Focused] → [Mounted, Snapshot]
// ============================================================================

// Mount a callsign. Fails if limit exceeded or already mounted.
// REQ-COCKPIT-1: max 4 cloud, 1 offline.
[[nodiscard]] tl::expected<void, MountError>
mount(CockpitState& state, std::string_view callsign);

// Unmount a callsign. Fails if not mounted.
// If the callsign was focused, focus is cleared.
[[nodiscard]] tl::expected<void, MountError>
unmount(CockpitState& state, std::string_view callsign);

// Focus a mounted callsign. Fails if not mounted.
// REQ-COCKPIT-4: Capture current → restore target.
// If another callsign is already focused, it is unfocused first.
[[nodiscard]] tl::expected<void, MountError>
focus(CockpitState& state, std::string_view callsign);

// Unfocus the currently focused callsign (back to Snapshot).
// Fails if no callsign is focused.
[[nodiscard]] tl::expected<void, MountError>
unfocus(CockpitState& state);

}  // namespace traveler::cockpit
