// Reference: ~/hatch-v3/packages/opencode/src/agent/roles.ts:61-155 (parseRoles)
// Port: 1:1 TypeScript → C++20. Preserves behaviour, adapts idioms. No feature additions.
#include "parser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

namespace traveler::roles {

// ============================================================================
// Helpers
// ============================================================================

bool isValidRoleName(const std::string& name) {
    // roles.ts L54: /^[a-zA-Z0-9_-]+$/
    if (name.empty()) return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
    });
}

bool isValidModelFormat(const std::string& model) {
    // roles.ts L135-136: modelStr.includes("/")
    return model.find('/') != std::string::npos;
}

static bool isProtectedName(const std::string& name) {
    // roles.ts L48: PROTECTED_NAMES = {"compaction", "title", "summary"}
    static const std::set<std::string> protected_set = {
        "compaction", "title", "summary"
    };
    return protected_set.count(name) > 0;
}

// ============================================================================
// Minimal YAML frontmatter parser
//
// Handles the subset used in roles.md frontmatter:
//   version: 1
//   roles:
//     vega:
//       model: anthropic/claude-opus-4-7
//       variant: opus
//       mode: primary
//       temperature: 0.7
//       top_p: 0.9
//       description: "Primary reasoning agent"
//       hidden: false
//       steps: 10
//
// No external YAML library dependency (faithful to Spec §1.2 single-binary).
// ============================================================================

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Parse a simple YAML scalar value (string, number, boolean)
static std::string parseYamlScalar(const std::string& line) {
    auto colon = line.find(':');
    if (colon == std::string::npos) return "";
    std::string val = trim(line.substr(colon + 1));
    // Strip surrounding quotes
    if (val.size() >= 2) {
        if ((val.front() == '"' && val.back() == '"') ||
            (val.front() == '\'' && val.back() == '\'')) {
            val = val.substr(1, val.size() - 2);
        }
    }
    return val;
}

