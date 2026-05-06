// GATE-P0-4 Adapter Corpus Test (PC-13)
// Reference: Traveler_Phase0_Spec_v0.1.md §8 Pass Criteria PC-13
// Reference: tests/reference_corpus/providers/adapter_msgs.json
//
// Loads the 20-fixture adapter message corpus and verifies that each
// adapter's to_request_json() produces the expected provider request
// body JSON byte-for-byte.
//
// A6 xmake target registration required — see brief for exact instructions.
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#ifndef TRAVELER_PROJECT_DIR
#define TRAVELER_PROJECT_DIR fs::current_path()
#endif

#include "adapters/anthropic_adapter.h"
#include "adapters/openai_adapter.h"
#include "adapters/google_adapter.h"
#include "adapters/llamacpp_adapter.h"
#include "llm/provider.h"

namespace fs = std::filesystem;
using namespace traveler::adapters;
using namespace traveler::llm;
using json = nlohmann::json;

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

// Read entire file into a string.
static std::string readFile(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

// Construct GenerateOptions from a fixture's input JSON object.
static GenerateOptions buildOptions(const json& input) {
    GenerateOptions opts;
    opts.model = input.at("model").get<std::string>();

    for (const auto& msg : input.at("messages")) {
        Message m;
        m.role = msg.at("role").get<std::string>();
        m.content = msg.at("content").get<std::string>();
        opts.messages.push_back(std::move(m));
    }

    if (input.contains("max_tokens")) {
        opts.max_tokens = input.at("max_tokens").get<int>();
    }

    if (input.contains("temperature")) {
        opts.temperature = input.at("temperature").get<float>();
    }

    return opts;
}

// Map an adapter name string to its to_request_json call.
static std::string adapterToJson(const std::string& adapter,
                                  const GenerateOptions& opts) {
    if (adapter == "anthropic") {
        return AnthropicAdapter::to_request_json(opts);
    } else if (adapter == "openai") {
        return OpenAIAdapter::to_request_json(opts);
    } else if (adapter == "google") {
        return GoogleAdapter::to_request_json(opts);
    } else if (adapter == "llamacpp") {
        return LlamaCppProvider::to_request_json(opts);
    }
    return {};
}

int main() {
    std::cout << "=== GATE-P0-4 Adapter Corpus Test (PC-13) ===" << std::endl;

    // Locate the fixture file
    fs::path fixtures_path = fs::path(TRAVELER_PROJECT_DIR) /
        "tests/reference_corpus/providers/adapter_msgs.json";

    if (!fs::exists(fixtures_path)) {
        std::cerr << "FATAL: fixture file not found: " << fixtures_path << std::endl;
        return 1;
    }

    std::string raw = readFile(fixtures_path);
    if (raw.empty()) {
        std::cerr << "FATAL: fixture file is empty: " << fixtures_path << std::endl;
        return 1;
    }

    json corpus;
    try {
        corpus = json::parse(raw);
    } catch (const std::exception& e) {
        std::cerr << "FATAL: JSON parse error: " << e.what() << std::endl;
        return 1;
    }

    const auto& fixtures = corpus.at("fixtures");
    std::cout << "\nLoaded " << fixtures.size() << " fixtures from "
              << fixtures_path.string() << std::endl;

    // Count per adapter for reporting
    int count_anthropic = 0, count_openai = 0, count_google = 0, count_llamacpp = 0;

    for (const auto& fx : fixtures) {
        std::string id = fx.at("id").get<std::string>();
        std::string adapter = fx.at("adapter").get<std::string>();
        std::string desc = fx.value("description", "");
        std::string expected = fx.at("expected").get<std::string>();

        std::cout << "\n--- " << id << " (" << adapter << ") ---" << std::endl;
        if (!desc.empty()) {
            std::cout << "  " << desc << std::endl;
        }

        // Build input from fixture
        GenerateOptions opts = buildOptions(fx.at("input"));

        // Call adapter
        std::string actual = adapterToJson(adapter, opts);

        // Compare byte-for-byte
        std::string testName = id + " (" + adapter + ")";
        std::cout << "  Expected: " << expected.substr(0, 120)
                  << (expected.size() > 120 ? "..." : "") << std::endl;
        std::cout << "  Actual:   " << actual.substr(0, 120)
                  << (actual.size() > 120 ? "..." : "") << std::endl;

        TEST_RUN(testName, actual == expected);

        // Per-adapter counters
        if (adapter == "anthropic") count_anthropic++;
        else if (adapter == "openai") count_openai++;
        else if (adapter == "google") count_google++;
        else if (adapter == "llamacpp") count_llamacpp++;
    }

    // --- Distribution check (spec: 5 per provider) ---
    std::cout << "\n=== Distribution ===" << std::endl;
    TEST_RUN("anthropic fixture count == 5", count_anthropic == 5);
    TEST_RUN("openai fixture count == 5", count_openai == 5);
    TEST_RUN("google fixture count == 5", count_google == 5);
    TEST_RUN("llamacpp fixture count == 5", count_llamacpp == 5);
    TEST_RUN("total fixture count == 20",
             fixtures.size() == 20 &&
             count_anthropic + count_openai + count_google + count_llamacpp == 20);

    std::cout << "\n=== Summary: " << g_passed << " passed, "
              << g_failed << " failed ===" << std::endl;

    return g_failed > 0 ? 1 : 0;
}
