#pragma once
// GATE-P0-2 Editor Mode Substrate — P0-2 PC-1 / PC-5
// Lane B1: Editor minimal open + large-file viewport evidence
// No tree-sitter, no live TUI integration. API is testable and minimal.
#include <cstdint>
#include <string>
#include <vector>

namespace traveler::editor {

struct FileMeta {
    std::string path;
    uint64_t size_bytes = 0;
    int line_count = 0;     // approximate (newline-scan)
    bool exists = false;
};

struct ViewportLines {
    std::vector<std::string> lines;
    int start_line = 0;     // 0-based
    int total_lines = 0;    // file line count
};

struct Breadcrumb {
    std::string display;    // "src/editor/editor.cpp  (4.2 KB, 142 lines)"
    std::string file_name;  // "editor.cpp"
    std::string dir_path;   // "src/editor"
};

// Open a file path and capture metadata (size, line count).
// Target: < 500 ms for 1.2 MB files (PC-5).
FileMeta open_metadata(const std::string& path);

// Read viewport lines (first N visible lines) without full-buffer parse.
// Uses mmap + newline-scan; O(file_size) for total line count but returns
// viewport lines from the requested start offset immediately.
ViewportLines read_viewport(const std::string& path,
                            int max_lines = 50,
                            int start_line = 0);

// Produce a breadcrumb string for Strip display.
// Format: "<dir_path>/<file_name>  (<size>, <lines> lines)"
Breadcrumb get_breadcrumb(const std::string& path);

// Produce a simple folder tree string (one level, directory listing).
// Returns newline-separated entries with indentation.
// depth=0 returns only the directory name; depth=1 adds immediate children.
std::string folder_tree(const std::string& dir_path, int depth = 1);

}  // namespace traveler::editor
