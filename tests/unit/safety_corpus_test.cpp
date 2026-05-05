// GATE-P0-4 Safety Corpus Tests (A1-A8 byte-for-byte)
// Reads each A{N}_input.txt, runs through canonicalize(), compares with
// A{N}_expected.txt byte-for-byte. Spec REQ-SAFETY-3: ALL pairs MUST PASS.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef TRAVELER_PROJECT_DIR
#define TRAVELER_PROJECT_DIR fs::current_path()
#endif

#include "safety/canonicalize.h"

namespace fs = std::filesystem;
using namespace traveler::safety;

static int g_passed = 0;
static int g_failed = 0;

static std::string readFile(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return {};
    std::ostringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

static void testCorpus(const std::string& testName, const fs::path& inputPath,
                        const fs::path& expectedPath) {
    std::string input = readFile(inputPath);
    std::string expected = readFile(expectedPath);

    if (input.empty() && !fs::file_size(inputPath)) {
        std::cerr << "  FAIL: " << testName << " — could not read input file: "
                  << inputPath << std::endl;
        g_failed++;
        return;
    }
    if (expected.empty() && !fs::file_size(expectedPath)) {
        std::cerr << "  FAIL: " << testName << " — could not read expected file: "
                  << expectedPath << std::endl;
        g_failed++;
        return;
    }

    std::string result = canonicalize(input);

    if (result == expected) {
        std::cout << "  PASS: " << testName << std::endl;
        g_passed++;
    } else {
        std::cerr << "  FAIL: " << testName << std::endl;
        std::cerr << "    Input:    " << input.substr(0, 120) << "..."
                  << (input.size() > 120 ? " (truncated)" : "") << std::endl;
        std::cerr << "    Expected: " << expected.substr(0, 120) << "..."
                  << (expected.size() > 120 ? " (truncated)" : "") << std::endl;
        std::cerr << "    Got:      " << result.substr(0, 120) << "..."
                  << (result.size() > 120 ? " (truncated)" : "") << std::endl;
        g_failed++;
    }
}

int main() {
    std::cout << "=== GATE-P0-4 Safety Corpus Tests (A1-A8) ===" << std::endl;

    // Determine corpus directory
    fs::path corpus_dir;
    {
        std::vector<fs::path> candidates = {
            fs::current_path() / "tests" / "reference_corpus" / "safety",
            fs::path(TRAVELER_PROJECT_DIR) / "tests" / "reference_corpus" / "safety",
        };
        for (const auto& c : candidates) {
            if (fs::exists(c / "A1_input.txt")) {
                corpus_dir = c;
                break;
            }
        }
        if (corpus_dir.empty()) {
            std::cerr << "FATAL: Could not find safety corpus directory." << std::endl;
            std::cerr << "Searched:" << std::endl;
            for (const auto& c : candidates) {
                std::cerr << "  " << c << std::endl;
            }
            return 1;
        }
    }

    std::cout << "Corpus dir: " << corpus_dir << std::endl;

    for (int i = 1; i <= 8; ++i) {
        std::string num = std::to_string(i);
        std::string testName = "A" + num;
        fs::path inputPath = corpus_dir / ("A" + num + "_input.txt");
        fs::path expectedPath = corpus_dir / ("A" + num + "_expected.txt");
        testCorpus(testName, inputPath, expectedPath);
    }

    // Summary
    std::cout << "\n=== Summary: " << g_passed << " passed, "
              << g_failed << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
