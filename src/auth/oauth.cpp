// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/index.ts (generatePKCE, exchangeCodeForTokens, buildAuthorizeUrl)
// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/token.ts (refreshAccessToken)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-1, REQ-OAUTH-2)
#include "oauth.h"
#include "../util/hash.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace traveler::auth {

using json = nlohmann::json;

// ============================================================================
// PKCE helpers
// ============================================================================

// Characters for PKCE verifier (RFC 7636 unreserved charset)
static const char* PKCE_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";

static std::string generate_random_string(size_t length) {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, 65);  // 66 chars in PKCE_CHARS

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(PKCE_CHARS[dist(gen)]);
    }
    return result;
}

static std::string base64_url_encode(const std::array<std::byte, 32>& digest) {
    // Base64 URL-safe encoding (no padding)
    static const char* BASE64_CHARS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string result;
    result.reserve(44);  // ceil(32 * 8 / 6)

    // Process 3 bytes at a time
    const uint8_t* data = reinterpret_cast<const uint8_t*>(digest.data());
    for (size_t i = 0; i < 32; i += 3) {
        uint32_t val = (static_cast<uint32_t>(data[i]) << 16);
        if (i + 1 < 32) val |= (static_cast<uint32_t>(data[i + 1]) << 8);
        if (i + 2 < 32) val |= static_cast<uint32_t>(data[i + 2]);

        result.push_back(BASE64_CHARS[(val >> 18) & 0x3F]);
        result.push_back(BASE64_CHARS[(val >> 12) & 0x3F]);
        if (i + 1 < 32) result.push_back(BASE64_CHARS[(val >> 6) & 0x3F]);
        if (i + 2 < 32) result.push_back(BASE64_CHARS[val & 0x3F]);
    }

    return result;
}

tl::expected<PkceCodes, llm::Error> generate_pkce() {
    PkceCodes codes;
    codes.verifier = generate_random_string(43);

    // SHA-256 of verifier
    auto digest = traveler::util::sha256(
        reinterpret_cast<const std::byte*>(codes.verifier.data()),
        codes.verifier.size());

    codes.challenge = base64_url_encode(digest);
    return codes;
}

tl::expected<std::string, llm::Error> generate_state() {
    // Generate 32 random bytes → base64url encode
    static thread_local std::random_device rd;
    std::array<uint8_t, 32> bytes;
    for (auto& b : bytes) b = static_cast<uint8_t>(rd());

    // Convert to std::byte array for SHA utility
    std::array<std::byte, 32> digest;
    std::memcpy(digest.data(), bytes.data(), 32);

    return base64_url_encode(digest);
}

// ============================================================================
// Authorization URL builder
// ============================================================================

static std::string url_encode(std::string_view value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2)
                    << static_cast<int>(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }
    return escaped.str();
}

std::string build_authorization_url(std::string_view challenge,
                                     std::string_view state,
                                     std::string_view redirect_uri) {
    std::ostringstream oss;
    oss << OAUTH_AUTHORIZE_URL
        << "?response_type=code"
        << "&client_id=" << url_encode(OAUTH_CLIENT_ID)
        << "&redirect_uri=" << url_encode(redirect_uri)
        << "&scope=" << url_encode(OAUTH_SCOPES)
        << "&code_challenge=" << url_encode(challenge)
        << "&code_challenge_method=S256"
        << "&state=" << url_encode(state);
    return oss.str();
}

// ============================================================================
// Token exchange
// ============================================================================

tl::expected<TokenResponse, llm::Error>
exchange_code_for_tokens(std::string_view code,
                         std::string_view redirect_uri,
                         std::string_view code_verifier) {
    // Build form body: application/x-www-form-urlencoded
    std::ostringstream body;
    body << "grant_type=authorization_code"
         << "&code=" << url_encode(code)
         << "&redirect_uri=" << url_encode(redirect_uri)
         << "&client_id=" << url_encode(OAUTH_CLIENT_ID)
         << "&code_verifier=" << url_encode(code_verifier);

    std::string body_str = body.str();

    try {
        httplib::Client cli("https://claude.ai");
        cli.set_connection_timeout(30, 0);
        cli.set_read_timeout(30, 0);

        httplib::Headers headers = {
            {"Content-Type", "application/x-www-form-urlencoded"}
        };

        auto res = cli.Post("/v1/oauth/token", headers, body_str,
                            "application/x-www-form-urlencoded");

        if (!res) {
            return tl::make_unexpected(
                llm::Error::Network("Token exchange failed: no response"));
        }
        if (res->status != 200) {
            return tl::make_unexpected(
                llm::Error::Auth("Token exchange failed with status " +
                                 std::to_string(res->status) + ": " +
                                 res->body));
        }

        auto j = json::parse(res->body);
        TokenResponse tokens;
        tokens.access_token = j.value("access_token", "");
        tokens.refresh_token = j.value("refresh_token", "");
        tokens.expires_in = j.value("expires_in", 36000);
        return tokens;
    } catch (const std::exception& e) {
        return tl::make_unexpected(
            llm::Error::Network(std::string("Token exchange error: ") + e.what()));
    }
}

// ============================================================================
// Token refresh
// ============================================================================

tl::expected<TokenResponse, llm::Error>
refresh_access_token(std::string_view refresh_token) {
    std::ostringstream body;
    body << "grant_type=refresh_token"
         << "&client_id=" << url_encode(OAUTH_CLIENT_ID)
         << "&refresh_token=" << url_encode(refresh_token);

    std::string body_str = body.str();

    try {
        httplib::Client cli("https://claude.ai");
        cli.set_connection_timeout(30, 0);
        cli.set_read_timeout(30, 0);

        httplib::Headers headers = {
            {"Content-Type", "application/x-www-form-urlencoded"}
        };

        auto res = cli.Post("/v1/oauth/token", headers, body_str,
                            "application/x-www-form-urlencoded");

        if (!res) {
            return tl::make_unexpected(
                llm::Error::Network("Token refresh failed: no response"));
        }
        if (res->status != 200) {
            return tl::make_unexpected(
                llm::Error::Auth("Token refresh failed with status " +
                                 std::to_string(res->status)));
        }

        auto j = json::parse(res->body);
        TokenResponse tokens;
        tokens.access_token = j.value("access_token", "");
        tokens.refresh_token = j.value("refresh_token", "");
        tokens.expires_in = j.value("expires_in", 36000);
        return tokens;
    } catch (const std::exception& e) {
        return tl::make_unexpected(
            llm::Error::Network(std::string("Token refresh error: ") + e.what()));
    }
}

}  // namespace traveler::auth
