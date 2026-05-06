// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/fetch.ts (createClaudeSubFetch L83-112)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-6: custom HTTP wrapper)
//
// Verbatim port of claude-sub fetch wrapper:
//   - Body JSON injection (injectBillingAndIdentity)
//   - Header set for api.anthropic.com
//   - UUID-v4, session ID, merge betas
#include "fetch_wrapper.h"
#include "../util/hash.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <nlohmann/json.hpp>

namespace traveler::auth {

using json = nlohmann::json;

// ============================================================================
// UUID-v4 generation
// ============================================================================

std::string generate_uuid_v4() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    // Generate 16 random bytes
    std::array<uint8_t, 16> bytes;
    for (auto& b : bytes) b = static_cast<uint8_t>(dist(gen));

    // Set UUID version 4 bits
    bytes[6] = (bytes[6] & 0x0F) | 0x40;  // version 4
    bytes[8] = (bytes[8] & 0x3F) | 0x80;  // variant 1

    char buf[37];
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  bytes[0], bytes[1], bytes[2], bytes[3],
                  bytes[4], bytes[5], bytes[6], bytes[7],
                  bytes[8], bytes[9], bytes[10], bytes[11],
                  bytes[12], bytes[13], bytes[14], bytes[15]);
    return std::string(buf);
}

// ============================================================================
// Session ID — once per process invocation
// ============================================================================

const std::string& session_id() {
    static std::string sid = generate_uuid_v4();
    return sid;
}

// ============================================================================
// injectBillingAndIdentity — faithful port of fetch.ts L50-66
// ============================================================================

static std::string first_user_message_text(const json& messages) {
    if (!messages.is_array()) return "";
    for (const auto& msg : messages) {
        if (!msg.is_object()) continue;
        auto role = msg.value("role", "");
        if (role != "user") continue;

        auto content = msg.value("content", json());
        if (content.is_string()) return content.get<std::string>();
        if (content.is_array()) {
            for (const auto& block : content) {
                if (!block.is_object()) continue;
                if (block.value("type", "") == "text" &&
                    block.contains("text") && block["text"].is_string()) {
                    return block["text"].get<std::string>();
                }
            }
        }
    }
    return "";
}

// computeBillingHeader — faithful port of fetch.ts L34-41
static std::string compute_billing_header(const json& messages) {
    std::string text = first_user_message_text(messages);
    const char* cch = "00000";

    // pick(s, 4, 7, 20)
    auto pick = [](const std::string& s, int pos) -> char {
        return (pos < static_cast<int>(s.size())) ? s[pos] : '0';
    };
    std::string picked;
    picked += pick(text, 4);
    picked += pick(text, 7);
    picked += pick(text, 20);

    std::string salted = std::string("59cf53e54c78") + picked + CC_VERSION;
    auto hash = traveler::util::sha256_hex_string(salted);
    std::string version_suffix = hash.substr(0, 3);

    return "x-anthropic-billing-header: cc_version=" + std::string(CC_VERSION)
           + "." + version_suffix + "; cc_entrypoint=cli; cch=" + cch + ";";
}

// normalizeSystem — faithful port of fetch.ts L43-48
static json normalize_system(const json& system) {
    if (system.is_array()) return system;
    if (system.is_string()) {
        return json::array({json::object({{"type", "text"}, {"text", system}})});
    }
    if (system.is_object() && system.contains("type")) {
        return json::array({system});
    }
    return json::array();
}

