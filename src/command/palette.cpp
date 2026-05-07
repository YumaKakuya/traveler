#include "command/palette.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

namespace traveler::command {

namespace {

// ---- fzf-like fuzzy scoring helpers ----

char to_lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

/// Returns true if `query` is a case-insensitive subsequence of `target`.
/// Also fills `positions` with the matched indices in `target`.
bool is_subsequence(std::string_view query, std::string_view target,
                    std::vector<std::size_t>& positions) {
    positions.clear();
    if (query.empty()) return true;
    if (target.empty()) return false;

    std::size_t ti = 0;
    for (std::size_t qi = 0; qi < query.size(); ++qi) {
        const char qc = to_lower(query[qi]);
        while (ti < target.size() && to_lower(target[ti]) != qc) {
            ++ti;
        }
        if (ti >= target.size()) return false;
        positions.push_back(ti);
        ++ti;
    }
    return true;
}

/// Bonus for consecutive matched characters.
int consecutive_bonus(const std::vector<std::size_t>& positions) {
    if (positions.size() < 2) return 0;
    int bonus = 0;
    for (std::size_t i = 1; i < positions.size(); ++i) {
        if (positions[i] == positions[i - 1] + 1) {
            bonus += 5;
        }
    }
    return bonus;
}

/// Returns true if `positions[0]` is at the start of `target` or follows
/// a non-alphanumeric character (word boundary).
bool starts_at_word_boundary(std::string_view target,
                             const std::vector<std::size_t>& positions) {
    if (positions.empty()) return false;
    const std::size_t p0 = positions[0];
    if (p0 == 0) return true;
    // Check if the character before p0 is a word separator
    const char prev = target[p0 - 1];
    return !std::isalnum(static_cast<unsigned char>(prev)) && prev != '_';
}

/// Check if the entire query matches as a case-insensitive prefix starting
/// at a word boundary.
bool is_prefix_at_word_boundary(std::string_view query, std::string_view target,
                                const std::vector<std::size_t>& positions) {
    if (query.size() != positions.size()) return false;
    if (!starts_at_word_boundary(target, positions)) return false;
    // Verify all consecutive from the start of that word
    for (std::size_t i = 1; i < positions.size(); ++i) {
        if (positions[i] != positions[i - 1] + 1) return false;
    }
    return true;
}

}  // namespace

CommandPalette::CommandPalette(const ActionRegistry& registry)
    : registry_(registry) {}

int CommandPalette::score_match(std::string_view query,
                                std::string_view target) {
    if (query.empty()) return 1;
    if (target.empty()) return 0;

    std::vector<std::size_t> positions;
    if (!is_subsequence(query, target, positions)) {
        return 0;
    }

    // Base score for any subsequence match
    int score = 20;

    // Consecutive bonus
    score += consecutive_bonus(positions);

    // Word-boundary start bonus
    if (starts_at_word_boundary(target, positions)) {
        score += 30;
    }

    // Exact prefix at word boundary — highest bonus (spec: "exact prefix wins")
    if (is_prefix_at_word_boundary(query, target, positions)) {
        score += 50;
    }

    // Bonus for matching early in the string
    if (!positions.empty()) {
        score += std::max(0, 10 - static_cast<int>(positions[0]));
    }

    return score;
}

std::vector<ScoredAction> CommandPalette::query(std::string_view input) const {
    std::vector<ScoredAction> results;

    for (const auto& action : registry_.actions()) {
        int s = score_match(input, action.display_name);
        if (s > 0) {
            results.push_back({action, s});
        }
    }

    // Sort descending by score; stable sort to preserve registry order on ties
    std::stable_sort(results.begin(), results.end(),
                     [](const ScoredAction& a, const ScoredAction& b) {
                         return a.score > b.score;
                     });

    return results;
}

}  // namespace traveler::command
