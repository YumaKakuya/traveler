// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-1, REQ-ADAPTERS-2, REQ-ADAPTERS-5)
// Reference: ~/hatch-v3/packages/opencode/src/provider/openai.ts
// Port: OpenAI Chat Completions adapter — maps canonical Message ↔ OpenAI format.
// OpenAI Chat Completions docs: https://platform.openai.com/docs/api-reference/chat
//
// Key format differences:
//  - All messages (incl. system) go in the messages array
//  - Streaming uses SSE with text/event-stream, [DONE] sentinel
//  - SSE delta: choices[].delta.content
//  - Auth: Authorization: Bearer <api_key>
#include "openai_adapter.h"

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
#include <unordered_map>

namespace traveler::adapters {

// ============================================================================
// Internal helpers (anonymous namespace) — minimal JSON + HTTP
// ============================================================================
namespace {

// --- JSON string builder ---

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

// Extract a JSON string value for a given key.
std::string json_extract_str(const std::string& src, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = src.find(search);
    if (pos == std::string::npos) return {};

    pos += search.size();
    while (pos < src.size() && src[pos] != '"') {
        if (src[pos] == ':') { ++pos; break; }
        ++pos;
    }
    while (pos < src.size() && src[pos] != '"') ++pos;
    if (pos >= src.size()) return {};
    ++pos;

    std::string result;
    while (pos < src.size()) {
        if (src[pos] == '\\') { pos += 2; continue; }
        if (src[pos] == '"') break;
        result += src[pos++];
    }
    return result;
}

// Extract a nested JSON string value within a parent key's object.
std::string json_extract_nested(const std::string& src,
                                 const std::string& outer_key,
                                 const std::string& inner_key) {
    std::string search = "\"" + outer_key + "\"";
    auto pos = src.find(search);
    if (pos == std::string::npos) return {};

    auto sub = src.substr(pos, std::min(src.size() - pos, size_t{512}));
    return json_extract_str(sub, inner_key);
}

// --- Minimal TCP HTTP/1.1 POST ---

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

    int sock = ::socket(result->ai_family, result->ai_socktype,
                        result->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(result);
        return tl::make_unexpected(llm::Error::Network("socket() failed"));
    }

