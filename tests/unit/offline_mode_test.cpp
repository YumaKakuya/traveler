// WAVE-C C1: Offline mode state/control tests.
// Reference: briefs/wave-c-c1-offline-ayane-dispatch-2026-05-07.md
//
// Tests offline single-callsign cap, cloud-to-offline retain-one behavior,
// switch back to cloud, and mode transition error handling.
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include "cockpit/mount.h"
#include "cockpit/offline_mode.h"
#include "cockpit/snapshot.h"
#include "cockpit/state.h"

using namespace traveler::cockpit;

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << (name) << std::endl; \
        g_failed++; \
    } else { \
        std::cout << "  PASS: " << (name) << std::endl; \
        g_passed++; \
    } \
} while(0)

// ============================================================================
// Test: OfflineModeTracker initial state
// ============================================================================
static void test_tracker_initial_state() {
    std::cout << "\n--- OfflineModeTracker initial state ---" << std::endl;

    OfflineModeTracker tracker;
    TEST("default mode is Cloud", tracker.mode() == OfflineMode::Cloud);
    TEST("default is_cloud() true", tracker.is_cloud());
    TEST("default is_offline() false", !tracker.is_offline());
    TEST("default max_callsigns = 4", tracker.max_callsigns() == 4);

    OfflineModeTracker cloud_tracker(OfflineMode::Cloud);
    TEST("explicit cloud mode", cloud_tracker.mode() == OfflineMode::Cloud);
    TEST("cloud max_callsigns = 4", cloud_tracker.max_callsigns() == 4);

    OfflineModeTracker offline_tracker(OfflineMode::Offline);
    TEST("explicit offline mode", offline_tracker.mode() == OfflineMode::Offline);
    TEST("offline max_callsigns = 1", offline_tracker.max_callsigns() == 1);
    TEST("offline is_offline() true", offline_tracker.is_offline());
    TEST("offline is_cloud() false", !offline_tracker.is_cloud());

    tracker.set_mode(OfflineMode::Offline);
    TEST("set_mode to offline works", tracker.mode() == OfflineMode::Offline);
    TEST("max_callsigns = 1 after set_mode", tracker.max_callsigns() == 1);
    tracker.set_mode(OfflineMode::Cloud);
    TEST("set_mode back to cloud", tracker.mode() == OfflineMode::Cloud);
    TEST("max_callsigns = 4 after set_mode", tracker.max_callsigns() == 4);
}

// ============================================================================
// Test: OfflineModeError factory methods
// ============================================================================
static void test_offline_error_factories() {
    std::cout << "\n--- OfflineModeError factory methods ---" << std::endl;

    auto cap = OfflineModeError::CapExceeded("cap exceeded");
    TEST("CapExceeded message", cap.message == "cap exceeded");

    auto conflict = OfflineModeError::ModeSwitchConflict("conflict");
    TEST("ModeSwitchConflict message", conflict.message == "conflict");
}

// ============================================================================
// Test: to_string for OfflineMode
// ============================================================================
static void test_to_string_mode() {
    std::cout << "\n--- to_string for OfflineMode ---" << std::endl;

    TEST("to_string Cloud",
         std::string(to_string(OfflineMode::Cloud)) == "cloud");
    TEST("to_string Offline",
         std::string(to_string(OfflineMode::Offline)) == "offline");
}

// ============================================================================
// Test: check_mount_permitted
// ============================================================================
static void test_check_mount_permitted() {
    std::cout << "\n--- check_mount_permitted ---" << std::endl;

    {
        CockpitState state;
        state.cloud_mode = true;
        OfflineModeTracker tracker(OfflineMode::Cloud);

        auto result = check_mount_permitted(state, tracker, "@vega");
        TEST("cloud mode with 0 mounts: permitted", result.has_value());
    }

    {
        CockpitState state;
        state.cloud_mode = true;
        OfflineModeTracker tracker(OfflineMode::Cloud);
        mount(state, "@vega");
        mount(state, "@altair");
        mount(state, "@orion");
        mount(state, "@rigel");

        auto result = check_mount_permitted(state, tracker, "@extra");
        TEST("cloud mode with 4 mounts: cap exceeded", !result.has_value());
    }

    {
        CockpitState state;
        state.cloud_mode = false;
        OfflineModeTracker tracker(OfflineMode::Offline);

        auto result = check_mount_permitted(state, tracker, "@vega");
        TEST("offline mode with 0 mounts: permitted", result.has_value());
    }

    {
        CockpitState state;
        state.cloud_mode = false;
        OfflineModeTracker tracker(OfflineMode::Offline);
        mount(state, "@vega");

        auto result = check_mount_permitted(state, tracker, "@altair");
        TEST("offline mode with 1 mount already: cap exceeded", !result.has_value());
    }
}

