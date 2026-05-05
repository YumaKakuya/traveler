// Reference: Traveler_Phase0_Spec_v0.1.md §8.3 (REQ-ADAPTERS-1, REQ-ADAPTERS-2, REQ-ADAPTERS-5)
// Reference: ~/hatch-v3/packages/opencode/src/provider/google.ts
// Port: Gemini API adapter — maps canonical Message ↔ Gemini format.
// Gemini API docs: https://ai.google.dev/gemini-api/docs/text-generation
//
// Key format differences:
//  - System prompt → top-level "systemInstruction" field
//  - Messages → "contents" array with "parts": [{"text": "..."}] objects
//  - Response → candidates[].content.parts[].text
//  - Auth: ?key=<api_key> query parameter
#include "google_adapter.h"

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

// Collect all text values from "text":"..." occurrences, filtering
// structural type markers.
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
        ++pos;
        if (val.size() <= 4 && (val == "text" || val == "json")) {
            continue;
        }
        if (!val.empty()) {
            if (!result.empty()) result += '\n';
            result += val;
        }
    }
    return result;
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
// GoogleAdapter — Provider interface
// ============================================================================

GoogleAdapter::GoogleAdapter(std::string api_key)
    : api_key_(std::move(api_key)) {}

std::string GoogleAdapter::id() const { return "google"; }

bool GoogleAdapter::supports_streaming() const { return true; }

// ---------------------------------------------------------------------------
// to_request_json — convert GenerateOptions → Gemini API JSON
//
// Gemini format:
//   {"contents":[{"role":"user","parts":[{"text":"Hello"}]}],
//    "systemInstruction":{"parts":[{"text":"system prompt"}]},
//    "generationConfig":{"maxOutputTokens":1024, "temperature":0.7}}
//
// Key difference: system prompt goes in systemInstruction, messages use
// "parts" arrays with {"text":"..."} objects.
// ---------------------------------------------------------------------------
std::string GoogleAdapter::to_request_json(const llm::GenerateOptions& opts) {
    std::ostringstream js;
    js << "{";

    // --- systemInstruction (from role="system" messages) ---
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
        js << "\"systemInstruction\":{\"parts\":[{\"text\":\""
           << json_escape(system_prompt) << "\"}]},";
    }

    // --- contents array ---
    js << "\"contents\":[";
    for (size_t i = 0; i < non_system.size(); ++i) {
        if (i > 0) js << ",";
        // Gemini roles: "user" or "model"
        std::string role = non_system[i].role;
        if (role == "assistant") role = "model";  // Gemini uses "model" not "assistant"
        js << "{\"role\":\"" << role << "\",\"parts\":[{\"text\":\""
           << json_escape(non_system[i].content) << "\"}]}";
    }
    js << "]";

    // --- generationConfig ---
    js << ",\"generationConfig\":{";
    if (opts.max_tokens.has_value()) {
        js << "\"maxOutputTokens\":" << opts.max_tokens.value();
    } else {
        js << "\"maxOutputTokens\":1024";
    }
    if (opts.temperature.has_value()) {
        js << ",\"temperature\":" << opts.temperature.value();
    }
    js << "}";

    js << "}";
    return js.str();
}

// ---------------------------------------------------------------------------
// from_response_json — parse complete (non-streaming) Gemini response
//
// Gemini format:
//   {"candidates":[{"content":{"parts":[{"text":"Hello!"}],
//    "role":"model"},"finishReason":"STOP"}]}
// ---------------------------------------------------------------------------
tl::expected<std::string, llm::Error>
GoogleAdapter::from_response_json(std::string_view json) {
    std::string src(json);

    // Check for top-level error
    auto err_type = json_extract_str(src, "error");
    if (!err_type.empty()) {
        auto err_msg = err_type;
        auto msg = json_extract_str(src, "message");
        if (!msg.empty()) err_msg += ": " + msg;
        return tl::make_unexpected(llm::Error::Provider(err_msg));
    }

    std::string text = json_collect_text(src);
    if (text.empty()) {
        // Check if there's a blockReason or safety issue
        auto block = json_extract_str(src, "blockReason");
        if (!block.empty()) {
            return tl::make_unexpected(llm::Error::Provider(
                "Content blocked: " + block));
        }
    }
    return text;
}

// ---------------------------------------------------------------------------
// from_sse_line — parse single SSE data line → GenerateChunk
//
// Gemini SSE streaming format:
//   data: {"candidates":[{"content":{"parts":[{"text":"Hello"}],
//          "role":"model"},"finishReason":"STOP"}]}
//
// Streaming chunks use incremental candidates with partial text.
// ---------------------------------------------------------------------------
tl::expected<llm::GenerateChunk, llm::Error>
GoogleAdapter::from_sse_line(std::string_view sse_data) {
    std::string src(sse_data);
    if (src.empty()) return llm::GenerateChunk{};

    // Check for error
    auto err_type = json_extract_str(src, "error");
    if (!err_type.empty()) {
        auto err_msg = err_type;
        auto msg = json_extract_str(src, "message");
        if (!msg.empty()) err_msg += ": " + msg;
        llm::GenerateChunk chunk;
        chunk.done = true;
        chunk.error_message = err_msg;
        return chunk;
    }

    // Extract text from candidates[].content.parts[].text
    // In streaming mode, each chunk has a candidates array with partial content
    std::string text = json_collect_text(src);

    // Check if stream is complete
    bool done = false;
    // finishReason signals the end (may be "STOP", "MAX_TOKENS", "SAFETY", etc.)
    auto finish = json_extract_str(src, "finishReason");
    if (!finish.empty()) {
        done = true;
    }

    return llm::GenerateChunk{text, done};
}

// ---------------------------------------------------------------------------
// generate — HTTP POST to Gemini API with SSE streaming
//
// Endpoint:  POST https://generativelanguage.googleapis.com/v1beta/models/
//            {model}:streamGenerateContent?alt=sse&key={api_key}
//
// Auth:      API key passed as ?key= query parameter.
// Backoff:   Exponential on HTTP 429, max 3 retries (REQ-ADAPTERS-5)
// ---------------------------------------------------------------------------
tl::expected<void, llm::Error>
GoogleAdapter::generate(const llm::GenerateOptions& opts,
                         std::function<void(llm::GenerateChunk)> on_chunk) {
    std::string request_json = to_request_json(opts);

    // Gemini endpoint: use the model name from options for the URL path
    std::string model = opts.model;
    if (model.empty()) model = "gemini-pro";

    const std::string k_host = "generativelanguage.googleapis.com";
    const std::string k_path = "/v1beta/models/" + model +
                               ":streamGenerateContent?alt=sse&key=" + api_key_;
    const int k_max_retries = 3;
    int delay_seconds = 1;

    for (int attempt = 0; attempt <= k_max_retries; ++attempt) {
        auto resp = tcp_http_post(k_host, k_path, request_json, {
            {"Content-Type", "application/json"}
        });

        if (!resp) {
            return tl::make_unexpected(resp.error());
        }

        // --- 200 OK: parse SSE stream ---
        if (resp->status_code == 200) {
            const std::string& body = resp->body;
            size_t pos = 0;

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
