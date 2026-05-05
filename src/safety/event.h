// Reference: ~/hatch-v3/packages/hatch-safety/src/collector/types.ts (19 lines)
// Reference: Traveler_Phase0_Spec_v0.1.md §8.2 (REQ-SAFETY-5: severity/category emission)
#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace traveler::safety {

// ============================================================================
// SafetyEvent — port of Hatch safety collector event type
// Spec REQ-SAFETY-5: severity + category for downstream filtering.
// ============================================================================

enum class Severity : uint8_t {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2,
    CRITICAL = 3,
};

const char* to_string(Severity s);

enum class Category : uint8_t {
    PII = 0,
    SECRET = 1,
    PATH = 2,
    DANGER = 3,
    BUDGET = 4,
    INTERNAL = 5,
};

const char* to_string(Category c);

struct SafetyEvent {
    Severity severity;
    Category category;
    std::string redacted_input;
    std::string redacted_output;
    std::string detail;                    // human-readable detail
    std::chrono::system_clock::time_point timestamp;
};

// ============================================================================
// JSONL emission — writes one JSON record per line to safety_events.jsonl
// ============================================================================

// Emit a safety event to the JSONL file. Thread-safe only in single-threaded
// Phase 0 context (background threads not required until P0-5).
void emit_safety_event(const SafetyEvent& event);

// Path to the safety events JSONL file
const char* safety_events_path();

}  // namespace traveler::safety
