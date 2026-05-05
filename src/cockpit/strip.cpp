// Reference: Traveler_Phase0_Spec_v0.1.md §9.1 (Cockpit Strip rendering)
#include "cockpit/strip.h"

#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>

namespace traveler::cockpit {

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

    // Build the header line: "Strip | Cockpit | @callsigns..."
    std::string header = "Strip | Cockpit";
    for (const auto& c : mounted_callsigns) {
        header += " | ";
        header += c;
    }

    // Build seat elements
    Elements seats;
    const std::size_t count = std::min(mounted_callsigns.size(), statuses.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto& callsign = mounted_callsigns[i];
        const auto& status = statuses[i];
        bool is_focused = (callsign == focused_callsign);

        auto label = format_seat(callsign, status, is_focused);
        auto seat = text(label) | border | flex;

        // Highlight focused callsign (REQ-COCKPIT-4 step 4)
        if (is_focused) {
            seat = seat | bold | inverted;
        }

        seats.push_back(std::move(seat));
    }

    // If no callsigns mounted, show placeholder
    if (seats.empty()) {
        seats.push_back(text("no callsigns mounted") | dim | flex);
    }

    return vbox({
               text(header) | bold,
               hbox(std::move(seats)) | flex,
           }) |
           border | size(HEIGHT, EQUAL, 3);
}

ftxui::Element render_strip_compact(const std::vector<std::string>& mounted_callsigns,
                                     std::string_view focused_callsign) {
    using namespace ftxui;

    std::string line;
    for (const auto& c : mounted_callsigns) {
        if (!line.empty()) line += "  ";
        if (c == focused_callsign) {
            line += "> ";
            line += c;
            line += " <";
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
