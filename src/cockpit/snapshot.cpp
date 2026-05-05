// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 REQ-COCKPIT-3, REQ-COCKPIT-4, REQ-COCKPIT-6
#include "cockpit/snapshot.h"

#include <algorithm>
#include <chrono>

namespace traveler::cockpit {

CockpitSnapshot capture_snapshot(std::string_view model_name,
                                  std::string_view last_message,
                                  std::string_view status) {
    CockpitSnapshot snap;
    snap.model_name = std::string(model_name);
    // Truncate last message head to 256 chars (REQ-COCKPIT-3)
    snap.last_message_head = last_message.size() > CockpitSnapshot::max_message_head()
                                 ? std::string(last_message.substr(0, CockpitSnapshot::max_message_head()))
                                 : std::string(last_message);
    snap.status = std::string(status);
    snap.last_active_at = std::chrono::system_clock::now();
    return snap;
}

CockpitSnapshot make_default_snapshot(std::string_view model_name) {
    CockpitSnapshot snap;
    snap.model_name = std::string(model_name);
    snap.status = "ready";
    snap.last_active_at = std::chrono::system_clock::now();
    return snap;
}

std::size_t snapshot_size_bytes(const CockpitSnapshot& snapshot) {
    // Estimate memory footprint: sum of string capacities + fixed overhead
    return sizeof(CockpitSnapshot) +
           snapshot.model_name.capacity() +
           snapshot.last_message_head.capacity() +
           snapshot.status.capacity();
}

bool is_snapshot_within_budget(const CockpitSnapshot& snapshot) {
    return snapshot_size_bytes(snapshot) <= CockpitSnapshot::max_snapshot_bytes();
}

}  // namespace traveler::cockpit
