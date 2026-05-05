// Reference: ~/hatch-v3/packages/hatch-safety/src/collector/store.ts (122 lines)
// Reference: Traveler_Phase0_Spec_v0.1.md §8.2 (REQ-SAFETY-6: drain guard)
#pragma once

#include "event.h"
#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace traveler::safety {

// ============================================================================
// SafetyQueue — pending queue with explicit drain on shutdown
//
// Per Spec REQ-SAFETY-6 / N-4: NO fire-and-forget. Every safety operation
// has explicit lifecycle: queue + drain + shutdown.
// Phase 0: single-threaded queue (background threads not required until P0-5).
// ============================================================================

class SafetyQueue {
public:
    SafetyQueue() = default;
    ~SafetyQueue();

    // Non-copyable, non-movable
    SafetyQueue(const SafetyQueue&) = delete;
    SafetyQueue& operator=(const SafetyQueue&) = delete;
    SafetyQueue(SafetyQueue&&) = delete;
    SafetyQueue& operator=(SafetyQueue&&) = delete;

    // Enqueue a safety event for processing
    void enqueue(SafetyEvent event);

    // Process (emit) all pending events. Call during idle or before shutdown.
    void drain();

    // Drain and shut down. MUST be called before process exit.
    // After shutdown(), no further events are accepted.
    void shutdown();

    // Number of pending events
    [[nodiscard]] size_t pending_count() const;

    // Whether the queue has been shut down
    [[nodiscard]] bool is_shutdown() const { return shutdown_; }

private:
    mutable std::mutex mutex_;
    std::vector<SafetyEvent> pending_;
    bool shutdown_{false};
};

// ============================================================================
// Global safety queue instance (Phase 0: single instance)
// ============================================================================
SafetyQueue& safety_queue();

// Register a shutdown handler to drain the queue on program exit
void register_safety_queue_shutdown();

}  // namespace traveler::safety
