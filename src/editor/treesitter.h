#pragma once
// GATE-P0-2 PC-6 Tree-sitter Integration — Production API
// Lane A: Senior S1 Production Integration
//
// Provides minimal parse/classification API for Tree-sitter-backed syntax
// analysis of 10 mainstream languages. No live TUI integration required.
// Classification is explicit per language; shared node-name groups are used
// only where Tree-sitter node names genuinely align.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace traveler::editor {

// ---------------------------------------------------------------------------
// Language enumeration — exactly the PC-6 mainstream languages
// ---------------------------------------------------------------------------
enum class TreeSitterLanguage : uint8_t {
    C = 0,
    CPP,
    Python,
    JavaScript,
    TypeScript,
    Rust,
    Go,
    Java,
    Ruby,
    Bash,
    // Sentinel
    COUNT
};

// ---------------------------------------------------------------------------
// Parse result — satisfies PC-6 evidence requirements
// ---------------------------------------------------------------------------
struct ParseResult {
    TreeSitterLanguage language;
    bool success = false;
    std::string root_node_type;             // e.g. "translation_unit", "module"
    bool has_declaration = false;           // ≥1 declaration node found
    bool has_statement = false;             // ≥1 statement node found
    bool has_expression = false;            // ≥1 expression node found
    std::vector<std::string> unsupported_node_types;  // nodes found but not classified
    std::string error_message;              // non-empty if !success
};

// ---------------------------------------------------------------------------
// Core API
// ---------------------------------------------------------------------------

// Parse source text using the grammar for `lang`.
// Returns a ParseResult with classification flags.
ParseResult parse(std::string_view source, TreeSitterLanguage lang);

// ---------------------------------------------------------------------------
// Language selection helpers
// ---------------------------------------------------------------------------

// Map a file extension to a language enum.
// Returns true if matched; `lang` is set to the matching language.
// Recognized extensions: c, cc/cpp/cxx/c++, py, js/mjs, ts, rs, go, java, rb, sh/bash
bool language_from_extension(std::string_view ext, TreeSitterLanguage& lang);

// Return the human-readable language name.
const char* language_name(TreeSitterLanguage lang);

}  // namespace traveler::editor
