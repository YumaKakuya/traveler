// WAVE-B Lane B4: LLM Mode Mock Round-Trip — PC-10
// Reference: Traveler_Phase0_Spec_v0.1.md §6 Pass Criteria PC-10
// Reference: briefs/reconcile-p0-2-2026-05-06.md §2 PC-10, §5 Dispatch #4
//
// Tests:
//   PC10-a  MockProvider::respond("Hello.") returns canonical greeting
//   PC10-b  MockProvider::respond(empty) returns fallback
//   PC10-c  MockProvider::respond(other) returns echo
//   PC10-d  MockProvider implements Provider interface (id, streaming)
//   PC10-e  MockProvider::generate() produces chunks and fires on_chunk
//   PC10-f  LlmMode::enter() creates active session
//   PC10-g  LlmMode::send_prompt("Hello.") round-trips canonical response
//   PC10-h  LlmMode::stage_text() contains both [You] and [Assistant]
//   PC10-i  Multiple-turn conversation accumulates messages
//   PC10-j  render_conversation formats correctly
//
// No network I/O, no secrets, no real provider adapters.
//
// Compile requirements (B6 xmake registration — NOT in xmake.lua, see brief):
//   target("test_llm_mode")
//       set_kind("binary")
//       add_includedirs("src")
//       add_files("tests/unit/llm_mode_test.cpp")
//       add_files("src/llm/mode.cpp")
//       add_files("src/llm/session.cpp")
//       add_packages("tl_expected")
//       set_group("test")
//       add_tests("default")
//
// ============================================================================

#include <cassert>
#include <iostream>
#include <string>
#include <string_view>

#include "llm/mode.h"
#include "llm/provider.h"
#include "llm/session.h"

using namespace traveler::llm;

// ============================================================================
// Minimal test harness (same pattern as tests/unit/cockpit_test.cpp)
// ============================================================================
static int g_passed = 0;
static int g_failed = 0;

#define TEST(name, expr)                                              \
    do {                                                               \
        if (!(expr)) {                                                 \
            std::cerr << "  FAIL: " << (name) << std::endl;            \
            g_failed++;                                                \
        } else {                                                       \
            std::cout << "  PASS: " << (name) << std::endl;            \
            g_passed++;                                                \
        }                                                              \
    } while (0)

// ============================================================================
// PC10-a through PC10-e: MockProvider unit
// ============================================================================
static void test_mock_provider_respond() {
    // PC10-a: Canonical "Hello." → known response
    {
        auto resp = MockProvider::respond("Hello.");
        TEST("PC10-a: respond('Hello.') == canonical",
             resp == MockProvider::kCanonicalResponse);
    }

    // Variants of Hello
    {
        TEST("PC10-a2: respond('Hello') == canonical",
             MockProvider::respond("Hello") == MockProvider::kCanonicalResponse);
        TEST("PC10-a3: respond('hello.') == canonical",
             MockProvider::respond("hello.") == MockProvider::kCanonicalResponse);
        TEST("PC10-a4: respond('hello') == canonical",
             MockProvider::respond("hello") == MockProvider::kCanonicalResponse);
    }

    // Whitespace-trimmed "Hello."
    {
        TEST("PC10-a5: respond('  Hello.  ') == canonical",
             MockProvider::respond("  Hello.  ") == MockProvider::kCanonicalResponse);
    }

    // PC10-b: Empty / whitespace-only → fallback
    {
        auto resp = MockProvider::respond("");
        TEST("PC10-b1: respond('') is fallback",
             !resp.empty() && resp != MockProvider::kCanonicalResponse);
    }
    {
        auto resp = MockProvider::respond("   ");
        TEST("PC10-b2: respond('   ') is fallback",
             !resp.empty() && resp != MockProvider::kCanonicalResponse);
    }

    // PC10-c: Other text → echo
    {
        auto resp = MockProvider::respond("What is the weather?");
        TEST("PC10-c1: respond(other) starts with 'Mock echo'",
             resp.rfind("Mock echo:", 0) == 0);
        TEST("PC10-c2: respond(other) contains input",
             resp.find("What is the weather?") != std::string::npos);
    }
}

// ============================================================================
// PC10-d: Provider interface compliance
// ============================================================================
static void test_mock_provider_interface() {
    MockProvider mp;

    TEST("PC10-d1: id() == 'mock'", mp.id() == "mock");
    TEST("PC10-d2: supports_streaming() == true", mp.supports_streaming());
}

