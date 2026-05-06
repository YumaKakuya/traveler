// GATE-P0-4 T2: Roles dispatcher integration with Cockpit @-mention
// Reference: Traveler_Phase0_Spec_v0.1.md §8.1 (REQ-ROLES-3)
// Reference: ~/hatch-v3/packages/opencode/src/agent/roles.ts (lookup + dispatch pattern)
//
// Header-only for Phase 0 testability. When the Cockpit dispatcher and
// provider request construction are completed (GATE-P0-5), the non-trivial
// dispatch logic moves to dispatch.cpp.
#pragma once

#include "roles/types.h"
#include "roles/registry.h"
#include <string>
#include <string_view>
#include <tl/expected.hpp>

namespace traveler::roles {

// ============================================================================
// DispatchTarget — resolved @-mention result suitable for provider inspection.
//
// Contains all data needed to construct a provider request without making
// real network calls. Satisfies PC-2: "verified by inspecting the outgoing
// provider request." No actual provider invocation happens here.
// ============================================================================
struct DispatchTarget {
    std::string callsign;       // "@vega"
    std::string model;          // "anthropic/claude-opus-4-7"
    std::string provider;       // "anthropic" (before first '/')
    std::string model_name;     // "claude-opus-4-7" (after first '/')
    std::string system_prompt;  // role's H2 body content
    std::string tier;           // "opus" | "sonnet" | "gpt" | etc.
};

// ============================================================================
// DispatchError — error returned on unknown callsign or empty registry.
// ============================================================================
struct DispatchError {
    std::string message;

    [[nodiscard]] static DispatchError UnknownCallsign(std::string cs) {
        return {std::string("unknown callsign: ") + std::move(cs)};
    }
    [[nodiscard]] static DispatchError EmptyRegistry() {
        return {"No roles loaded. Load a roles.md file first."};
    }
};

// ============================================================================
// resolve_dispatch — map a @-mention callsign to the configured model.
//
// Accepts callsigns with or without leading '@'.
// Returns DispatchTarget on success; DispatchError on unknown callsign.
// No network calls — pure RoleRegistry lookup + model string split.
//
// This is the dispatch path required by PC-2:
//   "Cockpit @-mention @altair Hello dispatches to the model assigned to
//    @altair per roles.md (verified by inspecting the outgoing provider request)"
// ============================================================================
inline tl::expected<DispatchTarget, DispatchError>
resolve_dispatch(const RoleRegistry& registry, std::string_view callsign) {
    if (registry.empty()) {
        return tl::make_unexpected(DispatchError::EmptyRegistry());
    }

    auto entry = registry.lookup(callsign);
    if (!entry.has_value()) {
        return tl::make_unexpected(DispatchError::UnknownCallsign(std::string(callsign)));
    }

    DispatchTarget target;
    target.callsign = entry->callsign;
    target.model = entry->model;
    target.system_prompt = entry->system_prompt;
    target.tier = entry->tier;

    // Split model into provider and model_name for request construction
    auto slash = entry->model.find('/');
    if (slash != std::string::npos) {
        target.provider = entry->model.substr(0, slash);
        target.model_name = entry->model.substr(slash + 1);
    }

    return target;
}

}  // namespace traveler::roles
