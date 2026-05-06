// GATE-P0-4 Safety Benchmark Test (PC-14)
// Hot-path: 1 KB input through canonical pipeline median latency < 1 ms
// over 1000 trials.
//
// Spec PC-14: "Hot-path: 1 KB input through canonical pipeline median
// latency < 1 ms over 1000 trials"
//
// Generates representative 1 KB input with mixed real-world content (paths,
// emails, tokens), runs canonicalize() 1000 times, computes median latency.
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "safety/canonicalize.h"

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

int main() {
    std::cout << "=== GATE-P0-4 Safety Benchmark Test (PC-14) ==="
              << std::endl;

    // Generate 1 KB representative input with realistic mix
    std::string input;
    input.reserve(1024);
    while (input.size() < 1024) {
        input += "Build OK for project-x86_64 at /home/user/src/main.cpp ";
        input += "Contact dev@example.com for details. Status: OK. ";
        input += "SHA: abcdef1234567890abcdef1234567890abcdef12 ";
        input += "Token: Bearer sk-abc123def456ghi789jkl ";
        input += "Connection from 10.0.0.1:443 established. ";
        input += "Config at ~/.config/traveler/settings.toml ";
        input += "CWD: /mnt/c/Users/user/project/build ";
    }
    // Trim to exactly 1024 bytes
    if (input.size() > 1024) input.resize(1024);
    if (input.size() < 1024) input.append(1024 - input.size(), '.');

    std::cout << "  Input size: " << input.size() << " bytes" << std::endl;

    constexpr int TRIALS = 1000;
    std::vector<double> elapsed_us;
    elapsed_us.reserve(TRIALS);

    // Warm-up run: compile regexes, populate caches
    volatile auto warmup = canonicalize(input);
    (void)warmup;

    // Benchmark: 1000 trials
    for (int i = 0; i < TRIALS; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        std::string result = canonicalize(input);
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      end - start).count();
        elapsed_us.push_back(static_cast<double>(us));
    }

    // Compute median
    std::sort(elapsed_us.begin(), elapsed_us.end());
    double median_us = elapsed_us[TRIALS / 2];
    double median_ms = median_us / 1000.0;

    // Compute min/max/avg for diagnostics
    double sum_us = 0;
    double min_us = elapsed_us[0];
    double max_us = elapsed_us[TRIALS - 1];
    for (auto v : elapsed_us) sum_us += v;
    double avg_us = sum_us / TRIALS;

    std::cout << "  Trials: " << TRIALS << std::endl;
    std::cout << "  Median: " << median_us << " us (" << median_ms
              << " ms)" << std::endl;
    std::cout << "  Average: " << avg_us << " us" << std::endl;
    std::cout << "  Min: " << min_us << " us" << std::endl;
    std::cout << "  Max: " << max_us << " us" << std::endl;

    // PC-14 spec: median < 1 ms (1000 us)
    TEST_RUN("PC-14a: median latency < 1 ms (1000 us) over 1000 trials",
             median_us < 1000.0);

    // Verify function still produces correct output
    std::string expected_substring = canonicalize(input);
    TEST_RUN("PC-14b: canonicalize output is non-empty",
             !expected_substring.empty());
    TEST_RUN("PC-14c: output does not contain raw email",
             expected_substring.find("dev@example.com") == std::string::npos);
    TEST_RUN("PC-14d: output contains [USER] for redacted email",
             expected_substring.find("[USER]") != std::string::npos);

    // Summary
    std::cout << "\n=== Summary: " << g_passed << " passed, "
              << g_failed << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
