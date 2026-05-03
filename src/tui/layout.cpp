#include "tui/layout.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace traveler::tui {
namespace {

ftxui::Element strip_element(const LayoutModel& model, int height) {
    ftxui::Elements seats;
    const auto& callsigns = model.callsigns.empty() ? LayoutModel{}.callsigns : model.callsigns;
    std::string summary;
    for (const auto& callsign : callsigns) {
        if (!summary.empty()) {
            summary += " ";
        }
        summary += callsign;
        seats.push_back(ftxui::text(callsign + " ready") | ftxui::border | ftxui::flex);
    }

    return ftxui::vbox({
               ftxui::text("Strip | " + model.mode + " | " + summary) | ftxui::bold,
               ftxui::hbox(std::move(seats)) | ftxui::flex,
           }) |
           ftxui::border | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, std::max(3, height));
}

ftxui::Element stage_element(const LayoutModel& model) {
    return ftxui::vbox({
               ftxui::text("Stage | " + model.stage_title) | ftxui::bold,
               ftxui::separator(),
               ftxui::paragraph(model.stage_body) | ftxui::flex,
           }) |
           ftxui::border | ftxui::flex;
}

ftxui::Element tower_element(const LayoutModel& model, int height) {
    return ftxui::vbox({
               ftxui::text("Tower | universal command surface") | ftxui::bold,
               ftxui::separator(),
               ftxui::text("> " + model.tower_prompt),
           }) |
           ftxui::border | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, std::max(5, height));
}

ftxui::Element layout_element(const LayoutModel& model, const LayoutState& state) {
    return ftxui::vbox({
               strip_element(model, state.strip_height),
               stage_element(model),
               tower_element(model, state.tower_height),
           }) |
           ftxui::border;
}

std::shared_ptr<LayoutState> shared_state(LayoutState* state) {
    if (state == nullptr) {
        return std::make_shared<LayoutState>();
    }
    return {state, [](LayoutState*) {}};
}

}  // namespace

Breakpoint breakpoint_for_columns(int columns) {
    if (columns >= 1100) {
        return Breakpoint::Wide;
    }
    if (columns >= 800) {
        return Breakpoint::Mid;
    }
    if (columns >= 600) {
        return Breakpoint::Narrow;
    }
    return Breakpoint::Tiny;
}

std::string breakpoint_name(Breakpoint breakpoint) {
    switch (breakpoint) {
        case Breakpoint::Wide:
            return "wide";
        case Breakpoint::Mid:
            return "mid";
        case Breakpoint::Narrow:
            return "narrow";
        case Breakpoint::Tiny:
            return "tiny";
    }
    return "tiny";
}

ftxui::Component make_layout_component(LayoutModel model, LayoutState* state) {
    auto model_ptr = std::make_shared<LayoutModel>(std::move(model));
    auto state_ptr = shared_state(state);

    auto strip = ftxui::Renderer([model_ptr, state_ptr] {
        return strip_element(*model_ptr, state_ptr->strip_height);
    });
    auto stage = ftxui::Renderer([model_ptr] { return stage_element(*model_ptr); });
    auto tower = ftxui::Renderer([model_ptr, state_ptr] {
        return tower_element(*model_ptr, state_ptr->tower_height);
    });

    auto stage_tower = ftxui::ResizableSplitBottom(tower, stage, &state_ptr->tower_height);
    return ftxui::ResizableSplitTop(strip, stage_tower, &state_ptr->strip_height);
}

std::string render_layout_snapshot(const LayoutModel& model, int columns, int rows, LayoutState state) {
    auto root = layout_element(model, state);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(std::max(40, columns)),
                                        ftxui::Dimension::Fixed(std::max(12, rows)));
    ftxui::Render(screen, root);
    return screen.ToString();
}

}  // namespace traveler::tui
