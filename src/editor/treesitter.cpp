#include "editor/treesitter.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

extern "C" {
#include <tree_sitter/api.h>

// Grammar entry points — one per language
TSLanguage* tree_sitter_c(void);
TSLanguage* tree_sitter_cpp(void);
TSLanguage* tree_sitter_python(void);
TSLanguage* tree_sitter_javascript(void);
TSLanguage* tree_sitter_typescript(void);
TSLanguage* tree_sitter_rust(void);
TSLanguage* tree_sitter_go(void);
TSLanguage* tree_sitter_java(void);
TSLanguage* tree_sitter_ruby(void);
TSLanguage* tree_sitter_bash(void);
}

namespace traveler::editor {

// =========================================================================
// Internal helpers
// =========================================================================

namespace {

// Return the TSLanguage pointer for a given language enum.
const TSLanguage* language_ptr(TreeSitterLanguage lang) {
    switch (lang) {
    case TreeSitterLanguage::C:           return tree_sitter_c();
    case TreeSitterLanguage::CPP:          return tree_sitter_cpp();
    case TreeSitterLanguage::Python:       return tree_sitter_python();
    case TreeSitterLanguage::JavaScript:   return tree_sitter_javascript();
    case TreeSitterLanguage::TypeScript:   return tree_sitter_typescript();
    case TreeSitterLanguage::Rust:         return tree_sitter_rust();
    case TreeSitterLanguage::Go:           return tree_sitter_go();
    case TreeSitterLanguage::Java:         return tree_sitter_java();
    case TreeSitterLanguage::Ruby:         return tree_sitter_ruby();
    case TreeSitterLanguage::Bash:         return tree_sitter_bash();
    default: return nullptr;
    }
}

// -------------------------------------------------------------------------
// Per-language node-type classification tables
// Each language has three sets: declaration, statement, expression.
// Node types encountered during tree walk that match none of the three
// sets are recorded as unsupported.
// -------------------------------------------------------------------------

// C
const std::unordered_set<std::string>& c_declarations() {
    static const std::unordered_set<std::string> s = {
        "function_definition", "declaration", "struct_specifier",
        "enum_specifier", "union_specifier", "type_definition",
        "preproc_def", "preproc_function_def", "init_declarator",
        "pointer_declarator", "function_declarator", "array_declarator",
        "parameter_list", "parameter_declaration"
    };
    return s;
}
const std::unordered_set<std::string>& c_statements() {
    static const std::unordered_set<std::string> s = {
        "expression_statement", "return_statement", "if_statement",
        "for_statement", "while_statement", "do_statement",
        "switch_statement", "compound_statement", "break_statement",
        "continue_statement", "goto_statement", "case_statement",
        "default_statement", "labeled_statement"
    };
    return s;
}
const std::unordered_set<std::string>& c_expressions() {
    static const std::unordered_set<std::string> s = {
        "binary_expression", "call_expression", "identifier",
        "number_literal", "string_literal", "char_literal",
        "parenthesized_expression", "subscript_expression",
        "field_expression", "unary_expression", "sizeof_expression",
        "cast_expression", "conditional_expression",
        "comma_expression", "assignment_expression", "pointer_expression",
        "update_expression", "arrow_expression",
        "primitive_type"
    };
    return s;
}

// C++
const std::unordered_set<std::string>& cpp_declarations() {
    static const std::unordered_set<std::string> s = {
        "function_definition", "declaration", "class_specifier",
        "struct_specifier", "enum_specifier", "template_declaration",
        "namespace_definition", "linkage_specification", "type_definition",
        "alias_declaration", "concept_definition", "using_declaration",
        "function_declarator", "field_declaration",
        "parameter_list", "parameter_declaration"
    };
    return s;
}
const std::unordered_set<std::string>& cpp_statements() {
    static const std::unordered_set<std::string> s = {
        "expression_statement", "return_statement", "if_statement",
        "for_statement", "while_statement", "do_statement",
        "switch_statement", "compound_statement", "break_statement",
        "continue_statement", "goto_statement", "case_statement",
        "co_return_statement", "co_yield_statement", "try_statement",
        "throw_statement", "catch_clause", "labeled_statement"
    };
    return s;
}
const std::unordered_set<std::string>& cpp_expressions() {
    static const std::unordered_set<std::string> s = {
        "binary_expression", "call_expression", "identifier",
        "number_literal", "string_literal", "char_literal",
        "parenthesized_expression", "subscript_expression",
        "field_expression", "unary_expression", "sizeof_expression",
        "cast_expression", "conditional_expression",
        "comma_expression", "assignment_expression",
        "new_expression", "delete_expression", "this_expression",
        "lambda_expression", "fold_expression", "requires_expression",
        "template_function", "template_type", "qualified_identifier",
        "user_defined_literal"
    };
    return s;
}

// Python
const std::unordered_set<std::string>& python_declarations() {
    static const std::unordered_set<std::string> s = {
        "function_definition", "class_definition",
        "import_statement", "import_from_statement",
        "decorated_definition"
    };
    return s;
}
const std::unordered_set<std::string>& python_statements() {
    static const std::unordered_set<std::string> s = {
        "expression_statement", "return_statement", "if_statement",
        "for_statement", "while_statement", "try_statement",
        "with_statement", "assert_statement", "raise_statement",
        "break_statement", "continue_statement", "pass_statement",
        "match_statement", "delete_statement"
    };
    return s;
}
const std::unordered_set<std::string>& python_expressions() {
    static const std::unordered_set<std::string> s = {
        "binary_operator", "call", "identifier", "integer", "float",
        "string", "list", "dictionary", "tuple", "set", "list_comprehension",
        "dictionary_comprehension", "set_comprehension",
        "generator_expression", "comparison_operator", "unary_operator",
        "boolean_operator", "lambda", "attribute", "subscript",
        "slice", "await", "not_operator", "parenthesized_expression",
        "concatenated_string", "conditional_expression",
        "named_expression", "keyword_argument", "pair"
    };
    return s;
}

// JavaScript
const std::unordered_set<std::string>& js_declarations() {
    static const std::unordered_set<std::string> s = {
        "function_declaration", "variable_declaration", "class_declaration",
        "import_statement", "export_statement", "method_definition",
        "lexical_declaration", "generator_function_declaration"
    };
    return s;
}
const std::unordered_set<std::string>& js_statements() {
    static const std::unordered_set<std::string> s = {
        "expression_statement", "return_statement", "if_statement",
        "for_statement", "for_in_statement", "while_statement",
        "do_statement", "switch_statement", "try_statement",
        "throw_statement", "break_statement", "continue_statement",
        "debugger_statement", "labeled_statement", "with_statement"
    };
    return s;
}
const std::unordered_set<std::string>& js_expressions() {
    static const std::unordered_set<std::string> s = {
        "binary_expression", "call_expression", "identifier", "number",
        "string", "template_string", "array", "object",
        "arrow_function", "function_expression",
        "member_expression", "subscript_expression",
        "new_expression", "assignment_expression", "update_expression",
        "unary_expression", "await_expression", "yield_expression",
        "ternary_expression", "this", "super", "null", "true", "false",
        "regex", "sequence_expression", "parenthesized_expression",
        "spread_element", "computed_property_name"
    };
    return s;
}

// TypeScript (same node types as JS + TS-specific additions)
const std::unordered_set<std::string>& ts_declarations() {
    static const std::unordered_set<std::string> s = {
        "function_declaration", "variable_declaration", "class_declaration",
        "import_statement", "export_statement", "method_definition",
        "lexical_declaration", "generator_function_declaration",
        "type_alias_declaration", "interface_declaration",
        "enum_declaration", "abstract_class_declaration",
        "module", "ambient_declaration"
    };
    return s;
}
const std::unordered_set<std::string>& ts_statements() {
    // TypeScript shares JS statement node type names
    return js_statements();
}
const std::unordered_set<std::string>& ts_expressions() {
    static const std::unordered_set<std::string> s = {
        "binary_expression", "call_expression", "identifier", "number",
        "string", "template_string", "array", "object",
        "arrow_function", "function_expression",
        "member_expression", "subscript_expression",
        "new_expression", "assignment_expression", "update_expression",
        "unary_expression", "await_expression", "yield_expression",
        "ternary_expression", "this", "super", "null", "true", "false",
        "regex", "sequence_expression", "parenthesized_expression",
        "spread_element", "computed_property_name",
        "as_expression", "non_null_expression", "type_annotation",
        "type_arguments", "type_parameters", "satisfies_expression"
    };
    return s;
}

// Rust
const std::unordered_set<std::string>& rust_declarations() {
    static const std::unordered_set<std::string> s = {
        "function_item", "struct_item", "enum_item", "trait_item",
        "impl_item", "mod_item", "use_declaration", "const_item",
        "static_item", "type_item", "macro_definition",
        "foreign_mod_item", "extern_crate_declaration"
    };
    return s;
}
const std::unordered_set<std::string>& rust_statements() {
    static const std::unordered_set<std::string> s = {
        "expression_statement", "let_declaration",
        "if_expression", "for_expression", "while_expression",
        "loop_expression", "match_expression", "unsafe_block",
        "return_expression", "break_expression", "continue_expression"
    };
    return s;
}
const std::unordered_set<std::string>& rust_expressions() {
    static const std::unordered_set<std::string> s = {
        "binary_expression", "call_expression", "identifier",
        "integer_literal", "string_literal", "float_literal",
        "char_literal", "boolean_literal", "field_expression",
        "block", "tuple_expression", "array_expression",
        "range_expression", "unary_expression", "reference_expression",
        "try_expression", "await_expression", "macro_invocation",
        "closure_expression", "type_cast_expression",
        "assignment_expression", "compound_assignment_expr",
        "index_expression", "method_call_expression"
    };
    return s;
}

// Go
const std::unordered_set<std::string>& go_declarations() {
    static const std::unordered_set<std::string> s = {
        "function_declaration", "method_declaration", "type_declaration",
        "import_declaration", "var_declaration", "const_declaration",
        "struct_type", "interface_type"
    };
    return s;
}
const std::unordered_set<std::string>& go_statements() {
    static const std::unordered_set<std::string> s = {
        "expression_statement", "return_statement", "if_statement",
        "for_statement", "switch_statement", "select_statement",
        "go_statement", "defer_statement", "break_statement",
        "continue_statement", "fallthrough_statement", "goto_statement",
        "labeled_statement", "send_statement"
    };
    return s;
}
const std::unordered_set<std::string>& go_expressions() {
    static const std::unordered_set<std::string> s = {
        "binary_expression", "call_expression", "identifier",
        "int_literal", "float_literal", "interpreted_string_literal",
        "raw_string_literal", "rune_literal", "imaginary_literal",
        "selector_expression", "composite_literal", "unary_expression",
        "index_expression", "slice_expression", "type_assertion_expression",
        "type_conversion_expression", "parenthesized_expression",
        "func_literal", "true", "false", "nil"
    };
    return s;
}

// Java
const std::unordered_set<std::string>& java_declarations() {
    static const std::unordered_set<std::string> s = {
        "method_declaration", "class_declaration", "interface_declaration",
        "field_declaration", "constructor_declaration",
        "enum_declaration", "annotation_type_declaration",
        "record_declaration", "module_declaration",
        "import_declaration", "package_declaration"
    };
    return s;
}
const std::unordered_set<std::string>& java_statements() {
    static const std::unordered_set<std::string> s = {
        "expression_statement", "return_statement", "if_statement",
        "for_statement", "enhanced_for_statement", "while_statement",
        "do_statement", "switch_expression", "try_statement",
        "throw_statement", "try_with_resources_statement",
        "break_statement", "continue_statement", "synchronized_statement",
        "assert_statement", "local_variable_declaration",
        "labeled_statement"
    };
    return s;
}
const std::unordered_set<std::string>& java_expressions() {
    static const std::unordered_set<std::string> s = {
        "binary_expression", "method_invocation", "identifier",
        "decimal_integer_literal", "hex_integer_literal",
        "string_literal", "character_literal",
        "field_access", "object_creation_expression",
        "array_creation_expression", "array_access",
        "unary_expression", "cast_expression",
        "ternary_expression", "assignment_expression",
        "update_expression", "instanceof_expression",
        "lambda_expression", "method_reference",
        "parenthesized_expression", "class_literal",
        "this", "null_literal"
    };
    return s;
}

// Ruby
const std::unordered_set<std::string>& ruby_declarations() {
    static const std::unordered_set<std::string> s = {
        "method", "class", "module", "singleton_method",
        "singleton_class", "alias",
        "method_parameters", "parameters"
    };
    return s;
}
const std::unordered_set<std::string>& ruby_statements() {
    static const std::unordered_set<std::string> s = {
        "body_statement", "return", "if", "unless", "while", "until", "for",
        "case", "when", "begin", "rescue", "ensure", "break",
        "next", "redo", "retry", "yield"
    };
    return s;
}
const std::unordered_set<std::string>& ruby_expressions() {
    static const std::unordered_set<std::string> s = {
        "binary", "call", "identifier", "integer", "float",
        "string", "string_array", "symbol", "array", "hash",
        "unary", "if", "unless", "lambda", "block",
        "method_call", "constant", "global_variable",
        "instance_variable", "class_variable", "self",
        "true", "false", "nil", "regex", "heredoc_body",
        "interpolation", "simple_symbol", "range",
        "string_content", "argument_list"
    };
    return s;
}

// Bash
const std::unordered_set<std::string>& bash_declarations() {
    static const std::unordered_set<std::string> s = {
        "function_definition", "variable_assignment",
        "declaration_command"
    };
    return s;
}
const std::unordered_set<std::string>& bash_statements() {
    static const std::unordered_set<std::string> s = {
        "command", "if_statement", "for_statement", "while_statement",
        "case_statement", "pipeline", "list",
        "subshell", "compound_statement", "return_statement",
        "elif_clause", "else_clause"
    };
    return s;
}
const std::unordered_set<std::string>& bash_expressions() {
    static const std::unordered_set<std::string> s = {
        "word", "string", "raw_string", "ansii_c_string",
        "simple_expansion", "expansion", "command_substitution",
        "process_substitution", "arithmetic_expansion",
        "binary_expression", "unary_expression", "postfix_expression",
        "test_command", "variable_name", "concatenation",
        "redirected_statement", "file_redirect",
        "heredoc_redirect", "number",
        "command_name", "string_content",
        "variable_expansion", "command_substitution"
    };
    return s;
}

// Retrieve classification sets for a given language.
void classification_sets(TreeSitterLanguage lang,
                         const std::unordered_set<std::string>*& decls,
                         const std::unordered_set<std::string>*& stmts,
                         const std::unordered_set<std::string>*& exprs) {
    switch (lang) {
    case TreeSitterLanguage::C:           decls = &c_declarations();   stmts = &c_statements();   exprs = &c_expressions();   break;
    case TreeSitterLanguage::CPP:          decls = &cpp_declarations(); stmts = &cpp_statements(); exprs = &cpp_expressions(); break;
    case TreeSitterLanguage::Python:       decls = &python_declarations(); stmts = &python_statements(); exprs = &python_expressions(); break;
    case TreeSitterLanguage::JavaScript:   decls = &js_declarations(); stmts = &js_statements(); exprs = &js_expressions(); break;
    case TreeSitterLanguage::TypeScript:   decls = &ts_declarations(); stmts = &ts_statements(); exprs = &ts_expressions(); break;
    case TreeSitterLanguage::Rust:         decls = &rust_declarations(); stmts = &rust_statements(); exprs = &rust_expressions(); break;
    case TreeSitterLanguage::Go:           decls = &go_declarations(); stmts = &go_statements(); exprs = &go_expressions(); break;
    case TreeSitterLanguage::Java:         decls = &java_declarations(); stmts = &java_statements(); exprs = &java_expressions(); break;
    case TreeSitterLanguage::Ruby:         decls = &ruby_declarations(); stmts = &ruby_statements(); exprs = &ruby_expressions(); break;
    case TreeSitterLanguage::Bash:         decls = &bash_declarations(); stmts = &bash_statements(); exprs = &bash_expressions(); break;
    default: decls = nullptr; stmts = nullptr; exprs = nullptr; break;
    }
}

// Filter: returns true if `type` is a terminal/punctuation token that
// should NOT be reported as "unsupported". These are concrete syntax tokens
// (parentheses, braces, semicolons, operators, keywords-as-tokens) that
// appear as leaf nodes in some tree-sitter grammars.
static bool is_terminal_token(const std::string& type) {
    // Single-character punctuation
    if (type.size() == 1) {
        char c = type[0];
        if (c == '(' || c == ')' || c == '{' || c == '}' ||
            c == '[' || c == ']' || c == ';' || c == ':' ||
            c == ',' || c == '.' || c == '+' || c == '-' ||
            c == '*' || c == '/' || c == '%' || c == '&' ||
            c == '|' || c == '^' || c == '!' || c == '~' ||
            c == '<' || c == '>' || c == '=' || c == '?' ||
            c == '@' || c == '$' || c == '#' || c == '\\' ||
            c == '\"' || c == '\'') {
            return true;
        }
    }
    // Common keyword-as-token nodes (the keyword itself, not the construct)
    if (type == "def" || type == "end" || type == "do" ||
        type == "if" || type == "else" || type == "elif" ||
        type == "elsif" || type == "unless" || type == "while" ||
        type == "until" || type == "for" || type == "in" ||
        type == "begin" || type == "rescue" || type == "ensure" ||
        type == "case" || type == "when" || type == "then" ||
        type == "return" || type == "break" || type == "next" ||
        type == "class" || type == "module" || type == "struct" ||
        type == "enum" || type == "interface" || type == "implements" ||
        type == "extends" || type == "include" || type == "require") {
        return true;
    }
    // Multi-char operators as tokens
    if (type == "==" || type == "!=" || type == "<=" || type == ">=" ||
        type == "&&" || type == "||" || type == "<<" || type == ">>" ||
        type == "=>" || type == "->" || type == "::" || type == ".." ||
        type == "..." || type == "+=" || type == "-=" || type == "*=" ||
        type == "/=" || type == "**" || type == "//" || type == "%=") {
        return true;
    }
    if (type == "comment") return true;  // Comments are not meaningful for classification
    return false;
}

// Recursively walk a TSNode tree and classify every node.
// `is_root` is true only for the initial (root) call — the root node type
// is already captured in result.root_node_type, so we skip it for unsupported.
static void walk_and_classify_impl(TSNode node, bool is_root,
                       const std::unordered_set<std::string>& decls,
                       const std::unordered_set<std::string>& stmts,
                       const std::unordered_set<std::string>& exprs,
                       ParseResult& result) {
    const char* type = ts_node_type(node);
    if (!type) return;

    std::string type_str(type);

    if (decls.count(type_str)) {
        result.has_declaration = true;
    } else if (stmts.count(type_str)) {
        result.has_statement = true;
    } else if (exprs.count(type_str)) {
        result.has_expression = true;
    } else if (!is_root && !type_str.empty()
               && type_str != "ERROR" && type_str != "MISSING"
               && !is_terminal_token(type_str)) {
        // Avoid duplicates in unsupported list
        auto& unsup = result.unsupported_node_types;
        if (std::find(unsup.begin(), unsup.end(), type_str) == unsup.end()) {
            unsup.push_back(type_str);
        }
    }

    // Recurse into children
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        if (!ts_node_is_null(child)) {
            walk_and_classify_impl(child, false, decls, stmts, exprs, result);
        }
    }
}

static void walk_and_classify(TSNode root_node,
                       const std::unordered_set<std::string>& decls,
                       const std::unordered_set<std::string>& stmts,
                       const std::unordered_set<std::string>& exprs,
                       ParseResult& result) {
    walk_and_classify_impl(root_node, true, decls, stmts, exprs, result);
}

}  // namespace