// ============================================================================
// Test: offline_cap_exceeded_message
// ============================================================================
static void test_cap_exceeded_message() {
    std::cout << "\n--- offline_cap_exceeded_message ---" << std::endl;

    std::string msg = offline_cap_exceeded_message();
    TEST("message contains 'single callsign'",
         msg.find("single callsign") != std::string::npos);
    TEST("message contains 'hardware budget'",
         msg.find("hardware budget") != std::string::npos);
}

// ============================================================================
// Test: Single-callsign cap in offline mode
// ============================================================================
static void test_single_callsign_cap() {
    std::cout << "\n--- Single-callsign cap ---" << std::endl;

    CockpitState state;
    state.cloud_mode = false;
    OfflineModeTracker tracker(OfflineMode::Offline);

    auto r1 = mount(state, "@vega");
    TEST("first mount succeeds", r1.has_value());
    TEST("mount_count = 1", mount_count(state) == 1);

    auto r2 = mount(state, "@altair");
    TEST("second mount fails", !r2.has_value());
    TEST("mount_count still 1", mount_count(state) == 1);

    if (!r2.has_value()) {
        TEST("error message contains 'Offline mode supports'",
             r2.error().message.find("Offline mode supports a single callsign") !=
                 std::string::npos);
    }

    TEST("@vega is mounted", is_mounted(state, "@vega"));
    TEST("@altair is not mounted", !is_mounted(state, "@altair"));
    TEST("max_callsigns = 1", max_callsigns(state) == 1);
}

// ============================================================================
// Test: Cloud-to-offline retain-one behavior
// ============================================================================
static void test_cloud_to_offline_retain() {
    std::cout << "\n--- Cloud-to-offline retain-one ---" << std::endl;

    CockpitState state;
    state.cloud_mode = true;
    OfflineModeTracker tracker(OfflineMode::Cloud);

    mount(state, "@vega");
    mount(state, "@altair");
    mount(state, "@orion");
    TEST("setup: 3 cloud mounts", mount_count(state) == 3);

    auto switch_result = switch_to_offline(state, tracker, "@altair");
    TEST("switch_to_offline succeeds", switch_result.has_value());

    if (switch_result.has_value()) {
        TEST("tracker is now offline", tracker.is_offline());
        TEST("state.cloud_mode is false", !state.cloud_mode);

        TEST("mount_count = 1 after retain", mount_count(state) == 1);
        TEST("@altair is mounted", is_mounted(state, "@altair"));
        TEST("@vega is unmounted", !is_mounted(state, "@vega"));
        TEST("@orion is unmounted", !is_mounted(state, "@orion"));

        TEST("@altair is focused",
             state.focused.has_value() && *state.focused == "@altair");
        TEST("verify_offline_single_mount passes",
             verify_offline_single_mount(state, "@altair"));
    }
}

// ============================================================================
// Test: Switch back to cloud
// ============================================================================
static void test_switch_to_cloud() {
    std::cout << "\n--- Switch back to cloud ---" << std::endl;

    CockpitState state;
    state.cloud_mode = false;
    OfflineModeTracker tracker(OfflineMode::Offline);
    mount(state, "@vega");

    auto result = switch_to_cloud(state, tracker);
    TEST("switch_to_cloud succeeds", result.has_value());

    if (result.has_value()) {
        TEST("tracker is_cloud() true", tracker.is_cloud());
        TEST("state.cloud_mode is true", state.cloud_mode);
        TEST("max_callsigns back to 4", tracker.max_callsigns() == 4);

        TEST("@vega still mounted", is_mounted(state, "@vega"));
        TEST("mount_count = 1", mount_count(state) == 1);

        auto r2 = mount(state, "@altair");
        TEST("can mount 2nd after switching to cloud", r2.has_value());
        TEST("mount_count = 2", mount_count(state) == 2);
    }
}

