#pragma once

#include "core/dispatcher.h"
#include "tui/layout.h"
#include "tui/surface_model.h"

#include <ftxui/component/component_base.hpp>

#include <functional>
#include <memory>
#include <string>

namespace traveler::tui {

// LiveAppState owns all mutable state for the interactive TUI ScreenLoop.
// The FTXUI components render from these shared pointers so that external
// mutations (mode switch, input, cheatsheet toggle) trigger re-render.

struct LiveAppState {
    // Layout (Strip + Stage + Tower)
    std::shared_ptr<LayoutModel> layout_model;
    std::shared_ptr<LayoutState> layout_state;

    // Mode dispatcher
    std::shared_ptr<core::Dispatcher> dispatcher;

    // Tower input buffer (shared with ftxui::Input component)
    std::shared_ptr<std::string> input_buffer;

    // Leader cheatsheet
    LeaderCheatsheet cheatsheet;
    bool show_cheatsheet{false};

    // @ autocomplete
    AutocompleteModel autocomplete;

    // Set by run_live_tui() so event handlers can leave ScreenLoop cleanly.
    std::function<void()> exit_loop;
};

/// Build the full Strip+Stage+Tower FTXUI component tree using the shared
/// app state.  The returned component includes ResizableSplit separators
/// (PC-4 drag-resize) and event handling for mode switching (PC-1),
/// leader cheatsheet (PC-2), and @ autocomplete (PC-3).
ftxui::Component make_live_tui_component(LiveAppState& st);

/// Launch the interactive FTXUI ScreenLoop in fullscreen mode.
/// Returns 0 on clean exit.
int run_live_tui();

}  // namespace traveler::tui