// ============================================================================
// PC10-e: generate() produces chunks + on_chunk callback
// ============================================================================
static void test_mock_provider_generate() {
    MockProvider mp;

    GenerateOptions opts;
    opts.model = "mock";
    opts.messages.push_back({"user", "Hello."});

    int chunk_count = 0;
    std::string total_delta;
    bool final_done = false;

    auto result = mp.generate(opts, [&](GenerateChunk chunk) {
        chunk_count++;
        total_delta += chunk.delta;
        if (chunk_count == 1) {
            // First chunk should not be done (streaming simulation)
            // unless response is very short
        }
        final_done = chunk.done;
    });

    TEST("PC10-e1: generate() returns success", result.has_value());
    TEST("PC10-e2: at least 1 chunk produced", chunk_count >= 1);
    TEST("PC10-e3: total delta is canonical response",
         total_delta == MockProvider::kCanonicalResponse);
    TEST("PC10-e4: final chunk.done == true", final_done);

    // Verify no error_message was set
    {
        bool had_error = false;
        auto err_result = mp.generate(opts, [&](GenerateChunk chunk) {
            if (chunk.error_message.has_value()) had_error = true;
        });
        TEST("PC10-e5: no error_message in chunks", !had_error);
        (void)err_result;
    }

    // Multiple messages — last user message wins
    {
        GenerateOptions multi_opts;
        multi_opts.model = "mock";
        multi_opts.messages.push_back({"system", "You are helpful."});
        multi_opts.messages.push_back({"user", "Hi"});
        multi_opts.messages.push_back({"assistant", "Hello!"});
        multi_opts.messages.push_back({"user", "Hello."});

        std::string last_response;
        auto multi_result = mp.generate(multi_opts, [&](GenerateChunk chunk) {
            if (chunk.done) last_response = total_delta;
            (void)chunk;
        });
        // The mock should respond based on the last user message
        // We verify this indirectly via LlmMode test below
        TEST("PC10-e6: multi-message generate succeeds", multi_result.has_value());
        (void)last_response;
    }
}

// ============================================================================
// PC10-f: LlmMode::enter() creates active session
// ============================================================================
static void test_llm_mode_enter() {
    LlmMode mode;

    // Before enter: no session
    TEST("PC10-f1: !has_active_session() before enter",
         !mode.has_active_session());
    TEST("PC10-f2: session() == nullptr before enter",
         mode.session() == nullptr);

    // After enter: session exists
    mode.enter();
    TEST("PC10-f3: has_active_session() after enter",
         mode.has_active_session());
    TEST("PC10-f4: session() != nullptr after enter",
         mode.session() != nullptr);
    TEST("PC10-f5: session id is non-empty",
         !mode.session()->id().empty());
    TEST("PC10-f6: session callsign is '@vega'",
         mode.session()->meta().callsign == "@vega");
    TEST("PC10-f7: session model is 'mock'",
         mode.session()->meta().model == "mock");
    TEST("PC10-f8: session state is idle",
         mode.session()->state() == SessionState::idle);

    // Second enter() replaces session
    mode.enter();
    TEST("PC10-f9: still has active session after re-enter",
         mode.has_active_session());
    TEST("PC10-f10: message_count == 0 after fresh enter",
         mode.session()->message_count() == 0);
}

// ============================================================================
// PC10-g: send_prompt("Hello.") round-trip
// ============================================================================
static void test_llm_mode_send_prompt_hello() {
    LlmMode mode;
    mode.enter();

    std::string response = mode.send_prompt("Hello.");

    TEST("PC10-g1: response == canonical greeting",
         response == MockProvider::kCanonicalResponse);

    // Verify session state
    TEST("PC10-g2: session has 2 messages (user + assistant)",
         mode.session()->message_count() == 2);
    TEST("PC10-g3: first message is user role",
         mode.session()->messages()[0].role == "user");
    TEST("PC10-g4: first message content is 'Hello.'",
         mode.session()->messages()[0].content == "Hello.");
    TEST("PC10-g5: second message is assistant role",
         mode.session()->messages()[1].role == "assistant");
    TEST("PC10-g6: second message content is canonical",
         mode.session()->messages()[1].content == MockProvider::kCanonicalResponse);
    TEST("PC10-g7: session state is idle after generation",
         mode.session()->state() == SessionState::idle);
}

// ============================================================================
// PC10-h: stage_text() contains both roles
// ============================================================================
static void test_llm_mode_stage_text() {
    LlmMode mode;
    mode.enter();
    mode.send_prompt("Hello.");

    std::string text = mode.stage_text();

    TEST("PC10-h1: stage_text contains [You]",
         text.find("[You]") != std::string::npos);
    TEST("PC10-h2: stage_text contains [Assistant]",
         text.find("[Assistant]") != std::string::npos);
    TEST("PC10-h3: stage_text contains prompt text",
         text.find("Hello.") != std::string::npos);
    TEST("PC10-h4: stage_text contains canonical response",
         text.find(MockProvider::kCanonicalResponse) != std::string::npos);
    TEST("PC10-h5: [You] appears before [Assistant]",
         text.find("[You]") < text.find("[Assistant]"));

    // No session → fallback text
    LlmMode empty_mode;
    TEST("PC10-h6: stage_text without session is fallback",
         empty_mode.stage_text().find("no active session") != std::string::npos);
}

