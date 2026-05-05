#include "command/parser.h"
#include "core/dispatcher.h"
#include "tui/layout.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/screen.hpp>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

bool run_dispatcher_self_test() {
    traveler::core::Dispatcher dispatcher;
    if (dispatcher.current_mode() != traveler::core::Mode::Cockpit) {
        std::fprintf(stderr, "dispatcher default mode mismatch\n");
        return false;
    }

    // REQ-MODE-3: transitions from any mode to any mode (no modal trap)
    const auto editor = dispatcher.transition_command("/Editor");
    if (!editor.ok || editor.to != traveler::core::Mode::Editor) {
        std::fprintf(stderr, "slash transition to Editor failed\n");
        return false;
    }

    // REQ-MODE-1: mode switching preserves in-mode state
    dispatcher.set_mode_state(traveler::core::Mode::Editor, "file=README.md");
    const auto llm = dispatcher.transition_command("/LLM");
    if (!llm.ok || llm.to != traveler::core::Mode::LLM) {
        std::fprintf(stderr, "slash transition to LLM failed\n");
        return false;
    }
    if (dispatcher.mode_state(traveler::core::Mode::Editor) != "file=README.md") {
        std::fprintf(stderr, "mode-local state was not preserved\n");
        return false;
    }

    // §6.2 Slash parser: / <command> [<args>...]
    const auto parsed = traveler::command::parse_slash_command("/Pane split");
    if (!parsed.ok || parsed.command.name != "Pane" || parsed.command.args.size() != 1 ||
        parsed.command.args.front() != "split") {
        std::fprintf(stderr, "slash parser failed\n");
        return false;
    }

    // §6.2 Leader parser: space opens shortcuts, sub-key matches
    const auto leader = traveler::command::parse_leader_key('e');
    if (!leader.ok || leader.command != "Editor") {
        std::fprintf(stderr, "leader parser failed\n");
        return false;
    }

    // §6.2 @ callsign parser: @vega/@altair/@orion/@rigel autocomplete
    const auto callsigns = traveler::command::complete_callsign("ask @v");
    if (callsigns.empty() || callsigns.front() != "@vega") {
        std::fprintf(stderr, "callsign completion failed\n");
        return false;
    }

    // §6.1: all six modes reachable via slash command
    const auto pane = dispatcher.transition_command("/Pane");
    if (!pane.ok || pane.to != traveler::core::Mode::Pane) {
        std::fprintf(stderr, "slash transition to Pane failed\n");
        return false;
    }

    const auto split = dispatcher.transition_command("/Split");
    if (!split.ok || split.to != traveler::core::Mode::Split) {
        std::fprintf(stderr, "slash transition to Split failed\n");
        return false;
    }

    // §6.1: /Cockpit from Cockpit is a no-op (from == to, REQ-MODE-3)
    const auto cockpit_transition = dispatcher.transition_command("/Cockpit");
    if (!cockpit_transition.ok || cockpit_transition.to != traveler::core::Mode::Cockpit) {
        std::fprintf(stderr, "slash transition to Cockpit failed\n");
        return false;
    }
    const auto noop = dispatcher.transition_command("/Cockpit");
    if (!noop.ok || noop.from != noop.to) {
        std::fprintf(stderr, "same-mode Cockpit transition unexpected\n");
        return false;
    }

    std::printf("dispatcher self-test PASS\n");
    return true;
}

void print_layout_snapshot() {
    traveler::core::Dispatcher dispatcher;
    const auto mode = traveler::core::mode_name(dispatcher.current_mode());

    traveler::tui::LayoutModel model;
    model.mode = std::string(mode);
    model.callsigns = traveler::command::callsign_roster();
    model.stage_title = "Cockpit Stage";
    model.stage_body = "Focused callsign @vega is ready. Stage content swaps when modes change.";
    model.tower_prompt = "/Editor /LLM /Pane /Git | space leader | Ctrl+P palette";

    const auto snapshot = traveler::tui::render_layout_snapshot(model, 96, 24);
    std::fwrite(snapshot.data(), 1, snapshot.size(), stdout);
}

// Interactive TUI entrypoint: launch when no CLI flag is given.
// Uses ScreenInteractive with mouse support so ResizableSplit drag can be
// verified (PC-4). Press Escape or 'q' to exit.
void run_interactive_tui() {
    traveler::core::Dispatcher dispatcher;
    traveler::tui::LayoutState layout_state;

    traveler::tui::LayoutModel model;
    model.mode = std::string(traveler::core::mode_name(dispatcher.current_mode()));
    model.callsigns = traveler::command::callsign_roster();
    model.stage_title = "Cockpit Stage";
    model.stage_body = "Focused callsign @vega is ready. Drag separators to resize.";
    model.tower_prompt = "/Editor /LLM /Pane /Git | space leader | Ctrl+P palette";

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto layout = traveler::tui::make_layout_component(model, &layout_state);

    // Wrap in CatchAll to ensure mouse events reach ResizableSplit
    auto interactive = ftxui::CatchEvent(layout, [&](ftxui::Event event) {
        // Exit on Escape or 'q' pressed
        if (event == ftxui::Event::Escape) {
            screen.Exit();
            return true;
        }
        if (event == ftxui::Event::Character('q')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(interactive);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--version") == 0) {
        std::printf("Traveler. v0.1.0-alpha\n");
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "--self-test-dispatcher") == 0) {
        return run_dispatcher_self_test() ? 0 : 1;
    }
    if (argc > 1 && std::strcmp(argv[1], "--layout-snapshot") == 0) {
        print_layout_snapshot();
        return 0;
    }
    // Default: launch interactive TUI with mouse-enabled layout (PC-4 verifiable)
    run_interactive_tui();
    return 0;
}
