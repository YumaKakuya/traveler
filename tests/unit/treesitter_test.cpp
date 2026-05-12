// GATE-P0-2 PC-6 Tree-sitter Test — Evidence for all 10 required languages
// Lane B: Worker W1 Evidence And Tests
//
// For each of the 10 PC-6 languages, parse a deterministic fixture and assert:
//   - parse success
//   - non-empty root node type
//   - at least one declaration node classified
//   - at least one statement node classified
//   - at least one expression node classified
// Print unsupported node types per language when present.
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "editor/treesitter.h"

using namespace traveler::editor;

static int g_passed = 0;
static int g_failed = 0;

#define TEST_RUN(name, expr) do {                                      \
    if (!(expr)) {                                                      \
        std::cerr << "  FAIL: " << name << std::endl;                   \
        g_failed++;                                                     \
    } else {                                                            \
        std::cout << "  PASS: " << name << std::endl;                   \
        g_passed++;                                                     \
    }                                                                   \
} while(0)

// Read a fixture file from tests/fixtures/treesitter/.
static std::string read_fixture(const std::string& filename) {
    std::string path = "tests/fixtures/treesitter/" + filename;
    std::ifstream in(path, std::ios::binary | std::ios::in);
    if (!in) {
        std::cerr << "  ERROR: cannot open fixture: " << path << std::endl;
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Test one language: parse fixture, assert PC-6 classification evidence.
static void test_language(TreeSitterLanguage lang,
                          const std::string& fixture_name,
                          const std::string& label) {
    std::cout << "\n--- " << label << " (" << fixture_name << ") ---\n";

    std::string source = read_fixture(fixture_name);
    if (source.empty()) {
        TEST_RUN(label + ": fixture readable", false);
        return;
    }
    std::cout << "  source size: " << source.size() << " bytes\n";

    ParseResult result = parse(source, lang);

    // PC-6: parse success
    TEST_RUN(label + ": parse success", result.success);
    if (!result.success) {
        std::cout << "  error: " << result.error_message << "\n";
    }

    // PC-6: non-empty root node type
    TEST_RUN(label + ": non-empty root node type",
             !result.root_node_type.empty());
    std::cout << "  root node type: " << result.root_node_type << "\n";

    // PC-6: declaration present
    TEST_RUN(label + ": has declaration", result.has_declaration);

    // PC-6: statement present
    TEST_RUN(label + ": has statement", result.has_statement);

    // PC-6: expression present
    TEST_RUN(label + ": has expression", result.has_expression);

    // PC-6: enumerate unsupported node types
    if (!result.unsupported_node_types.empty()) {
        std::cout << "  unsupported node types (" 
                  << result.unsupported_node_types.size() << "):";
        for (const auto& t : result.unsupported_node_types) {
            std::cout << " " << t;
        }
        std::cout << "\n";
    } else {
        std::cout << "  unsupported node types: (none)\n";
    }
}

int main() {
    std::cout << "=== GATE-P0-2 PC-6 Tree-sitter Test ===" << std::endl;

    // Test all 10 PC-6 languages
    test_language(TreeSitterLanguage::C,           "fixture.c",    "C");
    test_language(TreeSitterLanguage::CPP,         "fixture.cpp",  "C++");
    test_language(TreeSitterLanguage::Python,      "fixture.py",   "Python");
    test_language(TreeSitterLanguage::JavaScript,   "fixture.js",   "JavaScript");
    test_language(TreeSitterLanguage::TypeScript,  "fixture.ts",   "TypeScript");
    test_language(TreeSitterLanguage::Rust,        "fixture.rs",   "Rust");
    test_language(TreeSitterLanguage::Go,          "fixture.go",   "Go");
    test_language(TreeSitterLanguage::Java,        "fixture.java", "Java");
    test_language(TreeSitterLanguage::Ruby,        "fixture.rb",   "Ruby");
    test_language(TreeSitterLanguage::Bash,        "fixture.sh",   "Bash");

    // Summary
    std::cout << "\n=== Results: " << g_passed << " PASS, "
              << g_failed << " FAIL ===" << std::endl;
    return g_failed == 0 ? 0 : 1;
}
