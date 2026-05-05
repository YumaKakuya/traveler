// GATE-P0-4 Roles Unit Tests (M1-M5)
// Reference: ~/hatch-v3/packages/opencode/src/agent/roles.test.ts (489 lines)
// Port: 1:1 TypeScript -> C++20 test patterns.
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef TRAVELER_PROJECT_DIR
#define TRAVELER_PROJECT_DIR fs::current_path()
#endif

#include "roles/parser.h"
#include "roles/registry.h"
#include "roles/types.h"

namespace fs = std::filesystem;
using namespace traveler::roles;

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

static void writeRolesMd(const fs::path& dir, const std::string& content) {
    std::ofstream f(dir / "roles.md");
    f << content;
}

static bool mapContains(const std::unordered_map<std::string, ParsedRole>& map, const std::string& key) {
    return map.find(key) != map.end();
}

int main() {
    std::cout << "=== GATE-P0-4 Roles Unit Tests (M1-M5) ===" << std::endl;

    // =========================================================================
    // M1: parse valid roles.md — all 4 roles parsed, model strings correct,
    //     variant correct, system_prompt mapped
    // Reference: Hatch T2 (valid roles.md -> agent generated) + T9 (H2 body)
    // =========================================================================
    std::cout << "\n--- M1: Parse valid roles.md (4 callsigns) ---" << std::endl;
    {
        // Create tmpdir with roles.md matching the fixture content
        fs::path tmpdir = fs::temp_directory_path() / "traveler_test_m1";
        fs::create_directories(tmpdir);
        {
            std::ofstream f(tmpdir / "roles.md");
            f << R"(---
version: 1
roles:
  vega:
    model: anthropic/claude-opus-4-7
    variant: opus
    mode: primary
    description: "primary reasoning"
  altair:
    model: openai/gpt-5.4
    mode: primary
  orion:
    model: google/gemini-2.5-flash
    mode: subagent
  rigel:
    model: llamacpp/ayane
    mode: subagent
---
## vega
You are vega, a reasoning agent.

## altair
You are altair, a coding agent.
)";
        }

        auto result = parseRoles(tmpdir.string());

        TEST_RUN("M1a: parse returns success (has value)", result.has_value());
        if (result.has_value()) {
            const auto& roles = result.value();
            TEST_RUN("M1b: all 4 roles parsed (vega, altair, orion, rigel)",
                     roles.size() == 4);

            // vega
            TEST_RUN("M1c: vega present", mapContains(roles, "vega"));
            if (mapContains(roles, "vega")) {
                const auto& v = roles.at("vega");
                TEST_RUN("M1d: vega model = anthropic/claude-opus-4-7",
                         v.model == "anthropic/claude-opus-4-7");
                TEST_RUN("M1e: vega variant = opus", v.variant == "opus");
                TEST_RUN("M1f: vega mode = primary", v.mode == "primary");
                TEST_RUN("M1g: vega description contains 'primary reasoning'",
                         v.description.find("primary reasoning") != std::string::npos);
                TEST_RUN("M1h: vega prompt contains 'reasoning agent'",
                         v.prompt.find("reasoning agent") != std::string::npos);
            }

            // altair
            TEST_RUN("M1i: altair present", mapContains(roles, "altair"));
            if (mapContains(roles, "altair")) {
                const auto& a = roles.at("altair");
                TEST_RUN("M1j: altair model = openai/gpt-5.4",
                         a.model == "openai/gpt-5.4");
                TEST_RUN("M1k: altair variant empty (not specified)",
                         a.variant.empty());
                TEST_RUN("M1l: altair mode = primary", a.mode == "primary");
                TEST_RUN("M1m: altair prompt contains 'coding agent'",
                         a.prompt.find("coding agent") != std::string::npos);
            }

            // orion
            TEST_RUN("M1n: orion present", mapContains(roles, "orion"));
            if (mapContains(roles, "orion")) {
                const auto& o = roles.at("orion");
                TEST_RUN("M1o: orion model = google/gemini-2.5-flash",
                         o.model == "google/gemini-2.5-flash");
                TEST_RUN("M1p: orion mode = subagent", o.mode == "subagent");
                TEST_RUN("M1q: orion prompt empty (no H2 section)",
                         o.prompt.empty());
            }

            // rigel
            TEST_RUN("M1r: rigel present", mapContains(roles, "rigel"));
            if (mapContains(roles, "rigel")) {
                const auto& r = roles.at("rigel");
                TEST_RUN("M1s: rigel model = llamacpp/ayane",
                         r.model == "llamacpp/ayane");
                TEST_RUN("M1t: rigel mode = subagent", r.mode == "subagent");
            }
        }

        fs::remove_all(tmpdir);
    }

    // =========================================================================
    // M2: missing version field — empty result, no crash
    // Reference: Hatch T7 (version missing -> empty map + warning)
    // =========================================================================
    std::cout << "\n--- M2: Missing version field ---" << std::endl;
    {
        fs::path tmpdir = fs::temp_directory_path() / "traveler_test_m2";
        fs::create_directories(tmpdir);
        writeRolesMd(tmpdir, R"(---
roles:
  reviewer:
    model: anthropic/claude-opus-4-6
---
)");

        auto result = parseRoles(tmpdir.string());
        TEST_RUN("M2a: returns success (has value)", result.has_value());
        if (result.has_value()) {
            TEST_RUN("M2b: empty map when version field absent",
                     result.value().empty());
        }
        TEST_RUN("M2c: no crash", true);  // if we got here, no crash

        fs::remove_all(tmpdir);
    }

    // =========================================================================
    // M3: invalid role name (with special chars) — role skipped, no crash
    // Reference: Hatch T5 (invalid role name -> skip + warning)
    // =========================================================================
    std::cout << "\n--- M3: Invalid role name ---" << std::endl;
    {
        fs::path tmpdir = fs::temp_directory_path() / "traveler_test_m3";
        fs::create_directories(tmpdir);
        writeRolesMd(tmpdir, R"(---
version: 1
roles:
  "invalid name with spaces":
    model: anthropic/claude-opus-4-6
  valid_role:
    model: anthropic/claude-opus-4-6
---
)");

        auto result = parseRoles(tmpdir.string());
        TEST_RUN("M3a: returns success", result.has_value());
        if (result.has_value()) {
            const auto& roles = result.value();
            TEST_RUN("M3b: invalid name role not present",
                     !mapContains(roles, "invalid name with spaces"));
            TEST_RUN("M3c: valid_role present",
                     mapContains(roles, "valid_role"));
            TEST_RUN("M3d: only valid_role in result", roles.size() == 1);
        }
        TEST_RUN("M3e: no crash", true);

        fs::remove_all(tmpdir);
    }

    // =========================================================================
    // M4: protected name skip — "general" is overridable (parsed, not skipped);
    //     "compaction" is protected (skipped).
    // Reference: Hatch T16 (protected name -> skip) + Hatch T4 (overridable)
    // Note: C++ parser protects {"compaction", "title", "summary"}
    // =========================================================================
    std::cout << "\n--- M4: Protected/overridable name handling ---" << std::endl;
    {
        // Test protected names are skipped
        {
            fs::path tmpdir = fs::temp_directory_path() / "traveler_test_m4_protected";
            fs::create_directories(tmpdir);
            writeRolesMd(tmpdir, R"(---
version: 1
roles:
  compaction:
    model: anthropic/claude-opus-4-6
  title:
    model: anthropic/claude-opus-4-6
  summary:
    model: anthropic/claude-opus-4-6
  legitimate:
    model: anthropic/claude-opus-4-6
---
)");

            auto result = parseRoles(tmpdir.string());
            TEST_RUN("M4a: returns success", result.has_value());
            if (result.has_value()) {
                const auto& roles = result.value();
                TEST_RUN("M4b: compaction skipped (protected)",
                         !mapContains(roles, "compaction"));
                TEST_RUN("M4c: title skipped (protected)",
                         !mapContains(roles, "title"));
                TEST_RUN("M4d: summary skipped (protected)",
                         !mapContains(roles, "summary"));
                TEST_RUN("M4e: legitimate present",
                         mapContains(roles, "legitimate"));
                TEST_RUN("M4f: only legitimate parsed", roles.size() == 1);
            }

            fs::remove_all(tmpdir);
        }

        // Test "general" is overridable (not protected — parsed normally)
        {
            fs::path tmpdir = fs::temp_directory_path() / "traveler_test_m4_general";
            fs::create_directories(tmpdir);
            writeRolesMd(tmpdir, R"(---
version: 1
roles:
  general:
    model: openai/gpt-5
    mode: subagent
---
)");

            auto result = parseRoles(tmpdir.string());
            TEST_RUN("M4g: returns success", result.has_value());
            if (result.has_value()) {
                const auto& roles = result.value();
                TEST_RUN("M4h: 'general' is parsed (overridable, not protected)",
                         mapContains(roles, "general"));
                if (mapContains(roles, "general")) {
                    TEST_RUN("M4i: general model = openai/gpt-5",
                             roles.at("general").model == "openai/gpt-5");
                    TEST_RUN("M4j: general mode = subagent",
                             roles.at("general").mode == "subagent");
                }
            }

            fs::remove_all(tmpdir);
        }
    }

    // =========================================================================
    // M5: RoleRegistry::lookup returns correct model + tier + system_prompt
    // Reference: Hatch lookup pattern (roles.ts L43-45)
    // =========================================================================
    std::cout << "\n--- M5: RoleRegistry lookup ---" << std::endl;
    {
        fs::path tmpdir = fs::temp_directory_path() / "traveler_test_m5";
        fs::create_directories(tmpdir);
        writeRolesMd(tmpdir, R"(---
version: 1
roles:
  vega:
    model: anthropic/claude-opus-4-7
    variant: opus
    mode: primary
  altair:
    model: openai/gpt-5.4
    mode: primary
  orion:
    model: google/gemini-2.5-flash
    mode: subagent
---
## vega
You are vega, a reasoning agent.

## altair
You are altair, a coding agent.
)");

        RoleRegistry registry;
        registry.load(tmpdir / "roles.md");

        TEST_RUN("M5a: registry loaded 3 roles", registry.size() == 3);
        TEST_RUN("M5b: not empty", !registry.empty());

        // Lookup vega
        auto vega = registry.lookup("@vega");
        TEST_RUN("M5c: lookup @vega returns value", vega.has_value());
        if (vega.has_value()) {
            TEST_RUN("M5d: vega model = anthropic/claude-opus-4-7",
                     vega->model == "anthropic/claude-opus-4-7");
            TEST_RUN("M5e: vega tier = opus (from variant)",
                     vega->tier == "opus");
            TEST_RUN("M5f: vega callsign = @vega",
                     vega->callsign == "@vega");
            TEST_RUN("M5g: vega system_prompt contains 'reasoning agent'",
                     vega->system_prompt.find("reasoning agent") != std::string::npos);
        }

        // Lookup altair (tier derived from model name)
        auto altair = registry.lookup("altair");  // without @ prefix
        TEST_RUN("M5h: lookup altair (no @) returns value", altair.has_value());
        if (altair.has_value()) {
            TEST_RUN("M5i: altair model = openai/gpt-5.4",
                     altair->model == "openai/gpt-5.4");
            TEST_RUN("M5j: altair tier derived from model (gpt)",
                     altair->tier == "gpt");
            TEST_RUN("M5k: altair system_prompt contains 'coding agent'",
                     altair->system_prompt.find("coding agent") != std::string::npos);
        }

        // Lookup orion (no variant, tier from model)
        auto orion = registry.lookup("@orion");
        TEST_RUN("M5l: lookup @orion returns value", orion.has_value());
        if (orion.has_value()) {
            TEST_RUN("M5m: orion model = google/gemini-2.5-flash",
                     orion->model == "google/gemini-2.5-flash");
            TEST_RUN("M5n: orion tier = gemini (from model)",
                     orion->tier == "gemini");
            TEST_RUN("M5o: orion system_prompt empty (no H2 section)",
                     orion->system_prompt.empty());
        }

        // Lookup non-existent
        auto none = registry.lookup("@nonexistent");
        TEST_RUN("M5p: lookup nonexistent returns nullopt", !none.has_value());

        fs::remove_all(tmpdir);
    }

    // Summary
    std::cout << "\n=== Summary: " << g_passed << " passed, "
              << g_failed << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
