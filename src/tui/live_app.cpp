#include "tui/live_app.h"
#include "command/parser.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <memory>

namespace traveler::tui {

using namespace ftxui;

// ---------------------------------------------------------------------------
// Self-contained layout rendering (the layout.cpp helpers are anonymous-ns
// private, so we inline a minimal version here).
// ---------------------------------------------------------------------------

static Element make_strip(const LayoutModel& m, const LayoutState& s) {
    auto bp = s.current_breakpoint;
    std::string header = "Strip | " + m.mode + " | " + breakpoint_name(bp);
    Elements rows;
    rows.push_back(text(header) | bold);
    if (bp >= Breakpoint::Mid) {
        Elements seats;
        for (const auto& cs : m.callsigns) {
            seats.push_back(text(" " + cs + " ready ") | border | flex);
        }
        if (!seats.empty()) rows.push_back(hbox(std::move(seats)) | flex);
    } else {
        std::string summary;
        for (const auto& cs : m.callsigns) {
            if (!summary.empty()) summary += " ";
            summary += cs;
        }
        rows.push_back(text(" " + summary) | dim);
    }
    return vbox(std::move(rows)) | border
           | size(HEIGHT, EQUAL, std::max(3, s.strip_height));
}

static Element make_stage(const LayoutModel& m) {
    return vbox({
               text("Stage | " + m.stage_title) | bold,
               separator(),
               paragraph(m.stage_body) | flex,
           })
           | border | flex;
}

static Element make_tower(const LayoutModel& m, int height) {
    return vbox({
               text("Tower") | bold,
               separator(),
               text("> " + m.tower_prompt) | dim,
           })
           | border | size(HEIGHT, EQUAL, std::max(5, height));
}

// ---------------------------------------------------------------------------
// Helper: apply a mode transition to the shared layout model.
// ---------------------------------------------------------------------------

static bool apply_mode_switch(LiveAppState& st, std::string_view slash_cmd) {
    auto result = st.dispatcher->transition_command(slash_cmd);
    if (!result.ok) return false;

    auto mode_str = std::string(core::mode_name(result.to));
    st.layout_model->mode = mode_str;
    st.layout_model->stage_title = mode_str + " Stage";
    st.layout_model->stage_body
        = "Mode switched to " + mode_str
          + ".  Type /Editor /LLM /Pane /Git /Cockpit.";
    st.layout_model->tower_prompt
        = "/Editor /LLM /Pane /Git /Cockpit | Ctrl+P palette";
    return true;
}

// ---------------------------------------------------------------------------
// make_live_tui_component
// ---------------------------------------------------------------------------

ftxui::Component make_live_tui_component(LiveAppState& st) {
    // --- Input ---
    InputOption input_opt;
    input_opt.on_enter = [&st] {
        const auto& buf = *st.input_buffer;
        if (buf.size() >= 2 && buf[0] == '/') {
            apply_mode_switch(st, buf);
        }
        st.input_buffer->clear();
        st.autocomplete.clear();
    };
    input_opt.on_change = [&st] {
        if (!st.input_buffer->empty()) {
            st.autocomplete.compute(*st.input_buffer);
        } else {
            st.autocomplete.clear();
        }
    };
    auto input_comp = Input(st.input_buffer.get(), "> ", input_opt);

    // --- Layout renderers (strip, stage, tower) ---
    auto strip_comp = Renderer([&st] {
        st.layout_state->current_breakpoint
            = breakpoint_for_columns(Terminal::Size().dimx);
        return make_strip(*st.layout_model, *st.layout_state);
    });

    auto stage_comp = Renderer([&st] {
        return make_stage(*st.layout_model);
    });

    auto tower_comp = Renderer([&st] {
        return make_tower(*st.layout_model, st.layout_state->tower_height);
    });

    // --- ResizableSplit chain (PC-4: drag-resize) ---
    auto stage_tower = ResizableSplitBottom(
        tower_comp, stage_comp, &st.layout_state->tower_height);

    auto strip_stage_tower = ResizableSplitTop(
        strip_comp, stage_tower, &st.layout_state->strip_height);

    // Input bar sits below the layout
    auto main_area = ResizableSplitBottom(
        input_comp, strip_stage_tower, &st.layout_state->tower_height);

    // --- Event handling ---
    auto event_handler = CatchEvent(main_area, [&st](Event event) {
        // Esc: close overlays / clear input
        if (event == Event::Escape) {
            if (st.show_cheatsheet) { st.show_cheatsheet = false; return true; }
            if (st.input_buffer->empty()) {
                if (st.exit_loop) st.exit_loop();
                return true;
            }
            st.input_buffer->clear();
            st.autocomplete.clear();
            return true;
        }

        if (event == Event::CtrlC) {
            if (st.exit_loop) st.exit_loop();
            return true;
        }

        // Tab: accept top autocomplete
        if (event == Event::Tab && st.autocomplete.is_visible()
            && st.autocomplete.result_count() > 0) {
            auto at_pos = st.input_buffer->find('@');
            if (at_pos != std::string::npos) {
                *st.input_buffer = st.input_buffer->substr(0, at_pos)
                                   + std::string(st.autocomplete.top_callsign());
            }
            st.autocomplete.clear();
            return true;
        }

        // Space on empty input = leader cheatsheet
        if (event.is_character() && event.character() == " "
            && st.input_buffer->empty() && !st.show_cheatsheet) {
            st.show_cheatsheet = true;
            return true;
        }

        // Cheatsheet key dispatch
        if (st.show_cheatsheet && event.is_character()
            && event.character().size() == 1) {
            char k = event.character()[0];
            const auto* entry = st.cheatsheet.find(k);
            st.show_cheatsheet = false;
            if (entry) apply_mode_switch(st, "/" + entry->command);
            return true;
        }

        // Ctrl+P: command palette hint
        if (event.input().size() == 1 && event.input()[0] == '\x10') {
            st.layout_model->stage_body
                = "[Command Palette]\n"
                  "  /Editor  Open Editor mode\n"
                  "  /LLM     Open LLM mode\n"
                  "  /Cockpit Open Cockpit mode\n"
                  "  /Pane    Open Pane mode\n"
                  "  /Git     Open Git mode\n"
                  "Press Esc to dismiss.";
            return true;
        }

        return false;
    });

    // --- Final renderer with overlays ---
    auto final_comp = Renderer(event_handler, [&st, event_handler] {
        Element base = event_handler->Render() | border;

        // Leader cheatsheet overlay (PC-2)
        if (st.show_cheatsheet) {
            Elements rows;
            rows.push_back(text(" Leader Cheatsheet ")
                           | bold | center | color(Color::Cyan));
            rows.push_back(separator());
            for (const auto& e : st.cheatsheet.entries) {
                rows.push_back(text("  " + std::string(1, e.key)
                                    + "   " + e.display_name));
            }
            rows.push_back(separator());
            rows.push_back(text(" Press a key, or Esc to dismiss ")
                           | dim | center);
            base = dbox({base, vbox(std::move(rows)) | border
                                  | clear_under | center});
        }

        // @ autocomplete overlay (PC-3)
        if (st.autocomplete.is_visible()
            && st.autocomplete.result_count() > 0) {
            Elements rows;
            rows.push_back(text(" Callsigns ") | bold | center
                           | color(Color::GreenLight));
            rows.push_back(separator());
            for (const auto& r : st.autocomplete.results) {
                auto s = r.exact_prefix ? color(Color::GreenLight)
                                        : color(Color::White);
                rows.push_back(text("  " + r.callsign) | s);
            }
            rows.push_back(separator());
            rows.push_back(text(" Tab=accept  Esc=dismiss ") | dim | center);
            base = dbox({base, vbox(std::move(rows)) | border | clear_under
                                  | align_right
                                  | size(WIDTH, GREATER_THAN, 28)});
        }

        return base;
    });

    return final_comp;
}

// ---------------------------------------------------------------------------
// run_live_tui — entry point
// ---------------------------------------------------------------------------

int run_live_tui() {
    LiveAppState st;

    st.layout_model = std::make_shared<LayoutModel>();
    st.layout_state = std::make_shared<LayoutState>();
    st.dispatcher = std::make_shared<core::Dispatcher>();
    st.input_buffer = std::make_shared<std::string>();

    st.layout_model->mode = "Cockpit";
    st.layout_model->callsigns = command::callsign_roster();
    st.layout_model->stage_title = "Cockpit Stage";
    st.layout_model->stage_body
        = "  @vega mounted and ready.\n"
          "  Type /Editor /LLM /Pane /Git /Cockpit for mode switch.\n"
          "  SPACE = leader cheatsheet  |  @ = callsign mention\n"
          "  Ctrl+P = palette  |  Esc = clear  |  Mouse drag = resize.";
    st.layout_model->tower_prompt
        = "/Editor /LLM /Pane /Git /Cockpit | Ctrl+P palette";

    st.layout_state->strip_height = 3;
    st.layout_state->tower_height = 5;

    st.cheatsheet.load_defaults();

    auto screen = ScreenInteractive::Fullscreen();
    st.exit_loop = screen.ExitLoopClosure();

    auto component = make_live_tui_component(st);
    screen.Loop(component);
    return 0;
}

}  // namespace traveler::tui

// ---------------------------------------------------------------------------
// Standalone binary entry point
// ---------------------------------------------------------------------------

int main() {
    return traveler::tui::run_live_tui();
}
