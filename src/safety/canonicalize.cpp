// Reference: ~/hatch-v3/packages/hatch-safety/src/translator/llm/canonicalize.ts:53-96
// Reference: ~/hatch-v3/packages/hatch-safety/src/collector/anonymizer.ts:100-154
// Reference: ~/hatch-v3/packages/hatch-safety/src/mask/engine.ts:41-83
// Port: 1:1 TypeScript -> C++20. Single canonicalize() per Spec REQ-SAFETY-2.
//
// PC-14 RECOVERY (2026-05-07, Wizard2 plan):
//   - Replaced regex-based PII stage (Steps 2 rules 1-8) with deterministic
//     linear scanners.  Each scanner does one O(n) byte-scan with cheap
//     character-class checks — no regex compilation, NFA simulation,
//     backtracking, or double-pass regex_search+regex_replace overhead.
//   - Secret stage (Step 3b) keeps regex cache but adds cheap literal-string
//     prefilters so that regex_replace runs only when the literal substring
//     exists in the input.  This avoids the double-scan of the prior
//     regex_search guard.
//   - Rule order preserved exactly: URL → tilde → email → Windows → WSL →
//     Unix → host:port → IPv4.
#include "canonicalize.h"
#include "patterns.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <functional>
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
                std::string_view token(input.data() + token_start, i - token_start);
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
                cache_.erase(cache_.begin());
            }
            cache_[pattern] = std::move(re);
            return ptr;
        } catch (const std::regex_error&) {
            if (cache_.size() >= MAX_ENTRIES) {
                cache_.erase(cache_.begin());
            }
            cache_[pattern] = nullptr;
            return nullptr;
        }
    }

private:
    std::unordered_map<std::string, std::unique_ptr<std::regex>> cache_;
};

// ============================================================================
// Deterministic PII scanners — Wizard2 PC-14 recovery
//
// Each scanner replaces the corresponding regex block.  They perform a
// single linear pass over the input, checking trigger characters at each
// position.  When a trigger matches, the scanner expands to find the
// match boundary (using cheap character-class checks) and emits the
// replacement token.  Otherwise the byte is copied verbatim.
// ============================================================================

// Check whether a character terminates a PII token (whitespace or quote).
inline bool is_pii_term(char c) {
    return std::isspace(static_cast<unsigned char>(c)) || c == '"' || c == '\'';
}

// Copy s→buf, replacing URL patterns with [PATH].
// Matches: http://… / https://…  until whitespace/quote, max 2048 chars.
void scan_urls(std::string& s, std::string& buf) {
    buf.clear();
    buf.reserve(s.size());
    std::string_view sv = s;
    for (size_t i = 0; i < sv.size(); ) {
        if (sv[i] == 'h') {
            size_t prefix = 0;
            if (sv.size() - i >= 8 && sv.substr(i, 8) == "https://")       prefix = 8;
            else if (sv.size() - i >= 7 && sv.substr(i, 7) == "http://")  prefix = 7;
            if (prefix > 0) {
                size_t end = i + prefix;
                while (end < sv.size() && (end - i) < 2048 && !is_pii_term(sv[end]))
                    ++end;
                buf += "[PATH]";
                i = end;
                continue;
            }
        }
        buf += sv[i++];
    }
    std::swap(s, buf);
}

// Copy s→buf, replacing tilde paths (~/…) with [PATH].
// Matches: ~/… until whitespace/quote/colon, max 1024 chars.
void scan_tilde(std::string& s, std::string& buf) {
    buf.clear();
    buf.reserve(s.size());
    std::string_view sv = s;
    for (size_t i = 0; i < sv.size(); ) {
        if (sv[i] == '~' && i + 1 < sv.size() && sv[i + 1] == '/') {
            size_t end = i + 2;
            while (end < sv.size() && (end - i) < 1024 &&
                   !is_pii_term(sv[end]) && sv[end] != ':')
                ++end;
            buf += "[PATH]";
            i = end;
            continue;
        }
        buf += sv[i++];
    }
    std::swap(s, buf);
}

