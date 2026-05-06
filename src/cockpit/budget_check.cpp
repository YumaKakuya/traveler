// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 REQ-COCKPIT-6 (Memory budget runtime sanity check)
// Reference: Traveler_Phase0_Spec_v0.1.md §8.2 (CRITICAL safety event emission)
#include "cockpit/budget_check.h"
#include "safety/event.h"

#include <string>

namespace traveler::cockpit {
namespace {

void emit_budget_violation(const std::string& callsign,
                           const std::string& detail) {
    traveler::safety::SafetyEvent event;
    event.severity = traveler::safety::Severity::CRITICAL;
    event.category = traveler::safety::Category::BUDGET;
    event.detail = callsign + ": " + detail;
    event.timestamp = std::chrono::system_clock::now();
    traveler::safety::emit_safety_event(event);
}

}  // namespace

BudgetCheckResult check_snapshot_budget(const CockpitSnapshot& snapshot,
                                         const std::string& callsign) {
    std::size_t size = snapshot_size_bytes(snapshot);
    if (size > CockpitSnapshot::max_snapshot_bytes()) {
        std::string detail = "snapshot size " + std::to_string(size) +
                             " bytes exceeds 256 KB budget";
        emit_budget_violation(callsign, detail);
        return {false, detail};
    }
    return {true, "snapshot within budget (" + std::to_string(size) + " bytes)"};
}

BudgetCheckResult check_full_state_budget(std::size_t state_bytes,
                                           const std::string& callsign) {
    if (state_bytes > CockpitSnapshot::max_focused_state_bytes()) {
        std::string detail = "focused state " + std::to_string(state_bytes) +
                             " bytes exceeds 16 MB budget";
        emit_budget_violation(callsign, detail);
        return {false, detail};
    }
    return {true, "focused state within budget (" + std::to_string(state_bytes) + " bytes)"};
}

BudgetCheckResult check_cockpit_budget(const CockpitState& state,
                                        std::size_t focused_state_bytes) {
    // Check the focused callsign's full state
    if (state.focused.has_value()) {
        auto result = check_full_state_budget(focused_state_bytes, *state.focused);
        if (!result.ok) return result;
    }

    // Check each mounted callsign's snapshot budget
    // REQ-COCKPIT-6: per-callsign snapshot ≤ 256 KB. Inspect an actual
    // CockpitSnapshot object rather than checking struct overhead on the
    // mount table entry.
    for (const auto& [callsign, entry] : state.mounts) {
        if (entry.status == CallsignStatus::Unmounted) continue;

        // Check the actual captured snapshot's memory footprint against
        // the 256 KB budget (REQ-COCKPIT-6).  The snapshot is stored in
        // MountedEntry by the mount/focus/unfocus lifecycle transitions.
        const auto& snapshot = entry.snapshot;
        std::size_t size = snapshot_size_bytes(snapshot);
        if (size > CockpitSnapshot::max_snapshot_bytes()) {
            std::string detail = "callsign " + callsign +
                                 " snapshot size " + std::to_string(size) +
                                 " exceeds 256 KB budget";
            emit_budget_violation(callsign, detail);
            return {false, detail};
        }
    }

    return {true, "all cockpit budgets passed"};
}

}  // namespace traveler::cockpit
