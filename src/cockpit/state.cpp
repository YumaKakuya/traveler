// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 (Cockpit Mount Lifecycle)
#include "cockpit/state.h"

#include <algorithm>

namespace traveler::cockpit {

const char* to_string(CallsignStatus s) {
    switch (s) {
        case CallsignStatus::Unmounted: return "unmounted";
        case CallsignStatus::Snapshot:  return "snapshot";
        case CallsignStatus::Focused:   return "focused";
    }
    return "unknown";
}

bool is_mounted(const CockpitState& state, std::string_view callsign) {
    std::string key(callsign);
    auto it = state.mounts.find(key);
    return it != state.mounts.end() && it->second.status != CallsignStatus::Unmounted;
}

std::optional<std::string> focused_callsign(const CockpitState& state) {
    return state.focused;
}

std::size_t mount_count(const CockpitState& state) {
    return std::count_if(state.mounts.begin(), state.mounts.end(),
                         [](const auto& entry) {
                             return entry.second.status != CallsignStatus::Unmounted;
                         });
}

std::size_t max_callsigns(const CockpitState& state) {
    return state.cloud_mode ? 4 : 1;
}

std::vector<std::string> mounted_callsigns(const CockpitState& state) {
    std::vector<std::string> result;
    result.reserve(state.mounts.size());
    for (const auto& [callsign, entry] : state.mounts) {
        if (entry.status != CallsignStatus::Unmounted) {
            result.push_back(callsign);
        }
    }
    return result;
}

CallsignStatus callsign_status(const CockpitState& state,
                                std::string_view callsign) {
    std::string key(callsign);
    auto it = state.mounts.find(key);
    if (it == state.mounts.end()) return CallsignStatus::Unmounted;
    return it->second.status;
}

}  // namespace traveler::cockpit