// Copy s→buf, replacing email addresses with [USER].
// Scans for '@', expands left over [a-zA-Z0-9._%+-] (max 64),
// expands right over [a-zA-Z0-9.-] (max 253).
void scan_email(std::string& s, std::string& buf) {
    buf.clear();
    buf.reserve(s.size());
    std::string_view sv = s;
    for (size_t i = 0; i < sv.size(); ) {
        if (sv[i] == '@') {
            // Expand left: local part
            size_t left = i;
            while (left > 0 && (i - left + 1) <= 64) {
                char c = sv[left - 1];
                if (std::isalnum(static_cast<unsigned char>(c)) ||
                    c == '.' || c == '_' || c == '%' || c == '+' || c == '-') {
                    --left;
                } else {
                    break;
                }
            }
            // Expand right: domain part
            size_t right = i + 1;
            while (right < sv.size() && (right - i) <= 253) {
                char c = sv[right];
                if (std::isalnum(static_cast<unsigned char>(c)) ||
                    c == '.' || c == '-') {
                    ++right;
                } else {
                    break;
                }
            }
            // Require at least one char on each side and domain must not
            // start/end with dot or hyphen.
            if (left < i && right > i + 1 &&
                sv[left] != '.' && sv[left] != '-' &&
                sv[right - 1] != '.' && sv[right - 1] != '-') {
                // Remove local-part chars already emitted into buf before
                // we detected the '@' trigger.
                buf.resize(buf.size() - (i - left));
                buf += "[USER]";
                i = right;
                continue;
            }
        }
        buf += sv[i++];
    }
    std::swap(s, buf);
}

// Copy s→buf, replacing Windows paths (C:\…) with [PATH].
// Matches: drive letter + ':\' + chars until whitespace/quote, max 1024.
void scan_windows_path(std::string& s, std::string& buf) {
    buf.clear();
    buf.reserve(s.size());
    std::string_view sv = s;
    for (size_t i = 0; i < sv.size(); ) {
        if (std::isalpha(static_cast<unsigned char>(sv[i])) &&
            i + 2 < sv.size() && sv[i + 1] == ':' && sv[i + 2] == '\\') {
            size_t end = i + 3;
            while (end < sv.size() && (end - i) < 1024 && !is_pii_term(sv[end]))
                ++end;
            buf += "[PATH]";
            i = end;
            continue;
        }
        buf += sv[i++];
    }
    std::swap(s, buf);
}

// Copy s→buf, replacing WSL paths (/mnt/[a-z]/…) with [PATH].
void scan_wsl_path(std::string& s, std::string& buf) {
    buf.clear();
    buf.reserve(s.size());
    std::string_view sv = s;
    for (size_t i = 0; i < sv.size(); ) {
        if (sv[i] == '/') {
            if (sv.size() - i >= 7 && sv.substr(i, 5) == "/mnt/" &&
                std::islower(static_cast<unsigned char>(sv[i + 5])) && sv[i + 6] == '/') {
                size_t end = i + 7;
                while (end < sv.size() && (end - i) < 1024 && !is_pii_term(sv[end]))
                    ++end;
                buf += "[PATH]";
                i = end;
                continue;
            }
        }
        buf += sv[i++];
    }
    std::swap(s, buf);
}

// Copy s→buf, replacing short Unix absolute paths with [PATH].
// Matches: /{known_prefix}/ + word/dot/hyphen chars up to 20 chars.
void scan_unix_path(std::string& s, std::string& buf) {
    // Known directory prefixes (sorted so that longer prefixes are tried
    // before shorter ones when one is a prefix of another).
    static constexpr std::string_view UNIX_PREFIXES[] = {
        "home/", "root/", "etc/", "tmp/", "var/", "opt/",
        "mnt/", "usr/", "Users/"
    };

    buf.clear();
    buf.reserve(s.size());
    std::string_view sv = s;
    for (size_t i = 0; i < sv.size(); ) {
        if (sv[i] == '/') {
            bool matched = false;
            for (auto pfx : UNIX_PREFIXES) {
                if (sv.size() - (i + 1) >= pfx.size() && sv.substr(i + 1, pfx.size()) == pfx) {
                    size_t end = i + 1 + pfx.size();
                    while (end < sv.size() && (end - i) < 1024) {
                        char c = sv[end];
                        if (std::isalnum(static_cast<unsigned char>(c)) ||
                            c == '/' || c == '.' || c == '_' || c == '-') {
                            ++end;
                        } else {
                            break;
                        }
                    }
                    buf += "[PATH]";
                    i = end;
                    matched = true;
                    break;
                }
            }
            if (matched) continue;
        }
        buf += sv[i++];
    }
    std::swap(s, buf);
}

