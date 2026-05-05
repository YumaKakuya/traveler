// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 (Cockpit Strip rendering)
#include "cockpit/strip.h"

#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace traveler::cockpit {

namespace {

// Fixed 4-seat callsign roster (Phase 0 Non-Goals #6: 4-roster fixed).
constexpr std::array<const char*, 4> kSeatNames = {
    "@vega", "@altair", "@orion", "@rigel"
};

// Look up the status string for a callsign from the parallel vectors.
// Returns empty string if the callsign is not found.
std::string status_for(const std::vector<std::string>& callsigns,
                       const std::vector<std::string>& statuses,
                       std::string_view target) {
    auto it = std::find(callsigns.begin(), callsigns.end(), target);
    if (it != callsigns.end()) {
        auto idx = static_cast<std::size_t>(std::distance(callsigns.begin(), it));
        if (idx < statuses.size()) return statuses[idx];
    }
    return {};
}

}  // namespace

std::string format_seat(std::string_view callsign,
                         std::string_view status,
                         bool focused) {
    std::string result;
    if (focused) {
        result += "[";
    }
    result += callsign;
    if (!status.empty()) {
        result += ":";
        result += status;
    }
    if (focused) {
        result += "]";
    }
    return result;
}

ftxui::Element render_strip(const std::vector<std::string>& mounted_callsigns,
                             std::string_view focused_callsign,
                             const std::vector<std::string>& statuses) {
    using namespace ftxui;

    Elements seats;

    // REQ-COCKPIT-2: Render exactly 4 fixed-position seats.
    // Mounted callsigns fill their respective named seats; unmounted
    // seats display as dim.
    for (const auto& seat_name : kSeatNames) {
        bool is_mounted = std::find(mounted_callsigns.begin(),
                                     mounted_callsigns.end(),
                                     seat_name) != mounted_callsigns.end();
        bool is_focused = (seat_name == focused_callsign);

        Element seat;

        if (!is_mounted) {
            // Unmounted seat — dim/gray, no border decoration
            seat = text(seat_name) | dim;
        } else if (is_focused) {
            // REQ-COCKPIT-4 step 4: focused callsign is highlighted
            auto status_str = status_for(mounted_callsigns, statuses, seat_name);
            seat = text(format_seat(seat_name, status_str, true))
                   | bold | inverted;
        } else {
            // Mounted, non-focused — normal text
            auto status_str = status_for(mounted_callsigns, statuses, seat_name);
            seat = text(format_seat(seat_name, status_str, false));
        }

        seat = seat | border | flex;
        seats.push_back(std::move(seat));
    }

    return vbox({
               text("Strip | Cockpit") | bold,
               hbox(std::move(seats)) | flex,
           }) |
           border | size(HEIGHT, EQUAL, 3);
}

ftxui::Element render_strip_compact(const std::vector<std::string>& mounted_callsigns,
                                     std::string_view focused_callsign) {
    using namespace ftxui;

    std::string line;
    for (const auto& c : kSeatNames) {
        bool is_mounted = std::find(mounted_callsigns.begin(),
                                     mounted_callsigns.end(),
                                     c) != mounted_callsigns.end();
        if (!line.empty()) line += "  ";
        if (c == focused_callsign) {
            line += "> ";
            line += c;
            line += " <";
        } else if (!is_mounted) {
            line += "(";
            line += c;
            line += ")";
        } else {
            line += c;
        }
    }

    if (line.empty()) {
        line = "-- no callsigns --";
    }

    return text(line) | bold | border | size(HEIGHT, EQUAL, 1);
}

}  // namespace traveler::cockpit
