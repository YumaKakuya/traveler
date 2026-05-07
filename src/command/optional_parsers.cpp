#include "command/optional_parsers.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace traveler::command {
namespace {

ModalResult modal_error(std::string msg) {
    ModalResult r;
    r.error = std::move(msg);
    return r;
}

TmuxPrefixResult tmux_error(std::string msg) {
    TmuxPrefixResult r;
    r.error = std::move(msg);
    return r;
}

std::string_view ltrim(std::string_view s) {
    while (!s.empty() && s.front() == ' ') {
        s.remove_prefix(1);
    }
    return s;
}

/// Phase 0 ex-command set: :w, :q, :wq, :e <file>, :set modal
bool is_known_modal_command(std::string_view cmd) {
    return cmd == "w" || cmd == "q" || cmd == "wq" || cmd == "e" || cmd == "set";
}

}  // namespace

ModalResult OptionalParsers::parse_modal(std::string_view input) const {
    if (!modal_enabled_) {
        return modal_error("modal-ex mode is disabled (enable with --modal or :set modal)");
    }

    if (input.empty() || input.front() != ':') {
        return modal_error("modal-ex command must start with ':'");
    }

    input.remove_prefix(1);                    // strip ':'
    input = ltrim(input);

    if (input.empty()) {
        return modal_error("modal-ex command name is empty");
    }

    std::stringstream stream{std::string(input)};
    ModalResult result;
    result.ok = true;
    stream >> result.command;

    if (result.command.empty()) {
        return modal_error("modal-ex command name is empty");
    }

    if (!is_known_modal_command(result.command)) {
        return modal_error("unknown modal-ex command: " + result.command);
    }

    std::string arg;
    while (stream >> arg) {
        result.args.push_back(arg);
    }

    return result;
}

TmuxPrefixResult OptionalParsers::parse_tmux_prefix(char key) const {
    if (!tmux_prefix_enabled_) {
        return tmux_error(
            "tmux-prefix mode is disabled (enable via config tmux_prefix = true)");
    }

    switch (key) {
        case '"':
            return TmuxPrefixResult{true, "split-horizontal", {}};
        case '%':
            return TmuxPrefixResult{true, "split-vertical", {}};
        case 'n':
            return TmuxPrefixResult{true, "next-pane", {}};
        case 'p':
            return TmuxPrefixResult{true, "prev-pane", {}};
        default:
            return tmux_error("unknown tmux-prefix key: " + std::string(1, key));
    }
}

}  // namespace traveler::command