// =========================================================================
// Public API
// =========================================================================

ParseResult parse(std::string_view source, TreeSitterLanguage lang) {
    ParseResult result;
    result.language = lang;

    const TSLanguage* tsl = language_ptr(lang);
    if (!tsl) {
        result.success = false;
        result.error_message = "Unknown or unsupported language";
        return result;
    }

    TSParser* parser = ts_parser_new();
    if (!parser) {
        result.success = false;
        result.error_message = "Failed to create Tree-sitter parser";
        return result;
    }

    if (!ts_parser_set_language(parser, tsl)) {
        result.success = false;
        result.error_message = "Failed to set Tree-sitter language";
        ts_parser_delete(parser);
        return result;
    }

    TSTree* tree = ts_parser_parse_string(
        parser, nullptr,
        source.data(), static_cast<uint32_t>(source.size()));
    if (!tree) {
        result.success = false;
        result.error_message = "Tree-sitter parse returned null tree";
        ts_parser_delete(parser);
        return result;
    }

    TSNode root = ts_tree_root_node(tree);
    const char* root_type = ts_node_type(root);
    if (root_type) {
        result.root_node_type = root_type;
    }

    result.success = !ts_node_is_null(root) && !ts_node_has_error(root);

    // Classification walk
    const std::unordered_set<std::string>* decls = nullptr;
    const std::unordered_set<std::string>* stmts = nullptr;
    const std::unordered_set<std::string>* exprs = nullptr;

    classification_sets(lang, decls, stmts, exprs);
    if (decls && stmts && exprs) {
        walk_and_classify(root, *decls, *stmts, *exprs, result);
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return result;
}

bool language_from_extension(std::string_view ext, TreeSitterLanguage& lang) {
    if (ext == "c")                                 { lang = TreeSitterLanguage::C; return true; }
    if (ext == "cc" || ext == "cpp" || ext == "cxx" || ext == "c++" || ext == "h" || ext == "hpp" || ext == "hxx")
                                                     { lang = TreeSitterLanguage::CPP; return true; }
    if (ext == "py" || ext == "py3" || ext == "pyw") { lang = TreeSitterLanguage::Python; return true; }
    if (ext == "js" || ext == "mjs" || ext == "cjs") { lang = TreeSitterLanguage::JavaScript; return true; }
    if (ext == "ts" || ext == "mts" || ext == "cts") { lang = TreeSitterLanguage::TypeScript; return true; }
    if (ext == "rs")                                 { lang = TreeSitterLanguage::Rust; return true; }
    if (ext == "go")                                 { lang = TreeSitterLanguage::Go; return true; }
    if (ext == "java")                               { lang = TreeSitterLanguage::Java; return true; }
    if (ext == "rb")                                 { lang = TreeSitterLanguage::Ruby; return true; }
    if (ext == "sh" || ext == "bash")                { lang = TreeSitterLanguage::Bash; return true; }
    return false;
}

const char* language_name(TreeSitterLanguage lang) {
    switch (lang) {
    case TreeSitterLanguage::C:           return "C";
    case TreeSitterLanguage::CPP:          return "C++";
    case TreeSitterLanguage::Python:       return "Python";
    case TreeSitterLanguage::JavaScript:   return "JavaScript";
    case TreeSitterLanguage::TypeScript:   return "TypeScript";
    case TreeSitterLanguage::Rust:         return "Rust";
    case TreeSitterLanguage::Go:           return "Go";
    case TreeSitterLanguage::Java:         return "Java";
    case TreeSitterLanguage::Ruby:         return "Ruby";
    case TreeSitterLanguage::Bash:         return "Bash";
    default: return "Unknown";
    }
}

}  // namespace traveler::editor
