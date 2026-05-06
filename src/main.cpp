#include "command/parser.h"
#include "core/dispatcher.h"
#include "tui/layout.h"

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

    const auto editor = dispatcher.transition_command("/Editor");
    if (!editor.ok || editor.to != traveler::core::Mode::Editor) {
        std::fprintf(stderr, "slash transition to Editor failed\n");
        return false;
    }

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

    const auto parsed = traveler::command::parse_slash_command("/Pane split");
    if (!parsed.ok || parsed.command.name != "Pane" || parsed.command.args.size() != 1 ||
        parsed.command.args.front() != "split") {
        std::fprintf(stderr, "slash parser failed\n");
        return false;
    }

    const auto leader = traveler::command::parse_leader_key('e');
    if (!leader.ok || leader.command != "Editor") {
        std::fprintf(stderr, "leader parser failed\n");
        return false;
    }

    const auto callsigns = traveler::command::complete_callsign("ask @v");
    if (callsigns.empty() || callsigns.front() != "@vega") {
        std::fprintf(stderr, "callsign completion failed\n");
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

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        if (argc > 2) {
            std::fprintf(stderr,
                         "Usage: traveler [--version | --self-test-dispatcher | --layout-snapshot]\n");
            return 1;
        }
        if (std::strcmp(argv[1], "--version") == 0) {
            std::printf("Traveler. v0.1.0-alpha\n");
            return 0;
        }
        if (std::strcmp(argv[1], "--self-test-dispatcher") == 0) {
            return run_dispatcher_self_test() ? 0 : 1;
        }
        if (std::strcmp(argv[1], "--layout-snapshot") == 0) {
            print_layout_snapshot();
            return 0;
        }
        std::fprintf(stderr,
                     "Usage: traveler [--version | --self-test-dispatcher | --layout-snapshot]\n");
        return 1;
    }
    print_layout_snapshot();
    return 0;
}
