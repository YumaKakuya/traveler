#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace traveler::command {

/// Result of parsing a modal-ex command (":" prefix).
struct ModalResult {
    bool ok{false};
    std::string command;          // "w", "q", "wq", "e", "set"
    std::vector<std::string> args;
    std::string error;
};

/// Result of parsing a tmux-prefix key (Ctrl+B followed by a key).
struct TmuxPrefixResult {
    bool ok{false};
    std::string action;           // "split-horizontal", "split-vertical",
                                  // "next-pane", "prev-pane"
    std::string error;
};

/// Optional parsers gated by enable/disable flags.
///
/// Modal-ex (":") is disabled by default — PC-12.
/// Tmux-prefix ("<C-b>") is disabled by default — PC-12.
///
/// These are separate from the always-active slash/leader/@ parsers
/// in parser.{h,cpp} and do not modify existing parser semantics.
class OptionalParsers {
public:
    // ---- enable / disable ----

    bool modal_enabled() const noexcept { return modal_enabled_; }
    void set_modal_enabled(bool enabled) noexcept { modal_enabled_ = enabled; }

    bool tmux_prefix_enabled() const noexcept { return tmux_prefix_enabled_; }
    void set_tmux_prefix_enabled(bool enabled) noexcept {
        tmux_prefix_enabled_ = enabled;
    }

    // ---- parsing ----

    /// Parse a ":" command string.
    /// Returns ok=false with an error if modal is disabled or the command
    /// is unrecognised.
    ModalResult parse_modal(std::string_view input) const;

    /// Parse a keypress after the tmux prefix (Ctrl+B).
    /// Returns ok=false with an error if tmux prefix is disabled or the key
    /// is unrecognised.
    TmuxPrefixResult parse_tmux_prefix(char key) const;

private:
    bool modal_enabled_{false};
    bool tmux_prefix_enabled_{false};
};

}  // namespace traveler::command
