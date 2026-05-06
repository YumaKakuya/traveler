// Reference: Traveler_Phase0_Spec_v0.1.md §8.2 (REQ-SAFETY-7)
#include "redact.h"
#include "canonicalize.h"
#include "event.h"
#include "queue.h"

namespace traveler::safety {

std::string redact_for_log(std::string_view raw) {
    // Use the canonical pipeline for redaction
    std::string result = canonicalize(raw);

    // Emit safety event when redaction occurs (if content was modified)
    if (result != raw) {
        SafetyEvent event;
        event.severity = Severity::LOW;
        event.category = Category::INTERNAL;
        event.redacted_input = "[REDACTED]";
        event.redacted_output = "[REDACTED]";
        event.detail = "Log redaction: content was modified by canonical pipeline";
        event.timestamp = std::chrono::system_clock::now();

        // Enqueue (non-blocking; drained on shutdown)
        safety_queue().enqueue(std::move(event));
    }

    return result;
}

}  // namespace traveler::safety
