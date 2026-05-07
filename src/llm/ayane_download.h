// Reference: Traveler_Phase0_Spec_v0.1.md §9.4 (REQ-AYANE-1 through REQ-AYANE-6)
// Reference: briefs/wave-c-c1-offline-ayane-dispatch-2026-05-07.md
//
// Ayane download/cache module.
// Supports deterministic local-file fixture acquisition and SHA-256 verification.
// Network download is explicit unsupported-current-resource until real URLs/SHA
// are committed to ayane_manifest.h at v0.1.0 release.
#pragma once

#include "../assets/ayane_manifest.h"

#include <cstdint>
#include <functional>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <tl/expected.hpp>

namespace traveler::llm {

namespace fs = std::filesystem;

// ============================================================================
// AyaneDownloadError — error type for all download/cache operations
// ============================================================================
struct AyaneDownloadError {
    enum class Code {
        NetworkUnsupported,      // Network download not available (current-resource)
        FileNotFound,            // Local fixture file not found
        FileReadError,           // Could not read local file
        Sha256Mismatch,          // SHA-256 verification failed
        ManifestIncomplete,      // Manifest requires completion (no URL/SHA)
        CacheDirError,           // Could not create/access cache directory
        FileWriteError,          // Could not write to cache
        InvalidArgument,         // Invalid parameters
    };

    Code code;
    std::string message;
    std::optional<std::string> expected_sha256;
    std::optional<std::string> actual_sha256;

    [[nodiscard]] static AyaneDownloadError NetworkUnsupported(std::string msg) {
        return {Code::NetworkUnsupported, std::move(msg), std::nullopt, std::nullopt};
    }
    [[nodiscard]] static AyaneDownloadError FileNotFound(std::string msg) {
        return {Code::FileNotFound, std::move(msg), std::nullopt, std::nullopt};
    }
    [[nodiscard]] static AyaneDownloadError FileReadError(std::string msg) {
        return {Code::FileReadError, std::move(msg), std::nullopt, std::nullopt};
    }
    [[nodiscard]] static AyaneDownloadError Sha256Mismatch(
            std::string msg, std::string expected, std::string actual) {
        return {Code::Sha256Mismatch, std::move(msg),
                std::move(expected), std::move(actual)};
    }
    [[nodiscard]] static AyaneDownloadError ManifestIncomplete(std::string msg) {
        return {Code::ManifestIncomplete, std::move(msg), std::nullopt, std::nullopt};
    }
    [[nodiscard]] static AyaneDownloadError CacheDirError(std::string msg) {
        return {Code::CacheDirError, std::move(msg), std::nullopt, std::nullopt};
    }
    [[nodiscard]] static AyaneDownloadError FileWriteError(std::string msg) {
        return {Code::FileWriteError, std::move(msg), std::nullopt, std::nullopt};
    }
    [[nodiscard]] static AyaneDownloadError InvalidArgument(std::string msg) {
        return {Code::InvalidArgument, std::move(msg), std::nullopt, std::nullopt};
    }
};

// ============================================================================
// AyaneDownloadResult — holds path to acquired GGUF file
// ============================================================================
struct AyaneDownloadResult {
    fs::path model_path;        // Absolute path to the GGUF file
    assets::AyaneModelFamily family;
    std::string sha256_verified; // Verified SHA-256 hex string
    bool from_cache{false};      // true if model was already in cache
    bool from_fixture{false};    // true if acquired via local fixture (not download)
};

// ============================================================================
// Progress callback — used during download for percentage + ETA
// ============================================================================
struct AyaneDownloadProgress {
    std::uint64_t bytes_downloaded{0};
    std::uint64_t total_bytes{0};
    bool cancelled{false}; // set to true to request cancellation
};

using AyaneProgressFn = std::function<void(const AyaneDownloadProgress&)>;

// ============================================================================
// Public API
// ============================================================================

// Get the platform-appropriate model cache directory.
// Linux:   ~/.local/share/traveler/models/
// macOS:   ~/Library/Application Support/traveler/models/
// Windows: %LOCALAPPDATA%/traveler/models/
[[nodiscard]] fs::path ayane_cache_dir();

// Compute SHA-256 hex of a file on disk. Returns empty optional on I/O error.
[[nodiscard]] std::optional<std::string> sha256_file(const fs::path& path);

// Acquire a model GGUF file deterministically.
//
// Priority:
//   1. Check cache directory for existing GGUF file matching the manifest.
//   2. If a local fixture path is provided (via `fixture_path`), copy from there.
//   3. If download is requested and the manifest has a complete URL/SHA,
//      attempt network download (currently unsupported — returns NetworkUnsupported).
//
// On success, returns AyaneDownloadResult with verified SHA-256.
// On mismatch, deletes the downloaded file and returns Sha256Mismatch error.
[[nodiscard]] tl::expected<AyaneDownloadResult, AyaneDownloadError>
ayane_acquire(const assets::AyaneManifestEntry& entry,
              std::optional<fs::path> fixture_path = std::nullopt,
              AyaneProgressFn on_progress = nullptr);

// Scan the cache directory for available models.
// Returns list of (family, path) for each GGUF file found that matches a known manifest.
[[nodiscard]] std::vector<AyaneDownloadResult>
ayane_scan_cache();

// Find the first available model in the cache. Returns nullopt if none found.
[[nodiscard]] std::optional<AyaneDownloadResult>
ayane_first_available();

// Explicit network download attempt. Currently returns NetworkUnsupported
// because real URLs/SHA are not committed in ayane_manifest.h (current-resource gap).
// This function exists as a placeholder to be filled when manifest is completed.
[[nodiscard]] tl::expected<AyaneDownloadResult, AyaneDownloadError>
ayane_network_download(const assets::AyaneManifestEntry& entry,
                       AyaneProgressFn on_progress = nullptr);

// Check whether an entry is ready for download (manifest has URL and SHA).
[[nodiscard]] bool ayane_can_download(const assets::AyaneManifestEntry& entry);

// Purge all cached model files from the cache directory.
// Returns count of files removed, or error.
[[nodiscard]] tl::expected<std::size_t, AyaneDownloadError>
ayane_purge_cache();

}  // namespace traveler::llm
