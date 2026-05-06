// Reference: Traveler_Phase0_Spec_v0.1.md §7.1 (LLM Provider Plugin Layer Interface)
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <tl/expected.hpp>

namespace traveler::llm {

struct Message {
    std::string role;
    std::string content;
};

struct GenerateOptions {
    std::string model;
    std::vector<Message> messages;
    std::optional<int> max_tokens;
    std::optional<float> temperature;
};

struct GenerateChunk {
    std::string delta;
    bool done = false;
    std::optional<std::string> error_message;
};

struct Error {
    std::string message;

    static Error Network(std::string msg)  { return {std::move(msg)}; }
    static Error Auth(std::string msg)     { return {std::move(msg)}; }
    static Error Provider(std::string msg) { return {std::move(msg)}; }
};

class Provider {
public:
    virtual ~Provider() = default;
    virtual std::string id() const = 0;
    virtual bool supports_streaming() const = 0;
    virtual tl::expected<void, Error>
    generate(const GenerateOptions& opts,
             std::function<void(GenerateChunk)> on_chunk) = 0;
};

}  // namespace traveler::llm
