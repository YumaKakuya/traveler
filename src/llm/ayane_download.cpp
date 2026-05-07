// Reference: Traveler_Phase0_Spec_v0.1.md §9.4 (REQ-AYANE-1 through REQ-AYANE-6)
// Reference: briefs/wave-c-c1-offline-ayane-dispatch-2026-05-07.md
//
// Ayane download/cache module implementation.
// Network download is explicit unsupported-current-resource.
// Local fixture acquisition and SHA-256 verification are operational.
#include "ayane_download.h"

#include "../util/hash.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

namespace traveler::llm {

// ============================================================================
// Platform-appropriate cache directory
// ============================================================================

fs::path ayane_cache_dir() {
    const char* home = nullptr;

#if defined(_WIN32)
    // Windows: %LOCALAPPDATA%/traveler/models/
    home = std::getenv("LOCALAPPDATA");
    if (home != nullptr && home[0] != '\0') {
        return fs::path(home) / "traveler" / "models";
    }
    // Fallback to %APPDATA%
    home = std::getenv("APPDATA");
    if (home != nullptr && home[0] != '\0') {
        return fs::path(home) / "traveler" / "models";
    }
    // Last resort: %USERPROFILE%/.local/share/traveler/models/
    home = std::getenv("USERPROFILE");
    if (home != nullptr && home[0] != '\0') {
        return fs::path(home) / ".local" / "share" / "traveler" / "models";
    }
#elif defined(__APPLE__)
    // macOS: ~/Library/Application Support/traveler/models/
    home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return fs::path(home) / "Library" / "Application Support" / "traveler" / "models";
    }
#else
    // Linux / BSD: ~/.local/share/traveler/models/
    home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return fs::path(home) / ".local" / "share" / "traveler" / "models";
    }
#endif

    // Ultimate fallback
    return fs::path("models");
}

// ============================================================================
// SHA-256 file verification
// ============================================================================

std::optional<std::string> sha256_file(const fs::path& path) {
    std::error_code ec;

    if (!fs::exists(path, ec) || ec) return std::nullopt;
    if (!fs::is_regular_file(path, ec) || ec) return std::nullopt;

    // Read entire file into memory for hashing
    const auto file_size = fs::file_size(path, ec);
    if (ec || file_size == 0) return std::nullopt;

    std::vector<std::uint8_t> buffer;
    buffer.reserve(static_cast<std::size_t>(file_size));

    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;

    // Read file contents
    buffer.assign(std::istreambuf_iterator<char>(file),
                   std::istreambuf_iterator<char>());

    if (buffer.empty() && file_size > 0) return std::nullopt;

    // Compute SHA-256 using existing utility
    return util::sha256_hex(buffer);
}

// ============================================================================
// ayane_can_download — check manifest completeness
// ============================================================================

bool ayane_can_download(const assets::AyaneManifestEntry& entry) {
    return !entry.requires_manifest_completion() && entry.is_downloadable();
}

// ============================================================================
// ayane_network_download — explicit unsupported-current-resource
// ============================================================================

tl::expected<AyaneDownloadResult, AyaneDownloadError>
ayane_network_download(const assets::AyaneManifestEntry& /*entry*/,
                       AyaneProgressFn /*on_progress*/) {
    // Network download is not available until real URLs and SHA-256 values
    // are committed to src/assets/ayane_manifest.h at v0.1.0 release.
    // See briefs/wave-c-c1-offline-ayane-dispatch-2026-05-07.md:
    // "Network download may be explicit unsupported-current-resource."
    return tl::make_unexpected(AyaneDownloadError::NetworkUnsupported(
        "Network download is not available in this build. "
        "Ayane manifest requires completion with official Hugging Face URLs "
        "and SHA-256 values. Use local fixture acquisition instead."));
}

// ============================================================================
// ayane_acquire — deterministic local-file fixture acquisition
// ============================================================================

