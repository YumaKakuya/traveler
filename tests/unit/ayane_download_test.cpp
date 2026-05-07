// WAVE-C C1: Ayane download/cache module tests.
// Reference: briefs/wave-c-c1-offline-ayane-dispatch-2026-05-07.md
//
// Tests manifest selection, local fixture acquisition, SHA-256 success/failure,
// network unsupported-current-resource, and cache operations.
//
// No real network access or real GGUF model files required.
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef TRAVELER_PROJECT_DIR
#define TRAVELER_PROJECT_DIR fs::current_path()
#endif

#include "assets/ayane_manifest.h"
#include "llm/ayane_download.h"
#include "util/hash.h"

namespace fs = std::filesystem;
using namespace traveler::assets;
using namespace traveler::llm;
using namespace traveler::util;

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
// Helper: create a temp directory and set HOME for cache isolation
// ============================================================================
struct TempHomeGuard {
    std::string orig_home;
    std::string tmp_home;

    TempHomeGuard() {
        const char* home = std::getenv("HOME");
        if (home) orig_home = home;

        tmp_home = (fs::temp_directory_path() / "traveler_test_ayane_XXXXXX").string();
        tmp_home += "_" + std::to_string(std::time(nullptr));
        fs::create_directories(tmp_home);
        setenv("HOME", tmp_home.c_str(), 1);

        fs::create_directories(fs::path(tmp_home) / ".local" / "share" / "traveler" / "models");
    }

    ~TempHomeGuard() {
        if (!tmp_home.empty()) {
            std::error_code ec;
            fs::remove_all(tmp_home, ec);
        }
        if (!orig_home.empty()) {
            setenv("HOME", orig_home.c_str(), 1);
        }
    }
};

// ============================================================================
// Helper: full path to a fixture file under tests/fixtures/ayane/
// ============================================================================
static fs::path fixture_path(std::string_view name) {
    return fs::path(TRAVELER_PROJECT_DIR) / "tests" / "fixtures" / "ayane" / name;
}

// ============================================================================
// Test: Manifest selection
// ============================================================================
static void test_manifest_selection() {
    std::cout << "\n--- Manifest selection ---" << std::endl;

    auto* phi_prod = find_production_manifest(AyaneModelFamily::Phi35Mini);
    TEST("prod phi35mini found", phi_prod != nullptr);
    TEST("prod phi35mini is_production", phi_prod->is_production);
    TEST("prod phi35mini requires completion", phi_prod->requires_manifest_completion());
    TEST("prod phi35mini not downloadable", !phi_prod->is_downloadable());

    auto* llama_prod = find_production_manifest(AyaneModelFamily::Llama32_1b);
    TEST("prod llama32-1b found", llama_prod != nullptr);
    TEST("prod llama32-1b is_production", llama_prod->is_production);
    TEST("prod llama32-1b requires completion", llama_prod->requires_manifest_completion());

    auto* phi_test = find_test_fixture(AyaneModelFamily::Phi35Mini);
    TEST("test phi35mini found", phi_test != nullptr);
    TEST("test phi35mini not production", !phi_test->is_production);
    TEST("test phi35mini has fixture SHA", !phi_test->sha256_hex.empty());

    auto* llama_test = find_test_fixture(AyaneModelFamily::Llama32_1b);
    TEST("test llama32-1b found", llama_test != nullptr);
    TEST("test llama32-1b not production", !llama_test->is_production);

    auto p1 = parse_family("phi-3.5-mini");
    TEST("parse 'phi-3.5-mini'", p1.has_value() && *p1 == AyaneModelFamily::Phi35Mini);
    auto p2 = parse_family("llama-3.2-1b");
    TEST("parse 'llama-3.2-1b'", p2.has_value() && *p2 == AyaneModelFamily::Llama32_1b);
    auto p3 = parse_family("unknown");
    TEST("parse unknown returns nullopt", !p3.has_value());

    TEST("2 production entries", kAyaneProductionEntryCount == 2);
    TEST("2 test fixture entries", kAyaneTestFixtureCount == 2);

    TEST("test phi gguf filename matches fixture",
         phi_test->gguf_filename == "test-fixture-phi35mini.gguf");
    TEST("test llama gguf filename matches fixture",
         llama_test->gguf_filename == "test-fixture-llama32-1b.gguf");
}