// Copy s→buf, replacing hostname:port with [PATH]:[NUM].
// Scans for ':', expands left over hostname chars (alnum, dot, hyphen,
// max 253), expands right for 2–5 digits.  Must run AFTER path scanners.
void scan_hostport(std::string& s, std::string& buf) {
    buf.clear();
    buf.reserve(s.size());
    std::string_view sv = s;
    for (size_t i = 0; i < sv.size(); ) {
        if (sv[i] == ':') {
            // Count digits to the right (2–5 required)
            size_t right = i + 1;
            size_t digit_count = 0;
            while (right < sv.size() && digit_count < 5 &&
                   std::isdigit(static_cast<unsigned char>(sv[right]))) {
                ++right;
                ++digit_count;
            }
            if (digit_count < 2) { buf += sv[i++]; continue; }

            // Expand left over hostname chars
            size_t left = i;
            while (left > 0 && (i - left) < 253) {
                char c = sv[left - 1];
                if (std::isalnum(static_cast<unsigned char>(c)) ||
                    c == '.' || c == '-') {
                    --left;
                } else {
                    break;
                }
            }
            if (left < i && !(sv[left] == '.' || sv[left] == '-')) {
                // Remove hostname chars already emitted before ':' trigger.
                buf.resize(buf.size() - (i - left));
                buf += "[PATH]:[NUM]";
                i = right;
                continue;
            }
        }
        buf += sv[i++];
    }
    std::swap(s, buf);
}

// Copy s→buf, replacing IPv4 addresses with [PATH].
// Four 1–3 digit groups (0–255) separated by dots, with word boundaries.
// Must run AFTER host:port scanner.
void scan_ipv4(std::string& s, std::string& buf) {
    buf.clear();
    buf.reserve(s.size());
    std::string_view sv = s;

    // Fast digit-to-value lookup (0..999)
    auto try_parse_octet = [](std::string_view chunk) -> int {
        if (chunk.empty() || chunk.size() > 3) return -1;
        int val = 0;
        for (char c : chunk) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return -1;
            val = val * 10 + (c - '0');
        }
        return (val <= 255) ? val : -1;
    };

    for (size_t i = 0; i < sv.size(); ) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        if (std::isdigit(c)) {
            // Check left word boundary
            bool left_boundary = (i == 0) ||
                !std::isalnum(static_cast<unsigned char>(sv[i - 1]));
            if (!left_boundary) { buf += sv[i++]; continue; }

            // Find end of first octet
            size_t p = i;
            while (p < sv.size() && std::isdigit(static_cast<unsigned char>(sv[p])))
                ++p;
            if (p == i || p - i > 3) { buf += sv[i++]; continue; }

            // Try to parse all 4 octets
            size_t pos = i;
            int groups = 0;
            bool valid = true;

            for (int g = 0; g < 4 && valid; ++g) {
                size_t start = pos;
                while (pos < sv.size() && std::isdigit(static_cast<unsigned char>(sv[pos])))
                    ++pos;
                size_t len = pos - start;
                if (len == 0 || try_parse_octet(sv.substr(start, len)) < 0) {
                    valid = false;
                    break;
                }
                ++groups;
                if (g < 3) {
                    if (pos >= sv.size() || sv[pos] != '.') {
                        valid = false;
                        break;
                    }
                    ++pos; // skip dot
                }
            }

            if (valid && groups == 4) {
                // Check right word boundary
                bool right_boundary = (pos >= sv.size()) ||
                    !std::isalnum(static_cast<unsigned char>(sv[pos]));
                if (right_boundary) {
                    buf += "[PATH]";
                    i = pos;
                    continue;
                }
            }

            buf += sv[i++];
        } else {
            buf += sv[i++];
        }
    }
    std::swap(s, buf);
}

// ============================================================================
// Secret-stage literal prefilters — Wizard2 PC-14 plan §5
//
// For regex-based secret patterns, check for a cheap literal substring
// before running the expensive regex_replace.  This avoids the double-scan
// of regex_search→regex_replace and avoids unnecessary full-string copies.
// ============================================================================

