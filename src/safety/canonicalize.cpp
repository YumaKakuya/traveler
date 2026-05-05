// Reference: ~/hatch-v3/packages/hatch-safety/src/translator/llm/canonicalize.ts:53-96
// Reference: ~/hatch-v3/packages/hatch-safety/src/collector/anonymizer.ts:100-154
// Reference: ~/hatch-v3/packages/hatch-safety/src/mask/engine.ts:41-83
// Port: 1:1 TypeScript -> C++20. Single canonicalize() per Spec REQ-SAFETY-2.
#include "canonicalize.h"
#include "patterns.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace traveler::safety {

// ============================================================================
// Tokenizer — port of mask/tokenizer.ts L1-40
//
// Splits input on delimiter characters, applies matcher to each non-delimiter
// token, and replaces if matcher returns a non-empty replacement.
// ============================================================================
namespace {

const std::string DELIMITERS = " \t\n\r\"'`;()[]{}|=:";

bool isDelimiter(char c) {
    return DELIMITERS.find(c) != std::string::npos;
}

std::string tokenizeAndReplace(std::string_view input,
                                std::function<std::string(std::string_view)> matcher) {
    std::string result;
    result.reserve(input.size());
    size_t token_start = std::string::npos;

    for (size_t i = 0; i <= input.size(); ++i) {
        char ch = (i < input.size()) ? input[i] : '\0';

        if (i < input.size() && !isDelimiter(ch)) {
            // Accumulate token characters
            if (token_start == std::string::npos) {
                token_start = i;
            }
        } else {
            // Flush accumulated token
            if (token_start != std::string::npos) {
                std::string_view token = input.substr(token_start, i - token_start);
                auto replacement = matcher(token);
                if (!replacement.empty()) {
                    result += replacement;
                } else {
                    result += token;
                }
                token_start = std::string::npos;
            }
            // Emit delimiter
            if (i < input.size()) {
                result += ch;
            }
        }
    }

    return result;
}

// ============================================================================
// Regex cache — port of mask/engine.ts L6-35
//
// Bounded FIFO map (max 256 entries) for compiled regex patterns.
// ============================================================================
class RegexCache {
public:
    static constexpr size_t MAX_ENTRIES = 256;

    std::regex* get(const std::string& pattern) {
        auto it = cache_.find(pattern);
        if (it != cache_.end()) {
            return it->second.get();
        }
        try {
            auto re = std::make_unique<std::regex>(pattern, std::regex::icase);
            std::regex* ptr = re.get();
            if (cache_.size() >= MAX_ENTRIES) {
                // FIFO eviction: remove oldest entry
                cache_.erase(cache_.begin());
            }
            cache_[pattern] = std::move(re);
            return ptr;
        } catch (const std::regex_error&) {
            // Malformed regex — cache as null
            if (cache_.size() >= MAX_ENTRIES) {
                cache_.erase(cache_.begin());
            }
            cache_[pattern] = nullptr;
            return nullptr;
        }
    }

private:
    // std::unordered_map does NOT preserve insertion order. For a true FIFO
    // we'd need a linked hash map. Phase 0 accepts approximate FIFO via
    // unordered_map's unspecified iteration order. Hatch reference uses Map
    // (JS Map preserves insertion order). This is a known divergence.
    std::unordered_map<std::string, std::unique_ptr<std::regex>> cache_;
};

}  // anonymous namespace

// ============================================================================
// canonicalize — main entry point
// ============================================================================

std::string canonicalize(std::string_view input) {
    std::string s;

    // Step 1: NUL sanitize (canonicalize.ts L55)
    s.reserve(input.size());
    for (char c : input) {
        if (c != '\0') s += c;
    }

    // =========================================================================
    // Step 2: Strip PII (port of anonymizer.ts stripPII L100-149)
    // =========================================================================

    // Rule 1: URLs (before host:port)
    {
        std::regex url_re(R"(https?://[^\s"']{1,2048})", std::regex::icase);
        s = std::regex_replace(s, url_re, "[PATH]");
    }

    // Rule 2: Tilde paths
    {
        std::regex tilde_re(R"(~\/[^\s"':]{1,1024})");
        s = std::regex_replace(s, tilde_re, "[PATH]");
    }

    // Rule 3: Email addresses
    {
        std::regex email_re(R"([a-zA-Z0-9._%+-]{1,64}@[a-zA-Z0-9.-]{1,253})");
        s = std::regex_replace(s, email_re, "[USER]");
    }

    // Rule 4: Windows / WSL absolute paths
    {
        std::regex win_re(R"([A-Za-z]:\\[^\s"']{1,1024})");
        s = std::regex_replace(s, win_re, "[PATH]");
    }
    {
        std::regex wsl_re(R"(/mnt/[a-z]/[^\s"']{1,1024})");
        s = std::regex_replace(s, wsl_re, "[PATH]");
    }

    // Rule 5 (H11): Short Unix paths (bounded quantifier per REQ-SAFETY-4)
    {
        std::regex unix_path_re(R"(/(etc|tmp|var|opt|root|home|Users|usr|mnt)/[\w./-]{1,20})");
        s = std::regex_replace(s, unix_path_re, "[PATH]");
    }

    // Rule 6: hostname:port (after path removal to reduce false positives)
    {
        std::regex hostport_re(R"([a-zA-Z0-9.-]{1,253}:\d{2,5})");
        s = std::regex_replace(s, hostport_re, "[PATH]:[NUM]");
    }

    // Rule 9 (M13): IPv4
    {
        std::regex ipv4_re(R"(\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b)");
        s = std::regex_replace(s, ipv4_re, "[PATH]");
    }

    // =========================================================================
    // Step 3: Mask secrets (port of mask/engine.ts mask() L41-82)
    // =========================================================================

    // Stage 3a: Prefix-based token replacement (tokenizer port)
    {
        // Collect prefix patterns
        std::vector<const SecretPattern*> prefix_patterns;
        for (size_t i = 0; i < SECRET_PATTERNS_COUNT; ++i) {
            if (SECRET_PATTERNS[i].match_type == MatchType::Prefix) {
                prefix_patterns.push_back(&SECRET_PATTERNS[i]);
            }
        }

        if (!prefix_patterns.empty()) {
            s = tokenizeAndReplace(s, [&prefix_patterns](std::string_view token) -> std::string {
                for (auto* pattern : prefix_patterns) {
                    auto* pfx = pattern->match_value;
                    auto pfx_len = std::char_traits<char>::length(pfx);
                    if (token.size() >= pfx_len && token.substr(0, pfx_len) == std::string_view(pfx, pfx_len)) {
                        return pattern->replacement ? pattern->replacement : "[MASKED]";
                    }
                }
                return std::string{};  // no replacement
            });
        }
    }

    // Stage 3b: Regex-based replacement
    RegexCache regex_cache;
    for (size_t i = 0; i < SECRET_PATTERNS_COUNT; ++i) {
        if (SECRET_PATTERNS[i].match_type != MatchType::Regex) continue;

        const auto& pattern = SECRET_PATTERNS[i];
        auto* re = regex_cache.get(pattern.match_value);
        if (!re) continue;

        const char* replacement = pattern.replacement ? pattern.replacement : "[MASKED]";
        try {
            s = std::regex_replace(s, *re, replacement);
        } catch (const std::regex_error&) {
            // Skip malformed replacement
        }
    }

    return s;
}

}  // namespace traveler::safety