// ============================================================================
// Test: SHA-256 utility function
// ============================================================================
static void test_sha256_utility() {
    std::cout << "\n--- SHA-256 utility ---" << std::endl;

    std::string empty_hex = sha256_hex_string("");
    TEST("empty string SHA-256",
         empty_hex == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    std::string hello_hex = sha256_hex_string("hello");
    TEST("'hello' SHA-256",
         hello_hex == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");

    std::string known_hex = sha256_hex_string("TRAVELER_KNOWN_HASH_TEST_STRING");
    TEST("known string SHA-256",
         known_hex == "3e8b71e4ba103604b50e6c150a53254322009e5caab66d76ed03c6aa8274ccbd");

    std::vector<uint8_t> vec = {'h', 'e', 'l', 'l', 'o'};
    std::string vec_hex = sha256_hex(vec);
    TEST("vector<uint8_t> overload matches", vec_hex == hello_hex);

    const std::byte raw[] = {std::byte{'h'}, std::byte{'i'}};
    std::string raw_hex = sha256_hex(raw, 2);
    std::string expected_hi = sha256_hex_string("hi");
    TEST("raw bytes SHA-256", raw_hex == expected_hi);

    auto digest = sha256(raw, 2);
    TEST("digest size is 32 bytes", digest.size() == 32);
}

// ============================================================================
// Test: SHA-256 file verification (sha256_file)
// ============================================================================
static void test_sha256_file_verification() {
    std::cout << "\n--- SHA-256 file verification ---" << std::endl;

    auto missing = sha256_file(fs::path("/nonexistent/path.gguf"));
    TEST("sha256_file on non-existent returns nullopt", !missing.has_value());

    auto phi_path = fixture_path("test-fixture-phi35mini.gguf");
    TEST("phi fixture exists", fs::exists(phi_path));
    auto phi_sha = sha256_file(phi_path);
    TEST("sha256_file returns value for phi", phi_sha.has_value());
    TEST("phi fixture SHA-256 matches expected",
         *phi_sha == "c4d0997a7832893b32f7a3bce3fee66082e81a1c4c520c88576374951ae6a36f");

    auto llama_path = fixture_path("test-fixture-llama32-1b.gguf");
    TEST("llama fixture exists", fs::exists(llama_path));
    auto llama_sha = sha256_file(llama_path);
    TEST("sha256_file returns value for llama", llama_sha.has_value());
    TEST("llama fixture SHA-256 matches expected",
         *llama_sha == "842a5c5b5a1f83ded592adaf8057b5fefd2af5d40354b32b00818145f3fd37d1");
}

// ============================================================================
// Test: Local fixture acquisition (production entry, empty SHA -> no check)
// ============================================================================
static void test_fixture_acquisition_success() {
    std::cout << "\n--- Fixture acquisition success (production entry, empty SHA) ---" << std::endl;
    TempHomeGuard guard;

    auto* phi_prod = find_production_manifest(AyaneModelFamily::Phi35Mini);
    TEST("phi prod entry exists for acquire", phi_prod != nullptr);

    auto fixture = fixture_path("test-fixture-phi35mini.gguf");
    auto result = ayane_acquire(*phi_prod, fixture);

    TEST("ayane_acquire with fixture succeeds", result.has_value());
    if (result.has_value()) {
        TEST("result family is Phi35Mini",
             result->family == AyaneModelFamily::Phi35Mini);
        TEST("result from_fixture is true", result->from_fixture);
        TEST("result from_cache is false", !result->from_cache);
        TEST("result model_path is non-empty", !result->model_path.empty());
        TEST("result sha256_verified matches file",
             result->sha256_verified == "c4d0997a7832893b32f7a3bce3fee66082e81a1c4c520c88576374951ae6a36f");

        TEST("cached file exists", fs::exists(result->model_path));
        TEST("cached file is regular", fs::is_regular_file(result->model_path));
    }

    auto result2 = ayane_acquire(*phi_prod, fixture);
    TEST("second acquire succeeds", result2.has_value());
    if (result2.has_value()) {
        TEST("second acquire from_cache is true", result2->from_cache);
    }
}

// ============================================================================
// Test: SHA-256 mismatch detection
// ============================================================================
static void test_sha256_mismatch() {
    std::cout << "\n--- SHA-256 mismatch detection ---" << std::endl;
    TempHomeGuard guard;

    auto* phi_test = find_test_fixture(AyaneModelFamily::Phi35Mini);
    TEST("phi test fixture entry exists", phi_test != nullptr);

    auto fixture = fixture_path("test-fixture-phi35mini.gguf");
    auto result = ayane_acquire(*phi_test, fixture);

    TEST("ayane_acquire with mismatched SHA returns error", !result.has_value());
    if (!result.has_value()) {
        auto& err = result.error();
        TEST("error code is Sha256Mismatch",
             err.code == AyaneDownloadError::Code::Sha256Mismatch);
        TEST("error has expected_sha256",
             err.expected_sha256.has_value() &&
             *err.expected_sha256 == phi_test->sha256_hex);
        TEST("error has actual_sha256",
             err.actual_sha256.has_value());
    }
}

// ============================================================================
// Test: Network unsupported
// ============================================================================
static void test_network_unsupported() {
    std::cout << "\n--- Network unsupported ---" << std::endl;

    auto* phi_test = find_test_fixture(AyaneModelFamily::Phi35Mini);
    TEST("phi test fixture entry exists for download test", phi_test != nullptr);

    auto result = ayane_network_download(*phi_test, nullptr);
    TEST("ayane_network_download returns error", !result.has_value());
    if (!result.has_value()) {
        TEST("error code is NetworkUnsupported",
             result.error().code == AyaneDownloadError::Code::NetworkUnsupported);
    }
}

// ============================================================================
// Test: Manifest incomplete error
// ============================================================================
static void test_manifest_incomplete() {
    std::cout << "\n--- Manifest incomplete ---" << std::endl;
    TempHomeGuard guard;

    auto* phi_prod = find_production_manifest(AyaneModelFamily::Phi35Mini);

    auto result = ayane_acquire(*phi_prod, std::nullopt);
    TEST("ayane_acquire without fixture on prod entry returns error",
         !result.has_value());
    if (!result.has_value()) {
        TEST("error code is ManifestIncomplete",
             result.error().code == AyaneDownloadError::Code::ManifestIncomplete);
    }
}

// ============================================================================
// Test: ayane_can_download
// ============================================================================
static void test_ayane_can_download() {
    std::cout << "\n--- ayane_can_download ---" << std::endl;

    auto* phi_prod = find_production_manifest(AyaneModelFamily::Phi35Mini);
    TEST("prod entry can_download = false",
         !ayane_can_download(*phi_prod));

    auto* phi_test = find_test_fixture(AyaneModelFamily::Phi35Mini);
    TEST("test fixture entry can_download = true",
         ayane_can_download(*phi_test));

    auto* llama_prod = find_production_manifest(AyaneModelFamily::Llama32_1b);
    TEST("prod llama can_download = false",
         !ayane_can_download(*llama_prod));
}

// ============================================================================
// Test: Cache scan on empty and purge
// ============================================================================
static void test_cache_scan_purge() {
    std::cout << "\n--- Cache scan and purge ---" << std::endl;
    TempHomeGuard guard;

    // On empty cache, scan returns empty
    auto cached = ayane_scan_cache();
    TEST("scan on empty cache returns empty", cached.empty());

    // First available on empty cache returns nullopt
    auto first = ayane_first_available();
    TEST("first_available on empty cache", !first.has_value());

    // Purge on empty cache returns 0
    auto purge_result = ayane_purge_cache();
    TEST("purge on empty cache succeeds", purge_result.has_value());
    if (purge_result.has_value()) {
        TEST("purge on empty cache returns 0", *purge_result == 0);
    }

    // Acquire a fixture via production entry (SHA-256 skip, copy to cache)
    auto* phi_prod = find_production_manifest(AyaneModelFamily::Phi35Mini);
    auto fixture = fixture_path("test-fixture-phi35mini.gguf");
    auto acq = ayane_acquire(*phi_prod, fixture);
    TEST("acquire succeeded for purge test", acq.has_value());

    // Purge removes cached file
    auto purge2 = ayane_purge_cache();
    TEST("purge after acquisition succeeds", purge2.has_value());
    if (purge2.has_value()) {
        TEST("purge removes exactly 1 file", *purge2 == 1);
    }
}

// ============================================================================
// Test: Filenotfound error
// ============================================================================
static void test_fixture_not_found() {
    std::cout << "\n--- Fixture not found error ---" << std::endl;
    TempHomeGuard guard;

    auto* phi_prod = find_production_manifest(AyaneModelFamily::Phi35Mini);

    auto result = ayane_acquire(*phi_prod,
        fs::path("/nonexistent/fixture.gguf"));
    TEST("acquire with bad fixture returns error", !result.has_value());
    if (!result.has_value()) {
        TEST("error code is FileNotFound",
             result.error().code == AyaneDownloadError::Code::FileNotFound);
    }
}

// ============================================================================
// Test: to_string for AyaneModelFamily
// ============================================================================
static void test_to_string_family() {
    std::cout << "\n--- to_string for AyaneModelFamily ---" << std::endl;

    TEST("to_string Phi35Mini",
         std::string(to_string(AyaneModelFamily::Phi35Mini)) == "phi-3.5-mini");
    TEST("to_string Llama32_1b",
         std::string(to_string(AyaneModelFamily::Llama32_1b)) == "llama-3.2-1b");
}

// ============================================================================
// Test: AyaneDownloadError factory methods
// ============================================================================
static void test_error_factories() {
    std::cout << "\n--- AyaneDownloadError factory methods ---" << std::endl;

    auto err1 = AyaneDownloadError::NetworkUnsupported("net unsupported");
    TEST("NetworkUnsupported code", err1.code == AyaneDownloadError::Code::NetworkUnsupported);
    TEST("NetworkUnsupported message", err1.message == "net unsupported");

    auto err2 = AyaneDownloadError::Sha256Mismatch("mismatch", "expected", "actual");
    TEST("Sha256Mismatch code", err2.code == AyaneDownloadError::Code::Sha256Mismatch);
    TEST("Sha256Mismatch expected", err2.expected_sha256.has_value() && *err2.expected_sha256 == "expected");
    TEST("Sha256Mismatch actual", err2.actual_sha256.has_value() && *err2.actual_sha256 == "actual");
}

// ============================================================================
// main
// ============================================================================
int main() {
    std::cout << "=== WAVE-C C1: Ayane Download/Cache Tests ===" << std::endl;

    test_manifest_selection();
    test_sha256_utility();
    test_sha256_file_verification();
    test_fixture_acquisition_success();
    test_sha256_mismatch();
    test_network_unsupported();
    test_manifest_incomplete();
    test_ayane_can_download();
    test_cache_scan_purge();
    test_fixture_not_found();
    test_to_string_family();
    test_error_factories();

    std::cout << "\n=== Summary: " << g_passed << " passed, " << g_failed
              << " failed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