tl::expected<AyaneDownloadResult, AyaneDownloadError>
ayane_acquire(const assets::AyaneManifestEntry& entry,
              std::optional<fs::path> fixture_path,
              AyaneProgressFn on_progress) {

    // --- Step 1: Check cache for existing file ---
    const fs::path cache_dir = ayane_cache_dir();
    const fs::path cached_path = cache_dir / entry.gguf_filename;

    std::error_code ec;
    if (fs::exists(cached_path, ec) && !ec && fs::is_regular_file(cached_path, ec) && !ec) {
        // Verify SHA-256 of cached file
        auto sha = sha256_file(cached_path);
        if (!sha) {
            return tl::make_unexpected(AyaneDownloadError::FileReadError(
                "Could not read cached model file: " + cached_path.string()));
        }

        // If manifest has a SHA, verify it
        if (!entry.sha256_hex.empty() && *sha != entry.sha256_hex) {
            // Mismatch — delete cached file and fall through to re-acquire
            fs::remove(cached_path, ec);

            AyaneDownloadProgress progress{};
            if (on_progress) on_progress(progress);

            return tl::make_unexpected(AyaneDownloadError::Sha256Mismatch(
                "Cached model file SHA-256 mismatch for " + entry.gguf_filename,
                entry.sha256_hex, *sha));
        }

        // Cached file is valid
        AyaneDownloadResult result;
        result.model_path = fs::absolute(cached_path, ec);
        result.family = entry.family;
        result.sha256_verified = *sha;
        result.from_cache = true;
        result.from_fixture = false;
        return result;
    }

    // --- Step 2: Local fixture acquisition ---
    if (fixture_path.has_value()) {
        const auto& src = fixture_path.value();

        if (!fs::exists(src, ec) || ec) {
            return tl::make_unexpected(AyaneDownloadError::FileNotFound(
                "Local fixture file not found: " + src.string()));
        }

        if (!fs::is_regular_file(src, ec) || ec) {
            return tl::make_unexpected(AyaneDownloadError::FileNotFound(
                "Fixture path is not a regular file: " + src.string()));
        }

        // Create cache directory if needed
        fs::create_directories(cache_dir, ec);
        if (ec) {
            return tl::make_unexpected(AyaneDownloadError::CacheDirError(
                "Could not create cache directory: " + cache_dir.string()));
        }

        // Copy fixture to cache
        fs::copy_file(src, cached_path, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            return tl::make_unexpected(AyaneDownloadError::FileWriteError(
                "Could not copy fixture to cache: " + src.string() +
                " -> " + cached_path.string()));
        }

        // Verify SHA-256 of copied file
        auto sha = sha256_file(cached_path);
        if (!sha) {
            // Clean up partial copy
            fs::remove(cached_path, ec);
            return tl::make_unexpected(AyaneDownloadError::FileReadError(
                "Could not read copied fixture for verification: " + cached_path.string()));
        }

        // If manifest has expected SHA, verify
        if (!entry.sha256_hex.empty() && *sha != entry.sha256_hex) {
            fs::remove(cached_path, ec);
            return tl::make_unexpected(AyaneDownloadError::Sha256Mismatch(
                "Fixture SHA-256 mismatch for " + entry.gguf_filename +
                " (source: " + src.string() + ")",
                entry.sha256_hex, *sha));
        }

        // Report progress complete (not real download, but fixture copied)
        if (on_progress) {
            AyaneDownloadProgress progress;
            progress.total_bytes = 1;
            progress.bytes_downloaded = 1;
            on_progress(progress);
        }

        AyaneDownloadResult result;
        result.model_path = fs::absolute(cached_path, ec);
        result.family = entry.family;
        result.sha256_verified = *sha;
        result.from_cache = false;
        result.from_fixture = true;
        return result;
    }

    // --- Step 3: Network download request ---
    // Check if manifest is complete enough for download
    if (entry.requires_manifest_completion()) {
        return tl::make_unexpected(AyaneDownloadError::ManifestIncomplete(
            "Ayane manifest entry for " + entry.display_name +
            " requires completion (missing official download URL and/or SHA-256). "
            "Provide a local fixture path to acquire this model."));
    }

    if (!entry.is_downloadable()) {
        return tl::make_unexpected(AyaneDownloadError::ManifestIncomplete(
            "Ayane manifest entry for " + entry.display_name +
            " is not marked as downloadable."));
    }

    // Attempt network download — currently always unsupported
    return ayane_network_download(entry, on_progress);
}

// ============================================================================
// ayane_scan_cache — scan cache directory for available models
// ============================================================================

std::vector<AyaneDownloadResult> ayane_scan_cache() {
    std::vector<AyaneDownloadResult> results;
    const fs::path cache_dir = ayane_cache_dir();

    std::error_code ec;
    if (!fs::exists(cache_dir, ec) || ec) return results;
    if (!fs::is_directory(cache_dir, ec) || ec) return results;

    for (const auto& entry : assets::kAyaneProductionManifest) {
        const fs::path model_path = cache_dir / entry.gguf_filename;
        if (fs::exists(model_path, ec) && !ec && fs::is_regular_file(model_path, ec) && !ec) {
            auto sha = sha256_file(model_path);
            if (sha) {
                AyaneDownloadResult result;
                result.model_path = fs::absolute(model_path, ec);
                result.family = entry.family;
                result.sha256_verified = *sha;
                result.from_cache = true;
                result.from_fixture = false;
                results.push_back(std::move(result));
            }
        }
    }

    return results;
}

// ============================================================================
// ayane_first_available — find first model in cache
// ============================================================================

std::optional<AyaneDownloadResult> ayane_first_available() {
    auto cached = ayane_scan_cache();
    if (!cached.empty()) {
        return cached.front();
    }
    return std::nullopt;
}

// ============================================================================
// ayane_purge_cache — remove all cached model files
// ============================================================================

tl::expected<std::size_t, AyaneDownloadError>
ayane_purge_cache() {
    const fs::path cache_dir = ayane_cache_dir();
    std::error_code ec;
    std::size_t removed = 0;

    if (!fs::exists(cache_dir, ec) || ec) return removed;  // nothing to purge

    for (const auto& entry : assets::kAyaneProductionManifest) {
        const fs::path model_path = cache_dir / entry.gguf_filename;
        if (fs::exists(model_path, ec) && !ec) {
            if (fs::remove(model_path, ec); !ec) {
                ++removed;
            }
        }
    }

    return removed;
}

}  // namespace traveler::llm
