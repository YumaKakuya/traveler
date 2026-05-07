// WAVE-C C1: LlamaCpp adapter state/error behavior tests.
// Reference: briefs/wave-c-c1-offline-ayane-dispatch-2026-05-07.md
//
// Tests the three bounded states of LlamaCppProvider:
//   ModelMissing:            No GGUF file at model_path.
//   ModelPresentNoInference: GGUF exists but no llama support compiled.
//   InferenceReady:          GGUF exists + llama support compiled.
//
// No real GGUF model files or wired inference required.
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "adapters/llamacpp_adapter.h"
#include "llm/provider.h"

using namespace traveler::adapters;
using namespace traveler::llm;

namespace fs = std::filesystem;

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
// Helper: create a small temporary file to simulate a model file
// ============================================================================
static fs::path create_temp_model_file() {
    auto tmp_dir = fs::temp_directory_path() / "traveler_test_llamacpp";
    std::error_code ec;
    fs::create_directories(tmp_dir, ec);
    if (ec) return {};
    auto model_path = tmp_dir / "test-model.gguf";
    std::ofstream ofs(model_path, std::ios::binary);
    if (!ofs) return {};
    const char* content = "TRAVELER_TEST_GGUF_MOCK_DATA";
    ofs.write(content, std::strlen(content));
    ofs.close();
    return model_path;
}

// ============================================================================
// Test: ModelMissing state
// ============================================================================
static void test_model_missing() {
    std::cout << "\n--- ModelMissing state ---" << std::endl;

    LlamaCppProvider provider("/nonexistent/path/to/model.gguf");

    TEST("state = ModelMissing",
         provider.state() == LlamaCppState::ModelMissing);
    TEST("is_ready false", !provider.is_ready());
    TEST("model_file_exists false", !provider.model_file_exists());
    TEST("supports_streaming false", !provider.supports_streaming());
    TEST("id = llamacpp", provider.id() == "llamacpp");
    TEST("model_path preserved", provider.model_path() == "/nonexistent/path/to/model.gguf");

    // generate() should return error
    GenerateOptions opts;
    opts.model = "llamacpp/test";
    Message msg;
    msg.role = "user";
    msg.content = "hello";
    opts.messages.push_back(msg);

    auto result = provider.generate(opts, nullptr);
    TEST("generate returns error in ModelMissing", !result.has_value());
    if (!result.has_value()) {
        TEST("error message mentions model not found",
             result.error().message.find("model not found") != std::string::npos);
        TEST("error message mentions GGUF",
             result.error().message.find("GGUF") != std::string::npos);
        TEST("error message mentions model_path",
             result.error().message.find("nonexistent") != std::string::npos);
    }
}

// ============================================================================
// Test: ModelPresentNoInference state
// ============================================================================
static void test_model_present_no_inference() {
    std::cout << "\n--- ModelPresentNoInference state ---" << std::endl;

    auto model_path = create_temp_model_file();
    TEST("temp model file created", !model_path.empty());
    if (model_path.empty()) return;

    LlamaCppProvider provider(model_path.string());

    // Since TRAVELER_WITH_LLAMA is not defined in the test build,
    // the state should be ModelPresentNoInference when the file exists.
    // If the test build happens to have TRAVELER_WITH_LLAMA, handle both cases.
    if (provider.state() == LlamaCppState::ModelPresentNoInference) {
        TEST("state = ModelPresentNoInference", true);
        TEST("is_ready false", !provider.is_ready());
        TEST("model_file_exists true", provider.model_file_exists());
        TEST("supports_streaming false", !provider.supports_streaming());
    } else if (provider.state() == LlamaCppState::InferenceReady) {
        // Build was compiled with TRAVELER_WITH_LLAMA
        TEST("state = InferenceReady (this build has WITH_LLAMA)", true);
        TEST("is_ready true", provider.is_ready());
        TEST("model_file_exists true", provider.model_file_exists());
        TEST("supports_streaming true", provider.supports_streaming());
    }

    GenerateOptions opts;
    opts.model = "llamacpp/test";
    Message msg;
    msg.role = "user";
    msg.content = "hello";
    opts.messages.push_back(msg);

    auto result = provider.generate(opts, nullptr);
    TEST("generate returns error (no wired inference)", !result.has_value());
    if (!result.has_value()) {
        if (provider.state() == LlamaCppState::ModelPresentNoInference) {
            TEST("error mentions 'no llama support'",
                 result.error().message.find("llama.cpp") != std::string::npos);
        } else {
            TEST("error mentions 'not yet wired'",
                 result.error().message.find("not yet wired") != std::string::npos);
        }
    }

    // Clean up
    std::error_code ec;
    fs::remove(model_path, ec);
}

