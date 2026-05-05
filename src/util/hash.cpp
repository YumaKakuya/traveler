// Reference: Traveler_Phase0_Spec_v0.1.md §9.4 (REQ-AYANE-4: SHA-256 post-download verification)
// Reference: FIPS PUB 180-4 "Secure Hash Standard"
// Pure C++20 SHA-256 implementation — no external crypto dependency.
// Public domain algorithm (FIPS 180-4). No license restrictions.
//
// This implementation follows FIPS 180-4 §6.2 (SHA-256) exactly.
// No SIMD, no platform intrinsics — portable C++20.
#include "hash.h"

#include <algorithm>
#include <cstring>

namespace traveler::util {
namespace {

// ============================================================================
// SHA-256 constants (FIPS 180-4 §4.2.2)
// ============================================================================
constexpr std::uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

// ============================================================================
// Rotate right (FIPS 180-4 §3.2)
// ============================================================================
constexpr std::uint32_t rotr(std::uint32_t x, unsigned n) noexcept {
    return (x >> n) | (x << (32 - n));
}

// ============================================================================
// SHA-256 functions (FIPS 180-4 §4.1.2)
// ============================================================================
constexpr std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
    return (x & y) ^ (~x & z);
}

constexpr std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
    return (x & y) ^ (x & z) ^ (y & z);
}

constexpr std::uint32_t sigma0(std::uint32_t x) noexcept {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

constexpr std::uint32_t sigma1(std::uint32_t x) noexcept {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

constexpr std::uint32_t omega0(std::uint32_t x) noexcept {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

constexpr std::uint32_t omega1(std::uint32_t x) noexcept {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

// ============================================================================
// SHA-256 internal state
// ============================================================================
struct Sha256State {
    std::uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    std::uint64_t count = 0;  // total bits processed
    std::uint8_t buf[64]{};
    std::size_t buf_len = 0;

    // Process a single 512-bit block (FIPS 180-4 §6.2.2)
    void compress(const std::uint8_t block[64]) noexcept {
        std::uint32_t W[64];

        // Prepare message schedule
        for (unsigned t = 0; t < 16; ++t) {
            W[t] = (static_cast<std::uint32_t>(block[t * 4])     << 24)
                 | (static_cast<std::uint32_t>(block[t * 4 + 1]) << 16)
                 | (static_cast<std::uint32_t>(block[t * 4 + 2]) << 8)
                 |  static_cast<std::uint32_t>(block[t * 4 + 3]);
        }
        for (unsigned t = 16; t < 64; ++t) {
            W[t] = omega1(W[t - 2]) + W[t - 7] + omega0(W[t - 15]) + W[t - 16];
        }

        // Working variables
        std::uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
        std::uint32_t e = H[4], f = H[5], g = H[6], h = H[7];

        // Compression loop
        for (unsigned t = 0; t < 64; ++t) {
            const std::uint32_t T1 = h + sigma1(e) + ch(e, f, g) + K[t] + W[t];
            const std::uint32_t T2 = sigma0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + T1;
            d = c;
            c = b;
            b = a;
            a = T1 + T2;
        }

        // Compute intermediate hash values
        H[0] += a; H[1] += b; H[2] += c; H[3] += d;
        H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }

    // Feed data into the state
    void update(const std::uint8_t* data, std::size_t len) noexcept {
        count += len * 8;

        // Process any buffered data first
        if (buf_len > 0 && buf_len + len >= 64) {
            const std::size_t copy = 64 - buf_len;
            std::memcpy(buf + buf_len, data, copy);
            compress(buf);
            data += copy;
            len -= copy;
            buf_len = 0;
        }

        // Process full 64-byte blocks
        while (len >= 64) {
            compress(data);
            data += 64;
            len -= 64;
        }

        // Buffer remaining data
        if (len > 0) {
            std::memcpy(buf + buf_len, data, len);
            buf_len += len;
        }
    }

    // Finalize: pad and produce digest (FIPS 180-4 §5.1.1)
    Sha256Digest finalize() noexcept {
        // Append 0x80
        buf[buf_len++] = 0x80;

        // If no room for the 64-bit length, pad with zeros and compress
        if (buf_len > 56) {
            std::memset(buf + buf_len, 0, 64 - buf_len);
            compress(buf);
            buf_len = 0;
        }

        // Pad with zeros up to byte 56
        std::memset(buf + buf_len, 0, 56 - buf_len);

        // Append length in bits as big-endian 64-bit
        const std::uint64_t bits = count;
        buf[56] = static_cast<std::uint8_t>(bits >> 56);
        buf[57] = static_cast<std::uint8_t>(bits >> 48);
        buf[58] = static_cast<std::uint8_t>(bits >> 40);
        buf[59] = static_cast<std::uint8_t>(bits >> 32);
        buf[60] = static_cast<std::uint8_t>(bits >> 24);
        buf[61] = static_cast<std::uint8_t>(bits >> 16);
        buf[62] = static_cast<std::uint8_t>(bits >> 8);
        buf[63] = static_cast<std::uint8_t>(bits);

        compress(buf);

        // Produce digest (big-endian)
        Sha256Digest digest{};
        for (unsigned i = 0; i < 8; ++i) {
            digest[i * 4]     = static_cast<std::byte>(H[i] >> 24);
            digest[i * 4 + 1] = static_cast<std::byte>(H[i] >> 16);
            digest[i * 4 + 2] = static_cast<std::byte>(H[i] >> 8);
            digest[i * 4 + 3] = static_cast<std::byte>(H[i]);
        }
        return digest;
    }
};

}  // namespace

// ============================================================================
// Public API
// ============================================================================

Sha256Digest sha256(const std::byte* data, std::size_t len) noexcept {
    Sha256State state;
    state.update(reinterpret_cast<const std::uint8_t*>(data), len);
    return state.finalize();
}

std::string sha256_hex(const std::byte* data, std::size_t len) noexcept {
    const auto digest = sha256(data, len);

    static constexpr char hex_chars[] = "0123456789abcdef";
    std::string result(64, '0');
    for (std::size_t i = 0; i < 32; ++i) {
        const auto b = static_cast<std::uint8_t>(digest[i]);
        result[i * 2]     = hex_chars[b >> 4];
        result[i * 2 + 1] = hex_chars[b & 0x0f];
    }
    return result;
}

}  // namespace traveler::util