// Parse YAML frontmatter (between --- delimiters)
// Returns a map of role name → (key → value) pairs
static std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
parseFrontmatter(const std::string& content) {
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> roles;

    // Find frontmatter between --- delimiters
    auto start = content.find("---\n");
    if (start == std::string::npos) start = content.find("---\r\n");
    if (start == std::string::npos) return roles;  // no frontmatter

    start += 4;  // skip "---\n"
    auto end = content.find("\n---", start);
    if (end == std::string::npos) end = content.find("\r\n---", start);
    if (end == std::string::npos) return roles;  // unclosed frontmatter

    std::string fm = content.substr(start, end - start);

    // Line-by-line simple YAML parsing
    std::istringstream iss(fm);
    std::string line;
    std::string current_role;
    int version = 0;
    bool in_roles = false;

    while (std::getline(iss, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        // Count indentation (spaces at start)
        size_t indent = 0;
        while (indent < line.size() && line[indent] == ' ') indent++;

        std::string deindented = trim(line);

        if (indent == 0) {
            // Top-level key
            if (deindented.starts_with("version:")) {
                try {
                    version = std::stoi(parseYamlScalar(deindented));
                } catch (...) {
                    version = 0;
                }
            } else if (deindented == "roles:") {
                in_roles = true;
            } else {
                in_roles = false;
            }
            current_role.clear();
        } else if (indent == 2 && in_roles) {
            // Role name (top-level under roles:)
            if (deindented.ends_with(":")) {
                current_role = deindented.substr(0, deindented.size() - 1);
                current_role = trim(current_role);
            } else {
                current_role.clear();
            }
        } else if (indent == 4 && in_roles && !current_role.empty()) {
            // Role property
            auto colon = deindented.find(':');
            if (colon != std::string::npos) {
                std::string key = trim(deindented.substr(0, colon));
                std::string val = parseYamlScalar(deindented);
                roles[current_role][key] = val;
            }
        }
    }

    return roles;
}

// ============================================================================
// parseRoles — main entry point (port of roles.ts L61-155)
// ============================================================================

tl::expected<std::unordered_map<std::string, ParsedRole>, Error>
parseRoles(const std::string& path) {
    namespace fs = std::filesystem;
    fs::path filePath = path;
    // If the path is a directory, append "roles.md" for backward compatibility.
    // If it's an existing file, use it directly (exact path handling per REQ-ROLES-1/2).
    if (fs::is_directory(filePath)) {
        filePath = filePath / "roles.md";
    }

    // --- File existence check (roles.ts L66-68) ---
    if (!fs::exists(filePath)) {
        // roles.ts L71: ENOENT → empty map, no warning
        return std::unordered_map<std::string, ParsedRole>{};
    }

    // --- Read file (sync I/O, roles.md is local) ---
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return tl::make_unexpected(Error::IO("Failed to open " + filePath.string()));
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    std::string content = buf.str();
    file.close();

    // --- Parse frontmatter (roles.ts L68: ConfigMarkdown.parse) ---
    auto roles_data = parseFrontmatter(content);

    // --- version check (roles.ts L83-90) ---
    // version field is checked from the frontmatter; parseFrontmatter doesn't
    // return top-level keys directly, so we extract version separately
    {
        auto fm_start = content.find("---\n");
        if (fm_start == std::string::npos) fm_start = content.find("---\r\n");
        if (fm_start != std::string::npos) {
            fm_start += 4;
            auto fm_end = content.find("\n---", fm_start);
            if (fm_end == std::string::npos) fm_end = content.find("\r\n---", fm_start);
            if (fm_end != std::string::npos) {
                std::string fm = content.substr(fm_start, fm_end - fm_start);
                // Check for version field
                bool has_version = fm.find("\nversion:") != std::string::npos ||
                                   fm.starts_with("version:");
                if (!has_version) {
                    // roles.ts L84-86: version field required → warning, empty map
                    return std::unordered_map<std::string, ParsedRole>{};
                }

                // Check version == 1
                std::regex version_re(R"(version:\s*(\d+))");
                std::smatch m;
                if (std::regex_search(fm, m, version_re)) {
                    int ver = std::stoi(m[1].str());
                    if (ver != 1) {
                        // roles.ts L87-89: unsupported version → empty map
                        return std::unordered_map<std::string, ParsedRole>{};
                    }
                } else {
                    return std::unordered_map<std::string, ParsedRole>{};
                }
            }
        }
    }

    // roles.ts L93-95: no roles defined → empty map
    if (roles_data.empty()) {
        return std::unordered_map<std::string, ParsedRole>{};
    }

    // --- Remove frontmatter to get body (roles.ts L80) ---
    std::string body;
    {
        auto fm_start = content.find("---\n");
        if (fm_start == std::string::npos) fm_start = content.find("---\r\n");
        if (fm_start != std::string::npos) {
            fm_start += 4;
            auto fm_end = content.find("\n---", fm_start);
            if (fm_end == std::string::npos) fm_end = content.find("\r\n---", fm_start);
            if (fm_end != std::string::npos) {
                body = content.substr(fm_end + 4);
            } else {
                body = content;
            }
        } else {
            body = content;
        }
    }

    // --- Body H2 section parsing (roles.ts L99-108) ---
    std::unordered_map<std::string, std::string> bodyPrompts;
    {
        std::string current_name;
        std::string current_content;
        auto flush_section = [&]() {
            if (!current_name.empty()) {
                bodyPrompts[trim(current_name)] = trim(current_content);
                current_content.clear();
            }
        };

        size_t line_start = 0;
        while (line_start <= body.size()) {
            size_t line_end = body.find('\n', line_start);
            if (line_end == std::string::npos) {
                line_end = body.size();
            }

            std::string line = body.substr(line_start, line_end - line_start);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.rfind("## ", 0) == 0) {
                flush_section();
                current_name = line.substr(3);
            } else if (!current_name.empty()) {
                current_content += line;
                current_content += '\n';
            }

            if (line_end == body.size()) {
                break;
            }
            line_start = line_end + 1;
        }
        flush_section();
    }

    // roles.ts L111-114: warn if H2 heading doesn't match a role
    // (in C++ port, we silently skip; Worker will add logging in test phase)

    // --- Build ParsedRole entries (roles.ts L118-152) ---
    std::unordered_map<std::string, ParsedRole> result;
    for (const auto& [name, props] : roles_data) {
        // Name validation (roles.ts L121-124)
        if (!isValidRoleName(name)) {
            continue;  // warning logged in TS; skip in C++
        }

        // Protected name check (roles.ts L126-128)
        if (isProtectedName(name)) {
            continue;  // skip protected agents
        }

        // Skip roles without a valid model string (REQ-ROLES-1/2)
        auto model_it = props.find("model");
        if (model_it == props.end() || model_it->second.empty() || !isValidModelFormat(model_it->second)) {
            continue;
        }

        ParsedRole parsed;

        // model (roles.ts L132-140)
        if (model_it != props.end()) {
            std::string modelStr = model_it->second;
            if (isValidModelFormat(modelStr)) {
                parsed.model = modelStr;
            }
            // else: invalid format → model stays empty (roles.ts L136-137)
        }

        // variant (roles.ts L141)
        auto variant_it = props.find("variant");
        if (variant_it != props.end()) {
            parsed.variant = variant_it->second;
        }

        // mode (roles.ts L142-144)
        auto mode_it = props.find("mode");
        if (mode_it != props.end()) {
            const auto& m = mode_it->second;
            if (m == "subagent" || m == "primary" || m == "all") {
                parsed.mode = m;
            }
        }

        // temperature (roles.ts L145)
        auto temp_it = props.find("temperature");
        if (temp_it != props.end()) {
            try {
                parsed.temperature = std::stod(temp_it->second);
            } catch (...) {
                parsed.temperature = -1;
            }
        }

        // top_p (roles.ts L146)
        auto top_p_it = props.find("top_p");
        if (top_p_it != props.end()) {
            try {
                parsed.top_p = std::stod(top_p_it->second);
            } catch (...) {
                parsed.top_p = -1;
            }
        }

        // description (roles.ts L147)
        auto desc_it = props.find("description");
        if (desc_it != props.end()) {
            parsed.description = desc_it->second;
        }

        // hidden (roles.ts L148)
        auto hidden_it = props.find("hidden");
        if (hidden_it != props.end()) {
            parsed.hidden = (hidden_it->second == "true");
        }

        // steps (roles.ts L149)
        auto steps_it = props.find("steps");
        if (steps_it != props.end()) {
            try {
                parsed.steps = std::stoi(steps_it->second);
            } catch (...) {
                parsed.steps = -1;
            }
        }

        // prompt (body H2 section) (roles.ts L150)
        auto prompt_it = bodyPrompts.find(name);
        if (prompt_it != bodyPrompts.end() && !prompt_it->second.empty()) {
            parsed.prompt = prompt_it->second;
        }

        result[name] = std::move(parsed);
    }

    return result;
}

}  // namespace traveler::roles
