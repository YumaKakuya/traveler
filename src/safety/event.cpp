// Reference: ~/hatch-v3/packages/hatch-safety/src/collector/anonymizer.ts
// Reference: Traveler_Phase0_Spec_v0.1.md §8.2 (REQ-SAFETY-5)
#include "event.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <sstream>

namespace traveler::safety {

const char* to_string(Severity s) {
    switch (s) {
        case Severity::LOW:      return "LOW";
        case Severity::MEDIUM:   return "MEDIUM";
        case Severity::HIGH:     return "HIGH";
        case Severity::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

const char* to_string(Category c) {
    switch (c) {
        case Category::PII:      return "PII";
        case Category::SECRET:   return "SECRET";
        case Category::PATH:     return "PATH";
        case Category::DANGER:   return "DANGER";
        case Category::BUDGET:   return "BUDGET";
        case Category::INTERNAL: return "INTERNAL";
    }
    return "UNKNOWN";
}

namespace {
    std::mutex g_event_mutex;
    std::string g_event_path;
    bool g_initialized = false;

    void ensure_path() {
        if (g_initialized) return;
        g_initialized = true;

        // Use XDG state directory or fallback
        const char* xdg = std::getenv("XDG_STATE_HOME");
        std::filesystem::path base;
        if (xdg && *xdg) {
            base = std::filesystem::path(xdg) / "traveler";
        } else {
            const char* home = std::getenv("HOME");
            if (home && *home) {
                base = std::filesystem::path(home) / ".local" / "state" / "traveler";
            } else {
                base = std::filesystem::temp_directory_path() / "traveler";
            }
        }

        std::error_code ec;
        std::filesystem::create_directories(base, ec);
        g_event_path = (base / "safety_events.jsonl").string();
    }
}  // anonymous namespace

const char* safety_events_path() {
    ensure_path();
    return g_event_path.c_str();
}

void emit_safety_event(const SafetyEvent& event) {
    ensure_path();

    // Format JSON manually (no nlohmann/json dependency to keep safety module lean)
    // Phase 0: single-threaded, simple lock
    std::lock_guard<std::mutex> lock(g_event_mutex);

    auto ts = std::chrono::system_clock::to_time_t(event.timestamp);

    // Escape JSON strings
    auto escape = [](const std::string& s) -> std::string {
        std::string out;
        out.reserve(s.size() + 2);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;      break;
            }
        }
        return out;
    };

    std::ostringstream json;
    json << "{"
         << "\"severity\":\"" << to_string(event.severity) << "\","
         << "\"category\":\"" << to_string(event.category) << "\","
         << "\"timestamp\":" << ts << ","
         << "\"detail\":\"" << escape(event.detail) << "\","
         << "\"redacted_input\":\"" << escape(event.redacted_input) << "\","
         << "\"redacted_output\":\"" << escape(event.redacted_output) << "\""
         << "}\n";

    // Append to file
    FILE* f = std::fopen(g_event_path.c_str(), "a");
    if (f) {
        std::string line = json.str();
        std::fwrite(line.data(), 1, line.size(), f);
        std::fclose(f);
    }
}

}  // namespace traveler::safety