    int conn_err = ::connect(sock, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    if (conn_err < 0) {
        ::close(sock);
        return tl::make_unexpected(llm::Error::Network(
            "connect() failed to " + host + ":443"));
    }

    // Build HTTP request
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

    if (::send(sock, request.data(), request.size(), 0) < 0) {
        ::close(sock);
        return tl::make_unexpected(llm::Error::Network("send() failed"));
    }

    std::string raw;
    std::array<char, 4096> buf{};
    ssize_t n;
    while ((n = ::recv(sock, buf.data(), buf.size(), 0)) > 0) {
        raw.append(buf.data(), static_cast<size_t>(n));
        if (raw.size() > 10 * 1024 * 1024) break;
    }
    ::close(sock);

    if (raw.empty()) {
        return tl::make_unexpected(llm::Error::Network("Empty response from " + host));
    }

    size_t eol = raw.find("\r\n");
    if (eol == std::string::npos) {
        return tl::make_unexpected(llm::Error::Network("Invalid HTTP response"));
    }

    int status_code = 0;
    if (eol > 12) {
        try {
            status_code = std::stoi(raw.substr(9, 3));
        } catch (...) {
            status_code = 0;
        }
    }

    size_t header_end = raw.find("\r\n\r\n");
    std::string body;
    if (header_end != std::string::npos) {
        body = raw.substr(header_end + 4);
    }

    return TcpHttpResponse{status_code, std::move(body)};
}

}  // anonymous namespace

// ============================================================================
// OpenAIAdapter — Provider interface
// ============================================================================

OpenAIAdapter::OpenAIAdapter(std::string api_key)
    : api_key_(std::move(api_key)) {}

std::string OpenAIAdapter::id() const { return "openai"; }

bool OpenAIAdapter::supports_streaming() const { return true; }

// ---------------------------------------------------------------------------
// to_request_json — convert GenerateOptions → OpenAI Chat Completions JSON
//
// OpenAI format:
//   {"model":"gpt-4","messages":[{"role":"system","content":"..."},
//     {"role":"user","content":"Hello"}],"max_tokens":1024,
//     "temperature":0.7,"stream":true}
//
// Key difference from Anthropic: system role IS a standard message in the
// messages array, not a top-level field.
// ---------------------------------------------------------------------------
std::string OpenAIAdapter::to_request_json(const llm::GenerateOptions& opts) {
    std::ostringstream js;

    js << "{\"model\":\"" << json_escape(opts.model) << "\"";

    // --- messages array (all messages, including system) ---
    js << ",\"messages\":[";
    for (size_t i = 0; i < opts.messages.size(); ++i) {
        if (i > 0) js << ",";
        js << "{\"role\":\"" << json_escape(opts.messages[i].role) << "\""
           << ",\"content\":\"" << json_escape(opts.messages[i].content) << "\"}";
    }
    js << "]";

    // --- max_tokens ---
    if (opts.max_tokens.has_value()) {
        js << ",\"max_tokens\":" << opts.max_tokens.value();
    } else {
        js << ",\"max_tokens\":1024";
    }

    // --- temperature ---
    if (opts.temperature.has_value()) {
        js << ",\"temperature\":" << opts.temperature.value();
    }

    // --- streaming ---
    js << ",\"stream\":true";

    js << "}";
    return js.str();
}

// ---------------------------------------------------------------------------
// from_sse_line — parse single SSE data line → GenerateChunk
//
// OpenAI SSE format:
//   data: {"id":"...","object":"chat.completion.chunk",
//          "choices":[{"delta":{"content":"Hello"},"index":0,
//          "finish_reason":null}]}
//   data: [DONE]
//
// Chunk signals:
//   - choices[].delta.content → text token
//   - choices[].finish_reason (non-null) → stream termination
//   - [DONE] → stream done
// ---------------------------------------------------------------------------
tl::expected<llm::GenerateChunk, llm::Error>
OpenAIAdapter::from_sse_line(std::string_view sse_data) {
    std::string src(sse_data);
    if (src.empty()) return llm::GenerateChunk{};

    // --- [DONE] sentinel ---
    if (src == "[DONE]") {
        return llm::GenerateChunk{"", true};
    }

    // --- Check for error in response ---
    auto err_type = json_extract_str(src, "type");
    if (err_type == "error" || err_type == "server_error") {
        auto err_msg = json_extract_nested(src, "error", "message");
        if (err_msg.empty()) err_msg = "Unknown OpenAI API error";
        llm::GenerateChunk chunk;
        chunk.done = true;
        chunk.error_message = err_msg;
        return chunk;
    }

    // --- Extract delta text from choices[0].delta.content ---
    // Look for "delta" object then "content" within it
    auto delta_text = json_extract_nested(src, "delta", "content");

    // --- Check finish_reason ---
    bool done = false;
    auto finish_reason = json_extract_nested(
        src, "choices", "finish_reason");
    if (finish_reason.empty()) {
        // Try top-level finish_reason (some chunk formats)
        finish_reason = json_extract_str(src, "finish_reason");
    }
    if (!finish_reason.empty() && finish_reason != "null") {
        done = true;
    }

    return llm::GenerateChunk{delta_text, done};
}

// ---------------------------------------------------------------------------
// generate — HTTP POST to OpenAI Chat Completions with SSE streaming
//
// Endpoint:  POST https://api.openai.com/v1/chat/completions
// Headers:   Authorization: Bearer <api_key>, Content-Type: application/json
// Backoff:   Exponential on HTTP 429, max 3 retries (REQ-ADAPTERS-5)
// ---------------------------------------------------------------------------
tl::expected<void, llm::Error>
OpenAIAdapter::generate(const llm::GenerateOptions& opts,
                         std::function<void(llm::GenerateChunk)> on_chunk) {
    std::string request_json = to_request_json(opts);

    const std::string k_host = "api.openai.com";
    const std::string k_path = "/v1/chat/completions";
    const int k_max_retries = 3;
    int delay_seconds = 1;

    for (int attempt = 0; attempt <= k_max_retries; ++attempt) {
        auto resp = tcp_http_post(k_host, k_path, request_json, {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + api_key_}
        });

        if (!resp) {
            return tl::make_unexpected(resp.error());
        }

        // --- 200 OK: parse SSE stream ---
        if (resp->status_code == 200) {
            const std::string& body = resp->body;
            size_t pos = 0;

            // OpenAI may have multiple "data:" lines with empty lines between
            while (pos < body.size()) {
                auto data_start = body.find("data: ", pos);
                if (data_start == std::string::npos) break;

                data_start += 6;
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

            return {};
        }

        // --- 429: rate limited ---
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

        // --- 401 / 403: auth failure ---
        if (resp->status_code == 401 || resp->status_code == 403) {
            return tl::make_unexpected(llm::Error::Auth(
                "Authentication failed (HTTP " +
                std::to_string(resp->status_code) + ")"));
        }

        return tl::make_unexpected(llm::Error::Provider(
            "API error (HTTP " + std::to_string(resp->status_code) + ")"));
    }

    return tl::make_unexpected(llm::Error::Provider(
        "Unexpected retry loop exit"));
}

}  // namespace traveler::adapters
