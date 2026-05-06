#pragma once

#include <ftxui/component/component_base.hpp>

#include <string>
#include <vector>

namespace traveler::tui {

enum class Breakpoint {
    Wide,
    Mid,
    Narrow,
    Tiny,
};

struct LayoutModel {
    std::string mode{"Cockpit"};
    std::vector<std::string> callsigns{"@vega"};
    std::string stage_title{"Cockpit Stage"};
    std::string stage_body{"Focused callsign stream will render here."};
    std::string tower_prompt{"/Editor, /LLM, /Pane, /Git, Ctrl+P palette"};
};

struct LayoutState {
    int strip_height{3};
    int tower_height{5};
    Breakpoint current_breakpoint{Breakpoint::Narrow};
};

Breakpoint breakpoint_for_columns(int columns);
std::string breakpoint_name(Breakpoint breakpoint);

ftxui::Component make_layout_component(LayoutModel model, LayoutState* state = nullptr);

std::string render_layout_snapshot(const LayoutModel& model,
                                   int columns = 80,
                                   int rows = 24,
                                   LayoutState state = {});

}  // namespace traveler::tui
