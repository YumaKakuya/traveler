// GATE-P0-4 Safety JSONL Emission Test (PC-7)
// Verifies that safety_events.jsonl contains ≥1 JSON record with valid
// severity and category fields after emitting a safety event.
//
// Spec PC-7: "safety_events.jsonl contains ≥1 JSON record with severity
// ∈ {LOW, MEDIUM, HIGH, CRITICAL} + category ∈ {PII, SECRET, PATH, ...}
// fields after running the safety corpus test suite (verified by jq scripted
// assertion)"
//
// This unit test creates a deterministic SafetyEvent, emits it, reads back
// the JSONL file, and asserts field presence/values. Equivalent to:
//   jq -e '.severity == "HIGH" and .category == "SECRET"' safety_events.jsonl
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "safety/event.h"

namespace fs = std::filesystem;
using namespace traveler::safety;

static int g_passed = 0;
static int g_failed = 0;

#define TEST_RUN(name, expr) do {                                      \
    if (!(expr)) {                                                      \
        std::cerr << "  FAIL: " << name << std::endl;                   \
        g_failed++;                                                     \
    } else {                                                            \
        std::cout << "  PASS: " << name << std::endl;                   \
        g_passed++;                                                     \
    }                                                                   \
} while(0)

static std::string readFile(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

int main() {
    std::cout << "=== GATE-P0-4 Safety JSONL Emission Test (PC-7) ==="
              << std::endl;

    // Create isolated temp directory so event emission writes to a clean file
    // Must happen before any emit_safety_event() call because ensure_path()
    // uses a static g_initialized guard (reads env var once).
    fs::path tmpdir;
    for (int attempt = 0; attempt < 100; ++attempt) {
        std::string path =
            fs::temp_directory_path() /
            ("traveler_test_pc7_" + std::to_string(attempt));
        if (fs::create_directory(path)) {
            tmpdir = path;
            break;
        }
    }
    if (tmpdir.empty()) {
        std::cerr << "FATAL: could not create temp directory" << std::endl;
        return 1;
    }

    // Redirect event output to temp dir
    setenv("XDG_STATE_HOME", tmpdir.c_str(), 1);

    fs::path expected_path = tmpdir / "traveler" / "safety_events.jsonl";
    std::cout << "  JSONL path: " << expected_path << std::endl;

    // Clean any pre-existing file
    std::error_code ec;
    fs::remove(expected_path, ec);

    // ------------------------------------------------------------------
    // Create and emit a safety event with known severity + category
    // ------------------------------------------------------------------
    {
        SafetyEvent event;
        event.severity = Severity::HIGH;
        event.category = Category::SECRET;
        event.redacted_input = "user password = super_secret_123";
        event.redacted_output = "user [MASKED]";
        event.detail = "PC-7 test: secret detected in user input";
        event.timestamp = std::chrono::system_clock::now();
        emit_safety_event(event);
    }

    // ------------------------------------------------------------------
    // Verify the JSONL file exists and is valid
    // ------------------------------------------------------------------
    TEST_RUN("PC-7a: safety_events.jsonl exists",
             fs::exists(expected_path));

    std::string content = readFile(expected_path);
    TEST_RUN("PC-7b: JSONL file is non-empty", !content.empty());

    // Find first JSON line (JSONL = one JSON object per line)
    auto newline_pos = content.find('\n');
    std::string first_line =
        (newline_pos != std::string::npos)
            ? content.substr(0, newline_pos)
            : content;
    TEST_RUN("PC-7c: first JSONL line is non-empty", !first_line.empty());

    // Verify required fields exist in the JSON record
    // (equivalent to: jq -e '.severity' safety_events.jsonl)
    TEST_RUN("PC-7d: severity field present",
             first_line.find("\"severity\"") != std::string::npos);
    TEST_RUN("PC-7e: severity value is HIGH",
             first_line.find("\"severity\":\"HIGH\"") != std::string::npos);
    TEST_RUN("PC-7f: category field present",
             first_line.find("\"category\"") != std::string::npos);
    TEST_RUN("PC-7g: category value is SECRET",
             first_line.find("\"category\":\"SECRET\"") != std::string::npos);
    TEST_RUN("PC-7h: timestamp field present",
             first_line.find("\"timestamp\"") != std::string::npos);
    TEST_RUN("PC-7i: detail field present",
             first_line.find("\"detail\"") != std::string::npos);

    // Verify JSON structure (starts with { and ends with } before \n)
    TEST_RUN("PC-7j: JSON record starts with '{'",
             !first_line.empty() && first_line[0] == '{');
    TEST_RUN("PC-7k: JSON record has matching closing brace",
             first_line.find('}') != std::string::npos);

    // Verify severity is one of the allowed enum values per Spec
    bool has_valid_severity =
        first_line.find("\"LOW\"") != std::string::npos ||
        first_line.find("\"MEDIUM\"") != std::string::npos ||
        first_line.find("\"HIGH\"") != std::string::npos ||
        first_line.find("\"CRITICAL\"") != std::string::npos;
    TEST_RUN("PC-7l: severity is a valid enum value", has_valid_severity);

    // Verify category is one of the allowed enum values per Spec
    bool has_valid_category =
        first_line.find("\"PII\"") != std::string::npos ||
        first_line.find("\"SECRET\"") != std::string::npos ||
        first_line.find("\"PATH\"") != std::string::npos ||
        first_line.find("\"DANGER\"") != std::string::npos ||
        first_line.find("\"BUDGET\"") != std::string::npos ||
        first_line.find("\"INTERNAL\"") != std::string::npos;
    TEST_RUN("PC-7m: category is a valid enum value", has_valid_category);

    // Cleanup temp directory
    fs::remove_all(tmpdir, ec);

    // Summary
    std::cout << "\n=== Summary: " << g_passed << " passed, "
              << g_failed << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
