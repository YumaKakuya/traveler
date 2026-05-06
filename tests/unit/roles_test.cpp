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
#include "roles/dispatch.h"
#include "roles/reload_cmd.h"

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

    // =========================================================================
    // M6: Dispatch path — resolve_dispatch maps @callsign to model assignment
    // PC-2: "Cockpit @-mention @altair Hello dispatches to the model assigned
    //        to @altair per roles.md (verified by inspecting provider request)"
    // No real provider calls — pure registry lookup + model string split.
    // =========================================================================
    std::cout << "\n--- M6: Dispatch path (resolve_dispatch) ---" << std::endl;
    {
        fs::path tmpdir = fs::temp_directory_path() / "traveler_test_m6";
        fs::create_directories(tmpdir);
        writeRolesMd(tmpdir, R"(---
version: 1
roles:
  altair:
    model: openai/gpt-5.4
    mode: primary
    description: "coding agent"
  vega:
    model: anthropic/claude-opus-4-7
    variant: opus
    mode: primary
  orion:
    model: google/gemini-2.5-flash
    mode: subagent
---
## altair
You are altair, a coding agent.

## vega
You are vega, a reasoning agent.
)");

        RoleRegistry registry;
        registry.load(tmpdir / "roles.md");

        // M6a: valid dispatch returns correct model + provider + model_name
        auto d_altair = resolve_dispatch(registry, "@altair");
        TEST_RUN("M6a: dispatch @altair succeeds", d_altair.has_value());
        if (d_altair.has_value()) {
            TEST_RUN("M6b: altair model = openai/gpt-5.4",
                     d_altair->model == "openai/gpt-5.4");
            TEST_RUN("M6c: altair provider = openai",
                     d_altair->provider == "openai");
            TEST_RUN("M6d: altair model_name = gpt-5.4",
                     d_altair->model_name == "gpt-5.4");
            TEST_RUN("M6e: altair system_prompt contains 'coding agent'",
                     d_altair->system_prompt.find("coding agent") != std::string::npos);
            TEST_RUN("M6f: altair callsign = @altair",
                     d_altair->callsign == "@altair");
        }

        // M6g: dispatch without @ prefix works
        auto d_altair_noat = resolve_dispatch(registry, "altair");
        TEST_RUN("M6g: dispatch altair (no @) succeeds", d_altair_noat.has_value());
        if (d_altair_noat.has_value()) {
            TEST_RUN("M6h: altair model matches with/without @",
                     d_altair_noat->model == d_altair->model);
        }

        // M6i: dispatch vega with tier from variant
        auto d_vega = resolve_dispatch(registry, "@vega");
        TEST_RUN("M6i: dispatch @vega succeeds", d_vega.has_value());
        if (d_vega.has_value()) {
            TEST_RUN("M6j: vega tier = opus", d_vega->tier == "opus");
            TEST_RUN("M6k: vega provider = anthropic", d_vega->provider == "anthropic");
            TEST_RUN("M6l: vega system_prompt contains 'reasoning agent'",
                     d_vega->system_prompt.find("reasoning agent") != std::string::npos);
        }

        // M6m: dispatch unknown callsign returns error (no network call)
        auto d_unknown = resolve_dispatch(registry, "@nonexistent");
        TEST_RUN("M6m: dispatch unknown callsign fails", !d_unknown.has_value());
        if (!d_unknown.has_value()) {
            TEST_RUN("M6n: unknown error contains 'unknown callsign'",
                     d_unknown.error().message.find("unknown callsign") != std::string::npos);
        }

        fs::remove_all(tmpdir);
    }

    // M6o: dispatch on empty registry returns EmptyRegistry error
    {
        RoleRegistry empty_reg;
        auto d_empty = resolve_dispatch(empty_reg, "@vega");
        TEST_RUN("M6o: dispatch on empty registry fails", !d_empty.has_value());
        if (!d_empty.has_value()) {
            TEST_RUN("M6p: empty registry error message is descriptive",
                     !d_empty.error().message.empty());
        }
    }

    // =========================================================================
    // M7: /roles-reload command — reload updates registry from source path
    // PC-3: "/roles-reload after editing roles.md updates RoleRegistry;
    //        subsequent @-mention uses the new mapping"
    // =========================================================================
    std::cout << "\n--- M7: /roles-reload command ---" << std::endl;
    {
        fs::path tmpdir = fs::temp_directory_path() / "traveler_test_m7";
        fs::create_directories(tmpdir);
        fs::path roles_path = tmpdir / "roles.md";
        writeRolesMd(tmpdir, R"(---
version: 1
roles:
  vega:
    model: anthropic/claude-opus-4-7
    variant: opus
    mode: primary
---
## vega
You are vega.
)");

        RoleRegistry registry;
        registry.load(roles_path);

        TEST_RUN("M7a: initial load has 1 role", registry.size() == 1);
        TEST_RUN("M7b: source_path matches", registry.source_path() == roles_path);

        // Reload — same file, no change
        auto r1 = reload_roles_cmd(registry);
        TEST_RUN("M7c: reload succeeds", r1.ok);
        TEST_RUN("M7d: reload count = 1", r1.count == 1);

        // Edit roles.md on disk — add altair
        {
            std::ofstream f(roles_path);  // overwrite
            f << R"(---
version: 1
roles:
  vega:
    model: anthropic/claude-opus-4-7
    variant: opus
    mode: primary
  altair:
    model: openai/gpt-5.4
    mode: primary
---
## vega
You are vega.
)";
        }

        auto r2 = reload_roles_cmd(registry);
        TEST_RUN("M7e: reload after edit succeeds", r2.ok);
        TEST_RUN("M7f: reload count = 2 (vega + altair)", r2.count == 2);

        // Verify new mapping is active
        auto altair = registry.lookup("@altair");
        TEST_RUN("M7g: altair now resolvable after reload", altair.has_value());
        if (altair.has_value()) {
            TEST_RUN("M7h: altair model = openai/gpt-5.4",
                     altair->model == "openai/gpt-5.4");
        }

        fs::remove_all(tmpdir);
    }

    // M7i: reload on never-loaded registry returns error
    {
        RoleRegistry never_loaded;
        auto r = reload_roles_cmd(never_loaded);
        TEST_RUN("M7i: reload on never-loaded registry fails", !r.ok);
        TEST_RUN("M7j: never-loaded error message is descriptive",
                 !r.message.empty());
    }

    // =========================================================================
    // M8: Config-override path — explicit path takes effect
    // PC-16 (TB-D): "roles_test.cpp verifies config-override and default-path
    //               lookup; CI grep confirms no multi-tier resolution path"
    // =========================================================================
    std::cout << "\n--- M8: Config-override path (PC-16/TB-D) ---" << std::endl;
    {
        // Create two different roles files in different locations
        fs::path dir_a = fs::temp_directory_path() / "traveler_test_m8a";
        fs::path dir_b = fs::temp_directory_path() / "traveler_test_m8b";
        fs::create_directories(dir_a);
        fs::create_directories(dir_b);

        writeRolesMd(dir_a, R"(---
version: 1
roles:
  vega:
    model: anthropic/claude-opus-4-7
    variant: opus
---
## vega
You are vega.
)");

        writeRolesMd(dir_b, R"(---
version: 1
roles:
  altair:
    model: openai/gpt-5.4
---
## altair
You are altair.
)");

        // M8a: load from path A (config override) — get vega only
        RoleRegistry registry;
        registry.load(dir_a / "roles.md");
        TEST_RUN("M8a: config-override path loads vega", registry.size() == 1);
        TEST_RUN("M8b: source_path points to config-override path",
                 registry.source_path().string().find("traveler_test_m8a") != std::string::npos);
        auto vega = registry.lookup("@vega");
        TEST_RUN("M8c: vega present from override path", vega.has_value());
        auto altair_before = registry.lookup("@altair");
        TEST_RUN("M8d: altair NOT present from path A", !altair_before.has_value());

        // M8e: registry does NOT automatically resolve from a different path
        // (single-source — no multi-tier fallback)
        // The registry only knows about path A and has no knowledge of path B.

        fs::remove_all(dir_a);
        fs::remove_all(dir_b);
    }

    // =========================================================================
    // M9: Default-path lookup
    // PC-16 (TB-D): verify default-path style location works for single-source
    // =========================================================================
    std::cout << "\n--- M9: Default-path lookup (PC-16/TB-D) ---" << std::endl;
    {
        fs::path tmpdir = fs::temp_directory_path() / "traveler_test_m9";
        fs::create_directories(tmpdir);
        fs::path default_roles = tmpdir / "roles.md";

        writeRolesMd(tmpdir, R"(---
version: 1
roles:
  vega:
    model: anthropic/claude-opus-4-7
    variant: opus
---
)");

        // M9a: load from a default-style path works
        RoleRegistry registry;
        registry.load(default_roles);
        TEST_RUN("M9a: default-path load succeeds", registry.size() == 1);
        TEST_RUN("M9b: source_path is set",
                 !registry.source_path().empty());
        TEST_RUN("M9c: lookup finds vega",
                 registry.lookup("@vega").has_value());

        // M9d: reload from the same path keeps single-source
        auto r = reload_roles_cmd(registry);
        TEST_RUN("M9d: reload from default-path succeeds", r.ok);
        TEST_RUN("M9e: reload count preserved", r.count == 1);
        TEST_RUN("M9f: source_path unchanged after reload",
                 registry.source_path() == default_roles);

        // M9g: verify single-source — registry loaded from one file path
        // does not scan or resolve a different path. The source_path
        // remains the originally configured path throughout reload.
        std::string src_path_str = registry.source_path().string();
        TEST_RUN("M9g: single-source — source_path stable after reload",
                 src_path_str.find("traveler_test_m9") != std::string::npos);

        fs::remove_all(tmpdir);
    }

    // Summary
    std::cout << "\n=== Summary: " << g_passed << " passed, "
              << g_failed << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