// Return a mandatory literal substring that must appear in the input for
// a given secret pattern's regex to have any chance of matching.
// Returns nullptr when no reliable prefilter exists (regex_replace runs
// unconditionally for that pattern).
const char* secret_prefilter(const char* pattern_id) {
    // Static mapping ordered by pattern ID (defined in patterns.h).
    struct Mapping { const char* id; const char* needle; };
    static constexpr Mapping kMap[] = {
        {"C-AUTH-001",  "Bearer"},   // Bearer\s+[A-Za-z0-9_.~+/=-]+
        {"C-AUTH-002",  "Basic"},    // Basic\s+[A-Za-z0-9+/=]+
        {"C-JWT-001",   "eyJ"},      // eyJ… . … . …
        {"C-KV-001",    nullptr},     // too many OR-ed keywords → run regex
        {"C-JSON-001",  nullptr},     // too many OR-ed key names → run regex
        {"C-DSN-001",   "://"},       // postgres:// …, mysql:// …, etc.
    };
    for (const auto& m : kMap) {
        if (std::strcmp(m.id, pattern_id) == 0) return m.needle;
    }
    return nullptr;
}

}  // anonymous namespace

// ============================================================================
// canonicalize — main entry point
// ============================================================================

std::string canonicalize(std::string_view input) {
    std::string s;

    // Step 1: NUL sanitize (canonicalize.ts L55)
    s = input;
    if (auto pos = s.find('\0'); pos != std::string::npos) {
        s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());
    }

    // =========================================================================
    // Step 2: Strip PII — Wizard2 deterministic scanners
    //
    // Each scanner does one O(n) linear pass with cheap byte comparisons.
    // Stage order preserved exactly (URL → tilde → email → Windows → WSL →
    // Unix → host:port → IPv4).
    // =========================================================================
    std::string buf;
    buf.reserve(input.size() * 2);

    scan_urls(s, buf);          // Rule 1: URLs (before host:port)
    scan_tilde(s, buf);         // Rule 2: Tilde paths
    scan_email(s, buf);         // Rule 3: Email addresses
    scan_windows_path(s, buf);  // Rule 4: Windows paths
    scan_wsl_path(s, buf);      // Rule 5: WSL paths
    scan_unix_path(s, buf);     // Rule 6 (H11): Short Unix paths
    scan_hostport(s, buf);      // Rule 7: hostname:port (after paths)
    scan_ipv4(s, buf);          // Rule 9 (M13): IPv4 (after host:port)

    // =========================================================================
    // Step 3: Mask secrets (port of mask/engine.ts mask() L41-82)
    // =========================================================================

    // Stage 3a: Prefix-based token replacement (tokenizer port)
    {
        static const std::vector<const SecretPattern*> prefix_patterns = []{
            std::vector<const SecretPattern*> vec;
            for (size_t i = 0; i < SECRET_PATTERNS_COUNT; ++i) {
                if (SECRET_PATTERNS[i].match_type == MatchType::Prefix) {
                    vec.push_back(&SECRET_PATTERNS[i]);
                }
            }
            return vec;
        }();

        if (!prefix_patterns.empty()) {
            s = tokenizeAndReplace(s, [](std::string_view token) -> std::string {
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

    // Stage 3b: Regex-based replacement with cheap literal prefilters
    static RegexCache regex_cache;
    for (size_t i = 0; i < SECRET_PATTERNS_COUNT; ++i) {
        if (SECRET_PATTERNS[i].match_type != MatchType::Regex) continue;

        const auto& pattern = SECRET_PATTERNS[i];

        // --- Cheap literal prefilter (Wizard2 plan §5) ---
        const char* needle = secret_prefilter(pattern.id);
        if (needle && s.find(needle) == std::string::npos) continue;
        // -------------------------------------------------

        auto* re = regex_cache.get(pattern.match_value);
        if (!re) continue;

        const char* replacement = pattern.replacement ? pattern.replacement : "[MASKED]";
        try {
            buf.clear();
            std::regex_replace(std::back_inserter(buf), s.begin(), s.end(), *re, replacement);
            std::swap(s, buf);
        } catch (const std::regex_error&) {
            // Skip malformed replacement
        }
    }

    return s;
}

}  // namespace traveler::safety