tl::expected<std::string, Error>
inject_billing_and_identity(std::string_view json_body) {
    try {
        json body = json::parse(json_body);

        // Strip dated model suffix (e.g., claude-sonnet-4-20250514 → claude-sonnet-4)
        if (body.contains("model") && body["model"].is_string()) {
            std::string model = body["model"].get<std::string>();
            if (model.find("claude-") == 0) {
                // Remove 8-digit date suffix like -20250514
                auto dash8_pos = model.find_last_of('-');
                if (dash8_pos != std::string::npos) {
                    auto suffix = model.substr(dash8_pos + 1);
                    if (suffix.size() == 8 &&
                        std::all_of(suffix.begin(), suffix.end(), ::isdigit)) {
                        model = model.substr(0, dash8_pos);
                        body["model"] = model;
                    }
                }
            }
        }

        json messages = body.value("messages", json::array());
        json system = normalize_system(body.value("system", json()));

        // Filter out existing billing entries
        json filtered;
        for (const auto& entry : system) {
            if (!(entry.is_object() &&
                  entry.value("type", "") == "text" &&
                  entry.value("text", "").find("x-anthropic-billing-header:") == 0)) {
                filtered.push_back(entry);
            }
        }

        // Prepend billing entry
        json billing_entry = {
            {"type", "text"},
            {"text", compute_billing_header(messages)}
        };
        filtered.insert(filtered.begin(), billing_entry);

        body["system"] = filtered;
        return body.dump();
    } catch (const json::exception& e) {
        // Not valid JSON — pass through unchanged
        return std::string(json_body);
    }
}

// ============================================================================
// mergeBetas — faithful port of fetch.ts L68-77
// ============================================================================

std::string merge_betas(std::string_view existing_beta) {
    std::vector<std::string> betas;
    for (const auto* b : BASE_BETAS) {
        if (std::find(betas.begin(), betas.end(), b) == betas.end()) {
            betas.push_back(b);
        }
    }
    if (!existing_beta.empty()) {
        std::string existing(existing_beta);
        std::stringstream ss(existing);
        std::string token;
        while (std::getline(ss, token, ',')) {
            auto start = token.find_first_not_of(" \t");
            auto end = token.find_last_not_of(" \t");
            if (start != std::string::npos) {
                auto trimmed = token.substr(start, end - start + 1);
                if (std::find(betas.begin(), betas.end(), trimmed) == betas.end()) {
                    betas.push_back(trimmed);
                }
            }
        }
    }

    std::string result;
    for (size_t i = 0; i < betas.size(); ++i) {
        if (i > 0) result += ",";
        result += betas[i];
    }
    return result;
}

// ============================================================================
// prepare_claude_sub_request
// ============================================================================

tl::expected<HttpRequest, Error>
prepare_claude_sub_request(const HttpRequest& original,
                           std::string_view access_token) {
    HttpRequest modified = original;

    // Host check: only apply claude-sub headers for api.anthropic.com
    {
        auto host_start = modified.url.find("://");
        if (host_start != std::string::npos) {
            host_start += 3;
            auto host_end = modified.url.find('/', host_start);
            std::string host = modified.url.substr(host_start, host_end - host_start);
            if (host != "api.anthropic.com") {
                return modified;
            }
        }
    }

    // (1) Body modification: inject billing and identity
    if (!modified.body.empty()) {
        auto result = inject_billing_and_identity(modified.body);
        if (result) {
            modified.body = *result;
        }
        // On failure, pass through original body
    }

    // (2) Header set
    // Get existing anthropic-beta for merge
    std::string existing_beta;
    auto beta_it = modified.headers.find("anthropic-beta");
    if (beta_it != modified.headers.end()) {
        existing_beta = beta_it->second;
    }

    // Set required headers
    modified.headers["Authorization"] = std::string("Bearer ") + std::string(access_token);
    modified.headers["anthropic-version"] = "2023-06-01";
    modified.headers["anthropic-beta"] = merge_betas(existing_beta);
    modified.headers["x-app"] = "cli";
    modified.headers["user-agent"] = std::string("claude-cli/") + CC_VERSION;
    modified.headers["x-client-request-id"] = generate_uuid_v4();
    modified.headers["X-Claude-Code-Session-Id"] = session_id();

    // Delete x-api-key if present
    modified.headers.erase("x-api-key");

    return modified;
}

}  // namespace traveler::auth
