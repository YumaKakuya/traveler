#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "command/registry.h"

namespace traveler::command {

/// A ranked action result from fuzzy search.
struct ScoredAction {
    Action action;
    int score{0};  // higher = better match; 0 = no match (excluded from results)
};

/// Fuzzy-search engine for the Command Palette (REQ-CMDPAL-3).
///
/// Phase 0 scoring: case-insensitive subsequence match with bonuses for
/// consecutive characters, word-boundary starts, and exact prefix matches.
/// Exact prefix wins over scattered subsequence.
///
/// Usage:
///   CommandPalette palette(registry);
///   auto results = palette.query("edi");
///   // results[0].action.display_name == "Open Editor mode"
class CommandPalette {
public:
    explicit CommandPalette(const ActionRegistry& registry);

    /// Run a fuzzy search against the registry.
    ///
    /// Results are ranked descending by score. Actions with score == 0 are
    /// excluded. An empty query returns all actions, each with score == 1
    /// (preserving registry order).
    std::vector<ScoredAction> query(std::string_view input) const;

    /// Score a single display_name against a query string.
    /// Public for testing.
    static int score_match(std::string_view query,
                           std::string_view target);

private:
    const ActionRegistry& registry_;
};

}  // namespace traveler::command
