#include "editor/editor.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace traveler::editor {
namespace {

// Round bytes to a human-readable string with SI suffix.
std::string format_size(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    double v = static_cast<double>(bytes) / 1024.0;
    if (v < 1024.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f KB", v);
        return buf;
    }
    v /= 1024.0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f MB", v);
    return buf;
}

// Count newlines in [data, data+size). Returns line count (number of '\n').
int count_lines(const char* data, size_t size) {
    int count = 0;
    // Use memchr-chunked scan for speed on large files.
    const char* p = data;
    const char* end = data + size;
    while (p < end) {
        const char* nl = static_cast<const char*>(std::memchr(p, '\n', end - p));
        if (!nl) break;
        count++;
        p = nl + 1;
    }
    return count;
}

// Read entire file into a heap buffer. Returns nullptr on failure;
// fills *map_size. Portable: no POSIX-only headers required.
const char* map_file(const std::string& path, size_t* map_size, int* out_fd) {
    *out_fd = -1;  // fd unused in portable path
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return nullptr;

    std::streamsize sz = file.tellg();
    if (sz <= 0) {
        *map_size = 0;
        return nullptr;  // zero-length or unreadable file
    }
    *map_size = static_cast<size_t>(sz);

    char* buf = new (std::nothrow) char[*map_size];
    if (!buf) return nullptr;

    file.seekg(0);
    file.read(buf, static_cast<std::streamsize>(*map_size));
    if (file.fail()) {
        delete[] buf;
        return nullptr;
    }
    return buf;
}

void unmap_file(const char* data, size_t /*size*/, int /*fd*/) {
    delete[] data;
}

// Extract a single line from data starting at offset; returns the line and the
// offset just past the newline (or end).  If offset >= size, returns {"", offset}.
std::pair<std::string, size_t> extract_line(const char* data, size_t size,
                                             size_t offset) {
    if (offset >= size) return {"", offset};
    const char* start = data + offset;
    const char* nl = static_cast<const char*>(
        std::memchr(start, '\n', size - offset));
    size_t len;
    if (nl) {
        len = static_cast<size_t>(nl - start);
        // omit trailing \r for Windows files
        if (len > 0 && start[len - 1] == '\r') len--;
        return {std::string(start, len), offset + (nl - start) + 1};
    }
    // Last line (no trailing newline)
    len = size - offset;
    if (len > 0 && start[len - 1] == '\r') len--;
    return {std::string(start, len), size};
}

}  // namespace

FileMeta open_metadata(const std::string& path) {
    FileMeta meta;
    meta.path = path;

    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        meta.exists = false;
        return meta;
    }
    meta.size_bytes = std::filesystem::file_size(path, ec);
    if (ec) {
        meta.exists = false;
        return meta;
    }
    meta.exists = true;

    // Map file to count lines quickly.
    int fd = -1;
    size_t map_size = 0;
    const char* data = map_file(path, &map_size, &fd);
    if (data != nullptr && map_size > 0) {
        meta.line_count = count_lines(data, map_size);
        unmap_file(data, map_size, fd);
    } else {
        // Fallback: count lines from file-size heuristic (assume ~60 chars/line)
        if (meta.size_bytes > 0) {
            meta.line_count = static_cast<int>(meta.size_bytes / 60);
        } else {
            meta.line_count = 0;
        }
    }

    return meta;
}

ViewportLines read_viewport(const std::string& path, int max_lines, int start_line) {
    ViewportLines vp;
    vp.start_line = start_line;

    int fd = -1;
    size_t map_size = 0;
    const char* data = map_file(path, &map_size, &fd);
    if (data == nullptr) {
        // Map failed or empty file.
        return vp;
    }

    // Count total lines.
    vp.total_lines = count_lines(data, map_size);

    // Skip to start_line.
    size_t offset = 0;
    for (int i = 0; i < start_line && offset < map_size; i++) {
        const char* nl = static_cast<const char*>(
            std::memchr(data + offset, '\n', map_size - offset));
        if (nl) {
            offset = static_cast<size_t>(nl - data) + 1;
        } else {
            offset = map_size;  // past end
            break;
        }
    }

    // Read up to max_lines.
    for (int i = 0; i < max_lines && offset < map_size; i++) {
        auto [line, next_offset] = extract_line(data, map_size, offset);
        vp.lines.push_back(std::move(line));
        offset = next_offset;
    }

    unmap_file(data, map_size, fd);
    return vp;
}

Breadcrumb get_breadcrumb(const std::string& path) {
    Breadcrumb bc;
    std::filesystem::path p(path);
    bc.file_name = p.filename().string();
    bc.dir_path = p.parent_path().string();
    if (bc.dir_path.empty()) bc.dir_path = ".";

    auto meta = open_metadata(path);
    if (meta.exists) {
        std::ostringstream oss;
        oss << bc.dir_path << "/" << bc.file_name
            << "  (" << format_size(meta.size_bytes) << ", "
            << meta.line_count << " lines)";
        bc.display = oss.str();
    } else {
        bc.display = path + "  (not found)";
    }
    return bc;
}

std::string folder_tree(const std::string& dir_path, int depth) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir_path, ec) || ec) {
        return dir_path + "  (not a directory)";
    }

    std::ostringstream oss;
    oss << std::filesystem::path(dir_path).filename().string() << "/\n";

    if (depth <= 0) return oss.str();

    std::vector<std::filesystem::directory_entry> entries;
    for (auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        entries.push_back(entry);
    }
    // Sort: directories first, then files, both alphabetically.
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) {
                  bool ad = a.is_directory();
                  bool bd = b.is_directory();
                  if (ad != bd) return ad > bd;
                  return a.path().filename() < b.path().filename();
              });

    for (const auto& entry : entries) {
        const auto& name = entry.path().filename().string();
        if (entry.is_directory()) {
            oss << "  " << name << "/\n";
        } else {
            // Add file size hint for larger files.
            auto sz = entry.file_size(ec);
            if (!ec && sz > 1024 * 1024) {
                oss << "  " << name << "  (" << format_size(sz) << ")\n";
            } else {
                oss << "  " << name << "\n";
            }
        }
    }
    return oss.str();
}

}  // namespace traveler::editor
