// Reference: Traveler_Phase0_Spec_v0.1.md §9.2 (Background Streaming for Non-Focused Callsigns)
// Reference: Traveler_Phase0_Spec_v0.1.md REQ-COCKPIT-BG-1, REQ-COCKPIT-BG-2, REQ-COCKPIT-BG-3
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace traveler::cockpit {

// ============================================================================
// BgStreamStatus — status of a background streaming operation (§9.2)
// ============================================================================
enum class BgStreamStatus {
    Ready,        // Not streaming
    Running,      // Streaming in progress
    Done,         // Completed successfully
    Error,        // Completed with error
    Interrupted,  // Cancelled on shutdown
};

[[nodiscard]] const char* to_string(BgStreamStatus s);

// ============================================================================
// BackgroundStreamer — manages background generation for non-focused callsigns
//
// REQ-COCKPIT-BG-1: Non-focused callsign generation continues in background.
// REQ-COCKPIT-BG-3: On shutdown, cancel all in-flight, persist [interrupted].
//
// Phase 0: This is a state tracker. Actual threading/streaming is delegated
// to the provider layer. The BackgroundStreamer only tracks which callsigns
// have in-flight streams and their completion status.
// ============================================================================
class BackgroundStreamer {
public:
    BackgroundStreamer() = default;

    // Start a background stream for a callsign
    void start(std::string_view callsign);

    // Mark a callsign's background stream as completed successfully
    void mark_done(std::string_view callsign);

    // Mark a callsign's background stream as completed with error
    void mark_error(std::string_view callsign);

    // Get the current background stream status for a callsign
    [[nodiscard]] BgStreamStatus status(std::string_view callsign) const;

    // Check if a callsign has an active (running) background stream
    [[nodiscard]] bool is_running(std::string_view callsign) const;

    // REQ-COCKPIT-BG-3: Cancel all in-flight streams and mark [interrupted].
    // For each currently running stream, calls the provided callback so the
    // caller can persist partial transcripts with the [interrupted] marker.
    void shutdown(const std::function<void(const std::string& callsign)>& on_interrupt);

    // Check if any callsign has a running background stream
    [[nodiscard]] bool has_running() const;

private:
    std::unordered_map<std::string, BgStreamStatus> streams_;
};

}  // namespace traveler::cockpit