// ============================================================================
// Test: Mode transition conflicts
// ============================================================================
static void test_mode_transition_conflicts() {
    std::cout << "\n--- Mode transition conflicts ---" << std::endl;

    {
        CockpitState state;
        state.cloud_mode = false;
        OfflineModeTracker tracker(OfflineMode::Offline);

        auto result = switch_to_offline(state, tracker, "@vega");
        TEST("offline->offline returns error", !result.has_value());
    }

    {
        CockpitState state;
        state.cloud_mode = true;
        OfflineModeTracker tracker(OfflineMode::Cloud);

        auto result = switch_to_cloud(state, tracker);
        TEST("cloud->cloud returns error", !result.has_value());
    }

    {
        CockpitState state;
        state.cloud_mode = true;
        OfflineModeTracker tracker(OfflineMode::Cloud);
        mount(state, "@vega");

        auto result = switch_to_offline(state, tracker, "@rigel");
        TEST("retain non-mounted returns error", !result.has_value());
        if (!result.has_value()) {
            TEST("error message mentions 'not currently mounted'",
                 result.error().message.find("not currently mounted") !=
                     std::string::npos);
        }
    }
}

// ============================================================================
// Test: sync_cockpit_state
// ============================================================================
static void test_sync_cockpit_state() {
    std::cout << "\n--- sync_cockpit_state ---" << std::endl;

    CockpitState state;
    state.cloud_mode = true;

    OfflineModeTracker tracker(OfflineMode::Offline);

    sync_cockpit_state(state, tracker);
    TEST("sync with offline tracker -> cloud_mode false", !state.cloud_mode);

    tracker.set_mode(OfflineMode::Cloud);
    sync_cockpit_state(state, tracker);
    TEST("sync with cloud tracker -> cloud_mode true", state.cloud_mode);
}

// ============================================================================
// Test: verify_offline_single_mount edge cases
// ============================================================================
static void test_verify_offline_single_mount() {
    std::cout << "\n--- verify_offline_single_mount edge cases ---" << std::endl;

    {
        CockpitState state;
        state.cloud_mode = false;
        TEST("empty verify returns false",
             !verify_offline_single_mount(state, "@vega"));
    }

    {
        CockpitState state;
        state.cloud_mode = true;
        mount(state, "@vega");
        bool v = verify_offline_single_mount(state, "@vega");
        TEST("1 mount matching returns true", v);
    }

    {
        CockpitState state;
        state.cloud_mode = true;
        mount(state, "@vega");
        bool v = verify_offline_single_mount(state, "@altair");
        TEST("1 mount not matching retained returns false", !v);
    }

    {
        CockpitState state2;
        state2.cloud_mode = true;
        mount(state2, "@vega");
        mount(state2, "@altair");
        OfflineModeTracker tracker(OfflineMode::Cloud);
        auto result = switch_to_offline(state2, tracker, "@vega");
        TEST("switch with 2 mounted succeeded", result.has_value());
        if (result.has_value()) {
            TEST("verify with @vega",
                 verify_offline_single_mount(state2, "@vega"));
            TEST("verify with @altair is false",
                 !verify_offline_single_mount(state2, "@altair"));
        }
    }
}

// ============================================================================
// main
// ============================================================================
int main() {
    std::cout << "=== WAVE-C C1: Offline Mode Tests ===" << std::endl;

    test_tracker_initial_state();
    test_offline_error_factories();
    test_to_string_mode();
    test_check_mount_permitted();
    test_cap_exceeded_message();
    test_single_callsign_cap();
    test_cloud_to_offline_retain();
    test_switch_to_cloud();
    test_mode_transition_conflicts();
    test_sync_cockpit_state();
    test_verify_offline_single_mount();

    std::cout << "\n=== Summary: " << g_passed << " passed, " << g_failed
              << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
