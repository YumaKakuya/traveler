// Reference: Traveler_Phase0_Spec_v0.1.md §9.4 (REQ-AYANE-2: "Ayane" first-run download)
// Reference: Traveler_Phase0_Spec_v0.1.md CD-1: phi-3.5-mini AND llama-3.2-1b-class both ship
// Reference: briefs/wave-c-c1-offline-ayane-dispatch-2026-05-07.md
//
// Ayane manifest types for the two adopted model families.
// Production entries explicitly require manifest completion (no fabricated URLs/SHA-256).
// Test fixture entries are separate and may carry known-good values for testing.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace traveler::assets {

// ============================================================================
// AyaneModelFamily — adopted model families per CD-1
// ============================================================================
enum class AyaneModelFamily {
    Phi35Mini,      // Microsoft phi-3.5-mini-instruct
    Llama32_1b,     // Meta Llama-3.2-1B-Instruct
};

[[nodiscard]] constexpr const char* to_string(AyaneModelFamily f) noexcept {
    switch (f) {
        case AyaneModelFamily::Phi35Mini:  return "phi-3.5-mini";
        case AyaneModelFamily::Llama32_1b: return "llama-3.2-1b";
    }
    return "unknown";
}

[[nodiscard]] constexpr const char* to_display_name(AyaneModelFamily f) noexcept {
    switch (f) {
        case AyaneModelFamily::Phi35Mini:  return "Microsoft phi-3.5-mini-instruct";
        case AyaneModelFamily::Llama32_1b: return "Meta Llama-3.2-1B-Instruct";
    }
    return "unknown";
}

// ============================================================================
// AyaneManifestEntry — descriptor for one model family variant
// ============================================================================
struct AyaneManifestEntry {
    AyaneModelFamily family;
    bool is_production{false};  // true = shipped production entry; false = test fixture

    // Human-readable display
    std::string display_name;
    std::string description;
    std::string gguf_filename;

    // Approximate download size (bytes)
    std::uint64_t approx_size_bytes{0};

    // --- Production entries (is_production=true) ---
    // When is_production=true and download_url / sha256_hex are empty,
    // the entry requires manifest completion — real values must be filled
    // at release time with official URLs and SHA-256 from Hugging Face.
    std::string download_url;   // empty => requires manifest completion
    std::string sha256_hex;     // empty => requires manifest completion

    // --- Test fixture entries (is_production=false) ---
    // May carry known-good URLs/SHA for local testing; these are NOT
    // used in production builds.

    // Quick check helpers
    [[nodiscard]] bool requires_manifest_completion() const noexcept {
        return is_production && (download_url.empty() || sha256_hex.empty());
    }

    [[nodiscard]] bool is_downloadable() const noexcept {
        return !is_production || (!download_url.empty() && !sha256_hex.empty());
    }
};

// ============================================================================
// Standard manifest — two adopted model families
// ============================================================================

inline constexpr std::size_t kAyaneProductionEntryCount = 2;
inline constexpr std::size_t kAyaneTestFixtureCount = 2;

// Production entries — require manifest completion (no real URLs/SHA yet).
// These are placeholder entries that will be completed at v0.1.0 release
// with official Hugging Face URLs and SHA-256 values.
// NOTE: not constexpr because AyaneManifestEntry contains std::string (non-literal).
inline const std::array<AyaneManifestEntry, kAyaneProductionEntryCount> kAyaneProductionManifest = {{
    {
        .family = AyaneModelFamily::Phi35Mini,
        .is_production = true,
        .display_name = "Microsoft phi-3.5-mini-instruct (Q4_K_M)",
        .description = "~3.8B params, Q4_K_M quantisation, ~2.2 GB download, "
                       "balanced quality/speed for Core-i3-class hardware",
        .gguf_filename = "phi-3.5-mini-instruct-Q4_K_M.gguf",
        .approx_size_bytes = 2'200'000'000ULL,
        .download_url = "",   // REQUIRES MANIFEST COMPLETION — TBD at release
        .sha256_hex = "",     // REQUIRES MANIFEST COMPLETION — TBD at release
    },
    {
        .family = AyaneModelFamily::Llama32_1b,
        .is_production = true,
        .display_name = "Meta Llama-3.2-1B-Instruct (Q4_K_M)",
        .description = "~1B params, Q4_K_M quantisation, ~0.7 GB download, "
                       "fast inference with smaller memory footprint",
        .gguf_filename = "llama-3.2-1b-instruct-Q4_K_M.gguf",
        .approx_size_bytes = 700'000'000ULL,
        .download_url = "",   // REQUIRES MANIFEST COMPLETION — TBD at release
        .sha256_hex = "",     // REQUIRES MANIFEST COMPLETION — TBD at release
    },
}};

// Test fixture entries — separate from production, carry fake-but-valid URLs
// so Worker W1 can write unit tests without real model downloads.
// NOTE: not constexpr because AyaneManifestEntry contains std::string (non-literal).
inline const std::array<AyaneManifestEntry, kAyaneTestFixtureCount> kAyaneTestFixtureManifest = {{
    {
        .family = AyaneModelFamily::Phi35Mini,
        .is_production = false,
        .display_name = "test-fixture phi-3.5-mini",
        .description = "Test fixture only — not for production use",
        .gguf_filename = "test-fixture-phi35mini.gguf",
        .approx_size_bytes = 1024,
        .download_url = "file:///test/fixtures/models/phi35mini.gguf",
        .sha256_hex = "0000000000000000000000000000000000000000000000000000000000000000",
    },
    {
        .family = AyaneModelFamily::Llama32_1b,
        .is_production = false,
        .display_name = "test-fixture llama-3.2-1b",
        .description = "Test fixture only — not for production use",
        .gguf_filename = "test-fixture-llama32-1b.gguf",
        .approx_size_bytes = 1024,
        .download_url = "file:///test/fixtures/models/llama32-1b.gguf",
        .sha256_hex = "1111111111111111111111111111111111111111111111111111111111111111",
    },
}};

// ============================================================================
// Manifest lookup helpers
// ============================================================================

// Find a production manifest entry by family. Returns nullptr if not found.
[[nodiscard]] inline const AyaneManifestEntry* find_production_manifest(AyaneModelFamily family) noexcept {
    for (const auto& entry : kAyaneProductionManifest) {
        if (entry.family == family) return &entry;
    }
    return nullptr;
}

// Find a test fixture entry by family. Returns nullptr if not found.
[[nodiscard]] inline const AyaneManifestEntry* find_test_fixture(AyaneModelFamily family) noexcept {
    for (const auto& entry : kAyaneTestFixtureManifest) {
        if (entry.family == family) return &entry;
    }
    return nullptr;
}

// Parse family from callsign-friendly name string.
[[nodiscard]] inline std::optional<AyaneModelFamily> parse_family(std::string_view name) noexcept {
    if (name == "phi-3.5-mini" || name == "phi35mini" || name == "phi") {
        return AyaneModelFamily::Phi35Mini;
    }
    if (name == "llama-3.2-1b" || name == "llama32-1b" || name == "llama") {
        return AyaneModelFamily::Llama32_1b;
    }
    return std::nullopt;
}

}  // namespace traveler::assets
