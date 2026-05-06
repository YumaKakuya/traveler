// GATE-P0-4 Safety Backtracking Test (PC-6)
// Verifies that bounded quantifiers in PATH_PATTERNS prevent catastrophic
// backtracking. Injects adversarial input (10 KB of '/') through
// canonicalize() and asserts completion < 100 ms.
//
// Spec PC-6: "injecting an adversarial input that would trigger catastrophic
// backtracking on {1,} unbounded regex (e.g., a 10 KB string of /) completes
// in < 100 ms"
#include <chrono>
#include <iostream>
#include <string>

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
    std::cout << "=== GATE-P0-4 Safety Backtracking Test (PC-6) ==="
              << std::endl;

    // Generate 10 KB adversarial input: repeated '/' character
    // The Unix path regex (PII-UNIX) uses bounded {1,20} quantifier, but
    // an unbounded backtracking engine could degrade on pathological input
    // if the bounded quantifier were missing.
    constexpr size_t TEN_KB = 10 * 1024;
    std::string adversarial(TEN_KB, '/');

    // Measure canonicalize() elapsed time
    auto start = std::chrono::high_resolution_clock::now();
    std::string result = canonicalize(adversarial);
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    double elapsed_ms = static_cast<double>(elapsed_us) / 1000.0;

    std::cout << "  Adversarial input size: " << adversarial.size()
              << " bytes" << std::endl;
    std::cout << "  Elapsed: " << elapsed_ms << " ms" << std::endl;
    std::cout << "  Result length: " << result.size() << " bytes" << std::endl;

    // PC-6 spec: < 100 ms (100000 us)
    TEST_RUN("PC-6a: 10 KB adversarial '/' input completes in < 100 ms",
             elapsed_us < 100000);

    // Output should be unchanged (no PII in a string of '/' chars)
    TEST_RUN("PC-6b: output length matches input",
             result.size() == adversarial.size());

    TEST_RUN("PC-6c: output is identical to input (no PII to redact)",
             result == adversarial);

    // Summary
    std::cout << "\n=== Summary: " << g_passed << " passed, "
              << g_failed << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
