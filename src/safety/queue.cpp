// Reference: ~/hatch-v3/packages/hatch-safety/src/collector/store.ts
#include "queue.h"

#include <cstdlib>

namespace traveler::safety {

SafetyQueue::~SafetyQueue() {
    drain();
}

void SafetyQueue::enqueue(SafetyEvent event) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_) return;
    pending_.push_back(std::move(event));
}

void SafetyQueue::drain() {
    std::vector<SafetyEvent> batch;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batch.swap(pending_);
    }
    for (const auto& event : batch) {
        emit_safety_event(event);
    }
}

void SafetyQueue::shutdown() {
    drain();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
}

size_t SafetyQueue::pending_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}

// Global instance
static SafetyQueue g_queue;

SafetyQueue& safety_queue() {
    return g_queue;
}

// Static destructor drain: std::atexit ensures drain on normal exit
namespace {
    struct ShutdownGuard {
        ~ShutdownGuard() {
            g_queue.drain();
        }
    };
    static ShutdownGuard g_shutdown_guard;
}  // anonymous namespace

void register_safety_queue_shutdown() {
    // Shutdown guard is already constructed as a static
}

}  // namespace traveler::safety
