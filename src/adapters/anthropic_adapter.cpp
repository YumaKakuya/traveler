// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-1, REQ-ADAPTERS-2, REQ-ADAPTERS-5)
// Reference: ~/hatch-v3/packages/opencode/src/provider/anthropic.ts
// Port: Anthropic Messages API adapter — maps canonical Message ↔ Anthropic format.
// Anthropic Messages API docs: https://docs.anthropic.com/en/api/messages
//
// Key format differences:
//  - System prompt → top-level "system" field (NOT in messages array)
//  - Response text → content[].text
//  - SSE events: content_block_start, content_block_delta, message_stop, ping
//  - Auth: x-api-key header
#include "anthropic_adapter.h"

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>

namespace traveler::adapters {

// ============================================================================
// Internal helpers (anonymous namespace) — Phase 0 minimal JSON + HTTP
// ============================================================================
namespace {

// --- JSON string builder helpers ---

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// --- JSON string extraction helpers (simple scan, no full parser) ---

// Extract a JSON string value for a given key within a sub-object
// identified by prefix. Example: json_extract_nested(json, "delta", "text")
std::string json_extract_nested(const std::string& src,
                                 const std::string& outer_key,
                                 const std::string& inner_key) {
    std::string search = "\"" + outer_key + "\"";
    auto pos = src.find(search);
    if (pos == std::string::npos) return {};

    // Find the inner key within ~256 chars from outer key position
    auto sub = src.substr(pos, std::min(src.size() - pos, size_t{512}));
    search = "\"" + inner_key + "\"";
    pos = sub.find(search);
    if (pos == std::string::npos) return {};

    pos += search.size();
    // Scan past ':' and optional whitespace to opening quote
    while (pos < sub.size() && sub[pos] != '"') {
        if (sub[pos] == ':') { ++pos; break; }
        ++pos;
    }
    while (pos < sub.size() && sub[pos] != '"') ++pos;
    if (pos >= sub.size()) return {};
    ++pos;  // skip opening quote

    std::string result;
    while (pos < sub.size()) {
        if (sub[pos] == '\\') { pos += 2; continue; }
        if (sub[pos] == '"') break;
        result += sub[pos++];
    }
    return result;
}

// Extract a top-level JSON string value for a given key.
std::string json_extract_str(const std::string& src, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = src.find(search);
    if (pos == std::string::npos) return {};

    pos += search.size();
    // Scan to opening quote after ':'
    while (pos < src.size() && src[pos] != '"') {
        if (src[pos] == ':') { ++pos; break; }
        ++pos;
    }
    while (pos < src.size() && src[pos] != '"') ++pos;
    if (pos >= src.size()) return {};
    ++pos;  // skip opening quote

    std::string result;
    while (pos < src.size()) {
        if (src[pos] == '\\') { pos += 2; continue; }
        if (src[pos] == '"') break;
        result += src[pos++];
    }
    return result;
}

// Collect all text values from "text":"..." occurrences (filtering
// out short type-marker values like "text", "json", "text_delta").
std::string json_collect_text(const std::string& src) {
    std::string result;
    size_t pos = 0;
    std::string marker = "\"text\":\"";
    while ((pos = src.find(marker, pos)) != std::string::npos) {
        pos += marker.size();
        std::string val;
        while (pos < src.size()) {
            if (src[pos] == '\\') { pos += 2; continue; }
            if (src[pos] == '"') break;
            val += src[pos++];
        }
        ++pos;  // skip closing quote
        if (val.size() <= 11 && (val == "text" || val == "json" ||
            val == "text_delta" || val == "message")) {
            continue;  // structural type marker, not real content
        }
        if (!val.empty()) {
            if (!result.empty()) result += '\n';
            result += val;
        }
    }
    return result;
}

// --- Minimal TCP HTTP/1.1 POST (no SSL, Phase 0 infrastructure) ---
// Note: Production use requires OpenSSL or mbedtls (Phase 1+).

struct TcpHttpResponse {
    int status_code{0};
    std::string body;
};

tl::expected<TcpHttpResponse, llm::Error>
tcp_http_post(const std::string& host,
               const std::string& path,
               const std::string& json_body,
               const std::unordered_map<std::string, std::string>& headers) {
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    int s = getaddrinfo(host.c_str(), "443", &hints, &result);
    if (s != 0) {
        return tl::make_unexpected(llm::Error::Network(
            "DNS resolution failed: " + host));
    }

    // create socket
    int sock = ::socket(result->ai_family, result->ai_socktype,
                        result->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(result);
        return tl::make_unexpected(llm::Error::Network("socket() failed"));
    }

    // connect
    int conn_err = ::connect(sock, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    if (conn_err < 0) {
        ::close(sock);
        return tl::make_unexpected(llm::Error::Network(
            "connect() failed to " + host + ":443"));
    }

    // ---------- build HTTP request ----------
    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n";
    req << "Host: " << host << "\r\n";
    for (const auto& [k, v] : headers) {
        req << k << ": " << v << "\r\n";
    }
    req << "Content-Length: " << json_body.size() << "\r\n";
    req << "\r\n";
    req << json_body;

    std::string request = req.str();

    // send
    if (::send(sock, request.data(), request.size(), 0) < 0) {
        ::close(sock);
        return tl::make_unexpected(llm::Error::Network("send() failed"));
    }

    // ---------- read response ----------
    std::string raw;
    std::array<char, 4096> buf{};
    ssize_t n;
    while ((n = ::recv(sock, buf.data(), buf.size(), 0)) > 0) {
        raw.append(buf.data(), static_cast<size_t>(n));
        if (raw.size() > 10 * 1024 * 1024) break;  // 10 MB safety limit
    }
    ::close(sock);

    if (raw.empty()) {
        return tl::make_unexpected(llm::Error::Network("Empty response from " + host));
    }

    // ---------- parse HTTP status line ----------
    size_t eol = raw.find("\r\n");
    if (eol == std::string::npos) {
        return tl::make_unexpected(llm::Error::Network("Invalid HTTP response"));
    }

    // "HTTP/1.1 200 OK" → status_code starts at offset 9
    int status_code = 0;
    if (eol > 12) {
        try {
            status_code = std::stoi(raw.substr(9, 3));
        } catch (...) {
            status_code = 0;
        }
    }

    // ---------- extract body (after \r\n\r\n) ----------
    size_t header_end = raw.find("\r\n\r\n");
    std::string body;
    if (header_end != std::string::npos) {
        body = raw.substr(header_end + 4);
    }

    return TcpHttpResponse{status_code, std::move(body)};
}

}  // anonymous namespace

// ============================================================================
// AnthropicAdapter — Provider interface
// ============================================================================

AnthropicAdapter::AnthropicAdapter(std::string api_key)
    : api_key_(std::move(api_key)) {}

std::string AnthropicAdapter::id() const { return "anthropic"; }

bool AnthropicAdapter::supports_streaming() const { return true; }

// ---------------------------------------------------------------------------
// to_request_json — convert GenerateOptions → Anthropic Messages API JSON
//
// Anthropic format:
//   {"model":"claude-3-5-sonnet-20240620",
//    "max_tokens":1024,
//    "system":"system prompt here",
//    "messages":[{"role":"user","content":"Hello"}],
//    "stream":true}
// ---------------------------------------------------------------------------
std::string AnthropicAdapter::to_request_json(const llm::GenerateOptions& opts) {
    std::ostringstream js;

    js << "{\"model\":\"" << json_escape(opts.model) << "\"";

    // max_tokens
    if (opts.max_tokens.has_value()) {
        js << ",\"max_tokens\":" << opts.max_tokens.value();
    } else {
        js << ",\"max_tokens\":1024";  // safe default
    }

    // temperature (optional — Anthropic uses it differently but supports it)
    if (opts.temperature.has_value()) {
        js << ",\"temperature\":" << opts.temperature.value();
    }

    // --- Separate system messages → top-level "system" field ---
    std::string system_prompt;
    std::vector<llm::Message> non_system;

    for (const auto& msg : opts.messages) {
        if (msg.role == "system") {
            if (!system_prompt.empty()) system_prompt += "\n";
            system_prompt += msg.content;
        } else {
            non_system.push_back(msg);
        }
    }

    if (!system_prompt.empty()) {
        js << ",\"system\":\"" << json_escape(system_prompt) << "\"";
    }

    // --- messages array ---
    js << ",\"messages\":[";
    for (size_t i = 0; i < non_system.size(); ++i) {
        if (i > 0) js << ",";
        js << "{\"role\":\"" << json_escape(non_system[i].role) << "\""
           << ",\"content\":\"" << json_escape(non_system[i].content) << "\"}";
    }
    js << "]";

    // streaming
    js << ",\"stream\":true";

    js << "}";
    return js.str();
}

// ---------------------------------------------------------------------------
// from_response_json — parse complete (non-streaming) response
// ---------------------------------------------------------------------------
tl::expected<std::string, llm::Error>
AnthropicAdapter::from_response_json(std::string_view json) {
    std::string src(json);
    std::string text = json_collect_text(src);
    if (text.empty()) {
        // Check for explicit error in response
        auto err_type = json_extract_str(src, "type");
        if (err_type == "error") {
            auto err_msg = json_extract_nested(src, "error", "message");
            if (err_msg.empty()) err_msg = "Unknown Anthropic API error";
            return tl::make_unexpected(llm::Error::Provider(err_msg));
        }
    }
    return text;
}

// ---------------------------------------------------------------------------
// from_sse_line — parse single SSE data line → GenerateChunk
//
// Anthropic SSE event types:
//   content_block_start → first token (text appears in content_block.text)
//   content_block_delta  → incremental token (text appears in delta.text)
//   message_stop         → stream finished
//   ping                 → keep-alive, no content
// ---------------------------------------------------------------------------
tl::expected<llm::GenerateChunk, llm::Error>
AnthropicAdapter::from_sse_line(std::string_view sse_data) {
    std::string src(sse_data);
    if (src.empty()) return llm::GenerateChunk{};

    auto event_type = json_extract_str(src, "type");

    // --- message_stop: stream done ---
    if (event_type == "message_stop") {
        return llm::GenerateChunk{"", true};
    }

    // --- ping: keep-alive, no content ---
    if (event_type == "ping") {
        return llm::GenerateChunk{};
    }

    // --- content_block_start: first token ---
    if (event_type == "content_block_start") {
        auto text = json_extract_nested(src, "content_block", "text");
        return llm::GenerateChunk{text, false};
    }

    // --- content_block_delta: incremental token ---
    if (event_type == "content_block_delta") {
        auto text = json_extract_nested(src, "delta", "text");
        return llm::GenerateChunk{text, false};
    }

    // --- error event ---
    if (event_type == "error") {
        auto err_msg = json_extract_nested(src, "error", "message");
        if (err_msg.empty()) err_msg = "Unknown SSE error";
        llm::GenerateChunk chunk;
        chunk.done = true;
        chunk.error_message = err_msg;
        return chunk;
    }

    // Fallback: treat any "text" field in unknown events as a delta
    auto text = json_extract_str(src, "text");
    if (!text.empty() && text != "text" && text != "text_delta" &&
        text != "message") {
        return llm::GenerateChunk{text, false};
    }

    return llm::GenerateChunk{};
}

// ---------------------------------------------------------------------------
// generate — HTTP POST to Anthropic Messages API with SSE streaming
//
// Endpoint:  POST https://api.anthropic.com/v1/messages
// Headers:   x-api-key, anthropic-version: 2023-06-01, Content-Type: application/json
// Backoff:   Exponential on HTTP 429, max 3 retries (REQ-ADAPTERS-5)
// ---------------------------------------------------------------------------
tl::expected<void, llm::Error>
AnthropicAdapter::generate(const llm::GenerateOptions& opts,
                            std::function<void(llm::GenerateChunk)> on_chunk) {
    std::string request_json = to_request_json(opts);

    const std::string k_host = "api.anthropic.com";
    const std::string k_path = "/v1/messages";
    const int k_max_retries = 3;
    int delay_seconds = 1;

    for (int attempt = 0; attempt <= k_max_retries; ++attempt) {
        auto resp = tcp_http_post(k_host, k_path, request_json, {
            {"Content-Type", "application/json"},
            {"x-api-key", api_key_},
            {"anthropic-version", "2023-06-01"}
        });

        if (!resp) {
            return tl::make_unexpected(resp.error());
        }

        // --- 200 OK: parse SSE stream ---
        if (resp->status_code == 200) {
            const std::string& body = resp->body;
            size_t pos = 0;

            while (pos < body.size()) {
                // Seek to next "data: " prefix
                auto data_start = body.find("data: ", pos);
                if (data_start == std::string::npos) break;

                data_start += 6;  // skip "data: "
                auto data_end = body.find('\n', data_start);
                std::string data_line;
                if (data_end != std::string::npos) {
                    data_line = body.substr(data_start,
                        data_end - data_start);
                    pos = data_end + 1;
                } else {
                    data_line = body.substr(data_start);
                    pos = body.size();
                }

                // Trim trailing \r
                if (!data_line.empty() && data_line.back() == '\r') {
                    data_line.pop_back();
                }

                if (data_line.empty()) continue;

                auto chunk = from_sse_line(data_line);
                if (!chunk) {
                    llm::GenerateChunk err_chunk;
                    err_chunk.done = true;
                    err_chunk.error_message =
                        "SSE parse error: " + chunk.error().message;
                    on_chunk(err_chunk);
                    return {};
                }

                on_chunk(*chunk);
                if (chunk->done) return {};
            }

            // Normal stream completion
            return {};
        }

        // --- 429: rate limited — exponential backoff ---
        if (resp->status_code == 429) {
            if (attempt >= k_max_retries) {
                return tl::make_unexpected(llm::Error::Provider(
                    "Rate limit exceeded after " +
                    std::to_string(k_max_retries + 1) + " attempts"));
            }
            std::this_thread::sleep_for(std::chrono::seconds(delay_seconds));
            delay_seconds *= 2;
            continue;
        }

        // --- 401 / 403: authentication failure ---
        if (resp->status_code == 401 || resp->status_code == 403) {
            return tl::make_unexpected(llm::Error::Auth(
                "Authentication failed (HTTP " +
                std::to_string(resp->status_code) + ")"));
        }

        // --- Other errors ---
        return tl::make_unexpected(llm::Error::Provider(
            "API error (HTTP " + std::to_string(resp->status_code) + ")"));
    }

    return tl::make_unexpected(llm::Error::Provider(
        "Unexpected retry loop exit"));
}

}  // namespace traveler::adapters
