// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 REQ-COCKPIT-6 (Memory budget runtime sanity check)
// Reference: Traveler_Phase0_Spec_v0.1.md §8.2 (CRITICAL safety event emission)
#pragma once

#include "cockpit/snapshot.h"
#include "cockpit/state.h"

#include <cstddef>
#include <string>

namespace traveler::cockpit {

// ============================================================================
// BudgetCheckResult — result of a memory budget check
// ============================================================================
struct BudgetCheckResult {
    bool ok{true};
    std::string detail;
};

// ============================================================================
// Memory budget checks (REQ-COCKPIT-6)
//
// Per-callsign snapshot ≤ 256 KB
// Focused callsign full state ≤ 16 MB
// Violations emit CRITICAL safety event via traveler::safety::emit_safety_event
// ============================================================================

// Check snapshot size is within the 256 KB budget
[[nodiscard]] BudgetCheckResult check_snapshot_budget(
    const CockpitSnapshot& snapshot,
    const std::string& callsign);

// Check focused callsign full state size is within the 16 MB budget
[[nodiscard]] BudgetCheckResult check_full_state_budget(
    std::size_t state_bytes,
    const std::string& callsign);

// Check all budget constraints for the entire cockpit state.
// Iterates all mounted callsigns and the focused callsign's full state.
// Emits a CRITICAL safety event for each violation.
// Returns the first violation found, or {true, "passed"}.
[[nodiscard]] BudgetCheckResult check_cockpit_budget(
    const CockpitState& state,
    std::size_t focused_state_bytes);

}  // namespace traveler::cockpit