// ============================================================================
// Test: Forced state constructor
// ============================================================================
static void test_forced_states() {
    std::cout << "\n--- Forced state constructor ---" << std::endl;

    LlamaCppProvider missing_provider("/some/path", LlamaCppState::ModelMissing);
    TEST("forced ModelMissing", missing_provider.state() == LlamaCppState::ModelMissing);
    TEST("forced ModelMissing is_ready false", !missing_provider.is_ready());
    TEST("forced ModelMissing model_file_exists false", !missing_provider.model_file_exists());

    LlamaCppProvider noinf_provider("/some/path", LlamaCppState::ModelPresentNoInference);
    TEST("forced ModelPresentNoInference",
         noinf_provider.state() == LlamaCppState::ModelPresentNoInference);
    TEST("forced ModelPresentNoInference is_ready false", !noinf_provider.is_ready());
    TEST("forced ModelPresentNoInference model_file_exists true",
         noinf_provider.model_file_exists());
    TEST("forced ModelPresentNoInference supports_streaming false",
         !noinf_provider.supports_streaming());

    LlamaCppProvider ready_provider("/some/path", LlamaCppState::InferenceReady);
    TEST("forced InferenceReady",
         ready_provider.state() == LlamaCppState::InferenceReady);
    TEST("forced InferenceReady is_ready true", ready_provider.is_ready());
    TEST("forced InferenceReady model_file_exists true",
         ready_provider.model_file_exists());
    TEST("forced InferenceReady supports_streaming true",
         ready_provider.supports_streaming());

    // Even in InferenceReady, generate returns error (current-resource gap)
    GenerateOptions opts;
    opts.model = "test";
    Message msg;
    msg.role = "user";
    msg.content = "hi";
    opts.messages.push_back(msg);

    auto result = ready_provider.generate(opts, nullptr);
    TEST("forced InferenceReady generate returns error (not yet wired)",
         !result.has_value());
    if (!result.has_value()) {
        TEST("error mentions 'not yet wired'",
             result.error().message.find("not yet wired") != std::string::npos);
    }
}

// ============================================================================
// Test: to_string for LlamaCppState
// ============================================================================
static void test_to_string_state() {
    std::cout << "\n--- to_string for LlamaCppState ---" << std::endl;

    TEST("to_string ModelMissing",
         std::string(to_string(LlamaCppState::ModelMissing)) == "model-missing");
    TEST("to_string ModelPresentNoInference",
         std::string(to_string(LlamaCppState::ModelPresentNoInference)) ==
             "model-present-no-inference");
    TEST("to_string InferenceReady",
         std::string(to_string(LlamaCppState::InferenceReady)) == "inference-ready");
}

// ============================================================================
// main
// ============================================================================
int main() {
    std::cout << "=== WAVE-C C1: LlamaCpp Adapter State Tests ===" << std::endl;

    test_model_missing();
    test_model_present_no_inference();
    test_forced_states();
    test_to_string_state();

    std::cout << "\n=== Summary: " << g_passed << " passed, " << g_failed
              << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
