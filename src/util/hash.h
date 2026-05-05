// Reference: Traveler_Phase0_Spec_v0.1.md §9.4 (REQ-AYANE-4: SHA-256 post-download verification)
// Reference: FIPS PUB 180-4 "Secure Hash Standard"
// Pure C++20 SHA-256 implementation — no external crypto dependency.
// Public domain algorithm (FIPS 180-4). No license restrictions.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace traveler::util {

// ============================================================================
// sha256 / sha256_hex — SHA-256 digest computation
//
// Pure C++20 implementation of SHA-256 per FIPS 180-4.
// Single-binary invariant: no OpenSSL, no external crypto library.
//
// Overloads:
//   sha256(data, len)          — raw bytes via pointer+size
//   sha256_hex(data, len)      — raw bytes, returns hex string
//   sha256_hex(vec)            — vector<uint8_t> overload
//   sha256_hex_string(str)     — string_view convenience
// ============================================================================

// SHA-256 digest raw bytes (32 bytes / 256 bits)
using Sha256Digest = std::array<std::byte, 32>;

// Compute SHA-256 digest from raw bytes
[[nodiscard]] Sha256Digest sha256(const std::byte* data, std::size_t len) noexcept;

// Compute SHA-256 and return lowercase hex string (64 chars)
[[nodiscard]] std::string sha256_hex(const std::byte* data, std::size_t len) noexcept;

// Convenience: vector<uint8_t> overload
[[nodiscard]] inline std::string sha256_hex(const std::vector<uint8_t>& data) noexcept {
    return sha256_hex(
        reinterpret_cast<const std::byte*>(data.data()), data.size());
}

// Convenience: string_view overload
[[nodiscard]] inline std::string sha256_hex_string(std::string_view str) noexcept {
    return sha256_hex(
        reinterpret_cast<const std::byte*>(str.data()), str.size());
}

}  // namespace traveler::util
