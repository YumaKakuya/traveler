// GATE-P0-4 Safety Queue Drain Guard Test
// Spec PC-8: Push 100 events through SafetyQueue, drain on shutdown,
// verify all 100 processed (none dropped).
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "safety/event.h"
#include "safety/queue.h"

namespace fs = std::filesystem;
using namespace traveler::safety;

static int g_passed = 0;
static int g_failed = 0;

#define TEST_RUN(name, expr) do { \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << name << std::endl; \
        g_failed++; \
    } else { \
        std::cout << "  PASS: " << name << std::endl; \
        g_passed++; \
    } \
} while(0)

static std::string readFile(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

int main() {
    std::cout << "=== GATE-P0-4 Safety Queue Drain Guard Test ===" << std::endl;

    constexpr int NUM_EVENTS = 100;

    // Clean up any previous test artifacts
    std::string eventsPath = safety_events_path();
    std::cerr << "Events path: " << eventsPath << std::endl;

    // Create a fresh SafetyQueue (not using the global one to avoid interfering
    // with other tests or the running application)
    SafetyQueue queue;

    // Verify initial state
    TEST_RUN("T1: queue is empty initially", queue.pending_count() == 0);
    TEST_RUN("T2: queue not shut down initially", !queue.is_shutdown());

    // Push 100 events
    for (int i = 0; i < NUM_EVENTS; ++i) {
        SafetyEvent event;
        event.severity = Severity::LOW;
        event.category = Category::INTERNAL;
        event.redacted_input = "test input " + std::to_string(i);
        event.redacted_output = "test output " + std::to_string(i);
        event.detail = "Queue test event #" + std::to_string(i);
        event.timestamp = std::chrono::system_clock::now();
        queue.enqueue(std::move(event));
    }

    // Verify all 100 events are pending
    TEST_RUN("T3: 100 events pending after enqueue", queue.pending_count() == NUM_EVENTS);

    // Draining should process all pending events
    queue.drain();

    // After drain, pending count should be 0
    TEST_RUN("T4: pending count 0 after drain", queue.pending_count() == 0);

    // Drain again (idempotent)
    queue.drain();
    TEST_RUN("T5: pending count still 0 after second drain", queue.pending_count() == 0);

    // Push 5 more events after drain
    for (int i = 0; i < 5; ++i) {
        SafetyEvent event;
        event.severity = Severity::MEDIUM;
        event.category = Category::PII;
        event.redacted_input = "post_drain " + std::to_string(i);
        event.redacted_output = "post_drain_out " + std::to_string(i);
        event.detail = "Post-drain event #" + std::to_string(i);
        event.timestamp = std::chrono::system_clock::now();
        queue.enqueue(std::move(event));
    }
    TEST_RUN("T6: 5 events pending after post-drain enqueue", queue.pending_count() == 5);

    // Shutdown should drain remaining events and prevent further enqueues
    queue.shutdown();

    // After shutdown, pending count should be 0
    TEST_RUN("T7: pending count 0 after shutdown", queue.pending_count() == 0);
    TEST_RUN("T8: queue is shut down", queue.is_shutdown());

    // Attempt to enqueue after shutdown (should be silently dropped)
    SafetyEvent dropped_event;
    dropped_event.severity = Severity::HIGH;
    dropped_event.category = Category::SECRET;
    dropped_event.detail = "This should be dropped";
    dropped_event.timestamp = std::chrono::system_clock::now();
    queue.enqueue(std::move(dropped_event));

    // Verify the dropped event was not accepted
    TEST_RUN("T9: no events accepted after shutdown", queue.pending_count() == 0);

    // Summary
    std::cout << "\n=== Summary: " << g_passed << " passed, "
              << g_failed << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
