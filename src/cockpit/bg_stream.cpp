// Reference: Traveler_Phase0_Spec_v0.1.md §9.2 (Background Streaming for Non-Focused Callsigns)
#include "cockpit/bg_stream.h"

namespace traveler::cockpit {

const char* to_string(BgStreamStatus s) {
    switch (s) {
        case BgStreamStatus::Ready:       return "ready";
        case BgStreamStatus::Running:     return "running";
        case BgStreamStatus::Done:        return "done";
        case BgStreamStatus::Error:       return "error";
        case BgStreamStatus::Interrupted: return "interrupted";
    }
    return "unknown";
}

void BackgroundStreamer::start(std::string_view callsign) {
    std::string key(callsign);
    streams_[key] = BgStreamStatus::Running;
}

void BackgroundStreamer::mark_done(std::string_view callsign) {
    std::string key(callsign);
    auto it = streams_.find(key);
    if (it != streams_.end() && it->second == BgStreamStatus::Running) {
        it->second = BgStreamStatus::Done;
    }
}

void BackgroundStreamer::mark_error(std::string_view callsign) {
    std::string key(callsign);
    auto it = streams_.find(key);
    if (it != streams_.end() && it->second == BgStreamStatus::Running) {
        it->second = BgStreamStatus::Error;
    }
}

BgStreamStatus BackgroundStreamer::status(std::string_view callsign) const {
    std::string key(callsign);
    auto it = streams_.find(key);
    if (it == streams_.end()) return BgStreamStatus::Ready;
    return it->second;
}

bool BackgroundStreamer::is_running(std::string_view callsign) const {
    return status(callsign) == BgStreamStatus::Running;
}

void BackgroundStreamer::shutdown(const std::function<void(const std::string&)>& on_interrupt) {
    for (auto& [callsign, st] : streams_) {
        if (st == BgStreamStatus::Running) {
            st = BgStreamStatus::Interrupted;
            if (on_interrupt) {
                on_interrupt(callsign);
            }
        }
    }
}

bool BackgroundStreamer::has_running() const {
    for (const auto& [callsign, st] : streams_) {
        if (st == BgStreamStatus::Running) return true;
    }
    return false;
}

}  // namespace traveler::cockpit
