#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

int main() {
    int split_position = 20;

    auto left_pane = Renderer([] {
        return vbox({
            text("Left Pane") | bold | center,
            filler(),
            text("Drag the separator ->") | dim | center,
            filler(),
        }) | bgcolor(Color::Blue);
    });

    auto right_pane = Renderer([] {
        return vbox({
            text("Right Pane") | bold | center,
            filler(),
            text("<- Drag the separator") | dim | center,
            filler(),
        }) | bgcolor(Color::Green);
    });

    auto split = ResizableSplitLeft(left_pane, right_pane, &split_position);

    auto screen = ScreenInteractive::Fullscreen();
    screen.Loop(split);

    return 0;
}
