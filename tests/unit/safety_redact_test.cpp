// GATE-P0-4 Safety Redact Test
// Spec PC-9: redact_for_log() with email + key + path input
// → output replaces with placeholder tokens.
#include <cstdlib>
#include <iostream>
#include <string>

#include "safety/redact.h"

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

int main() {
    std::cout << "=== GATE-P0-4 Safety Redact Test ===" << std::endl;

    // Test 1: Email redaction
    {
        std::string input = "Contact alice@example.com for details";
        std::string result = redact_for_log(input);
        TEST_RUN("R1: email replaced with [USER]",
                 result.find("[USER]") != std::string::npos);
        TEST_RUN("R1b: original email not present",
                 result.find("alice@example.com") == std::string::npos);
        // The full result should contain [USER]
        TEST_RUN("R1c: results contains 'Contact [USER] for details'",
                 result == "Contact [USER] for details");
    }

    // Test 2: Path redaction
    {
        std::string input = "Error in /home/user/project/file.cpp";
        std::string result = redact_for_log(input);
        TEST_RUN("R2: path replaced with [PATH]",
                 result.find("[PATH]") != std::string::npos);
        TEST_RUN("R2b: original path not present",
                 result.find("/home/user/") == std::string::npos);
    }

    // Test 3: API key / auth header redaction
    {
        std::string input = "Authorization: Bearer eyJhbGciOiJIUzI1NiJ9.dGVzdA.invalid";
        std::string result = redact_for_log(input);
        TEST_RUN("R3: Bearer token replaced with [MASKED]",
                 result.find("[MASKED]") != std::string::npos);
        TEST_RUN("R3b: original Bearer token not present",
                 result.find("eyJhbGci") == std::string::npos);
    }

    // Test 4: Mixed PII (email + path)
    {
        std::string input = "user@host.com pushed to /etc/app";
        std::string result = redact_for_log(input);
        TEST_RUN("R4: email replaced in mixed input",
                 result.find("[USER]") != std::string::npos);
        TEST_RUN("R4b: path replaced in mixed input (/etc/ is matched)",
                 result.find("[PATH]") != std::string::npos);
        TEST_RUN("R4c: original email absent",
                 result.find("user@host.com") == std::string::npos);
    }

    // Test 5: IP address redaction
    {
        std::string input = "Connection from 192.168.1.1 refused";
        std::string result = redact_for_log(input);
        TEST_RUN("R5: IP replaced with [PATH] (actual replacement token)",
                 result.find("[PATH]") != std::string::npos);
        TEST_RUN("R5b: original IP absent",
                 result.find("192.168.1.1") == std::string::npos);
    }

    // Test 6: Clean input (no PII) — should pass through unchanged
    {
        std::string input = "Build completed successfully with 0 warnings";
        std::string result = redact_for_log(input);
        TEST_RUN("R6: clean input unchanged", result == input);
    }

    // Test 7: Empty string
    {
        std::string result = redact_for_log("");
        TEST_RUN("R7: empty string remains empty", result.empty());
    }

    // Test 8: hostname:port redaction
    {
        std::string input = "db.internal:5432 timeout";
        std::string result = redact_for_log(input);
        TEST_RUN("R8: host:port replaced with placeholder",
                 result.find("[PATH]") != std::string::npos ||
                 result.find("[NUM]") != std::string::npos);
        TEST_RUN("R8b: original host:port not present",
                 result.find("db.internal:5432") == std::string::npos);
    }

    // Summary
    std::cout << "\n=== Summary: " << g_passed << " passed, "
              << g_failed << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