// ============================================================================
// PC10-i: Multiple-turn conversation accumulates messages
// ============================================================================
static void test_llm_mode_multiple_turns() {
    LlmMode mode;
    mode.enter();

    // Turn 1
    mode.send_prompt("Hello.");
    TEST("PC10-i1: after turn 1, message_count == 2",
         mode.session()->message_count() == 2);

    // Turn 2
    std::string r2 = mode.send_prompt("How are you?");
    TEST("PC10-i2: turn 2 response is echo (not canonical)",
         r2 != MockProvider::kCanonicalResponse);
    TEST("PC10-i3: turn 2 response contains 'How are you?'",
         r2.find("How are you?") != std::string::npos);
    TEST("PC10-i4: after turn 2, message_count == 4",
         mode.session()->message_count() == 4);

    // Turn 3
    mode.send_prompt("Goodbye.");
    TEST("PC10-i5: after turn 3, message_count == 6",
         mode.session()->message_count() == 6);

    // Verify message roles alternate: user, assistant, user, assistant, ...
    const auto& msgs = mode.session()->messages();
    for (size_t i = 0; i < msgs.size(); i += 2) {
        TEST(("PC10-i6a: msg[" + std::to_string(i) + "] is user").c_str(),
             msgs[i].role == "user");
        if (i + 1 < msgs.size()) {
            TEST(("PC10-i6b: msg[" + std::to_string(i + 1) + "] is assistant").c_str(),
                 msgs[i + 1].role == "assistant");
        }
    }

    // Verify stage_text shows all turns
    auto text = mode.stage_text();
    TEST("PC10-i7: stage_text has 'Hello.'",
         text.find("Hello.") != std::string::npos);
    TEST("PC10-i8: stage_text has 'How are you?'",
         text.find("How are you?") != std::string::npos);
    TEST("PC10-i9: stage_text has 'Goodbye.'",
         text.find("Goodbye.") != std::string::npos);
}

// ============================================================================
// PC10-j: render_conversation standalone function
// ============================================================================
static void test_render_conversation() {
    SessionManager mgr;
    auto result = mgr.create_session("@rigel", "mock");
    TEST("PC10-j1: session created", result.has_value());

    Session* s = result.value();
    s->add_message("system", "You are a helpful assistant.");
    s->add_message("user", "Hello.");
    s->add_message("assistant", "Hi there!");

    auto text = render_conversation(*s);

    TEST("PC10-j2: render has [System]",
         text.find("[System]") != std::string::npos);
    TEST("PC10-j3: render has [You]",
         text.find("[You]") != std::string::npos);
    TEST("PC10-j4: render has [Assistant]",
         text.find("[Assistant]") != std::string::npos);
    TEST("PC10-j5: render has system message text",
         text.find("You are a helpful assistant.") != std::string::npos);
    TEST("PC10-j6: render has user text",
         text.find("Hello.") != std::string::npos);
    TEST("PC10-j7: render has assistant text",
         text.find("Hi there!") != std::string::npos);
    TEST("PC10-j8: messages separated by newlines",
         text.find("\n\n") != std::string::npos);

    // Empty session
    SessionMeta empty_meta;
    empty_meta.id = "test";
    Session empty_session(std::move(empty_meta));
    auto empty_text = render_conversation(empty_session);
    TEST("PC10-j9: empty session renders without crash",
         true);  // render_conversation didn't throw
}

// ============================================================================
// Edge cases
// ============================================================================
static void test_edge_cases() {
    // send_prompt without enter()
    {
        LlmMode mode;
        std::string resp = mode.send_prompt("Hello.");
        TEST("EDGE-1: send_prompt without enter() returns error",
             resp.find("Error:") != std::string::npos);
    }

    // Very long prompt
    {
        LlmMode mode;
        mode.enter();
        std::string long_prompt(5000, 'x');
        std::string resp = mode.send_prompt(long_prompt);
        TEST("EDGE-2: long prompt doesn't crash",
             !resp.empty());
    }

    // Newline-only prompt
    {
        LlmMode mode;
        mode.enter();
        std::string resp = mode.send_prompt("\n\n");
        TEST("EDGE-3: newline-only prompt returns fallback",
             resp.find("Mock echo:") == std::string::npos &&
             resp != MockProvider::kCanonicalResponse);
    }

    // MockProvider respond with very long input (cap test)
    {
        std::string long_input(200, 'A');
        auto resp = MockProvider::respond(long_input);
        TEST("EDGE-4: respond caps long input",
             resp.size() <= 200);  // "Mock echo: " + capped at 80
    }
}

// ============================================================================
// main
// ============================================================================
int main() {
    std::cout << "=== WAVE-B Lane B4: LLM Mode Mock Round-Trip (P0-2 PC-10) ==="
              << std::endl;

    test_mock_provider_respond();
    test_mock_provider_interface();
    test_mock_provider_generate();
    test_llm_mode_enter();
    test_llm_mode_send_prompt_hello();
    test_llm_mode_stage_text();
    test_llm_mode_multiple_turns();
    test_render_conversation();
    test_edge_cases();

    std::cout << "\n=== Summary: " << g_passed << " passed, " << g_failed
              << " failed ===" << std::endl;

    return g_failed > 0 ? 1 : 0;
}
