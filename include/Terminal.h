#pragma once
#include "imgui.h"
#include <deque>
#include <string>
#include <vector>

// Lightweight ANSI-aware terminal view with scrollback and input editing.
class Terminal {
public:
    Terminal();

    // Incoming data from remote shell
    void Feed(const std::string& data);

    // Clear scrollback and current line
    void Clear();
    // Full reset (styles + scrollback)
    void Reset();

    // Render inside current window.
    void Render();

    // Outgoing data typed by the user. Cleared after read.
    std::string ConsumeOutgoing();

private:
    struct Style {
        ImVec4 fg;
        ImVec4 bg;
        bool bold = false;
        bool underline = false;
        bool inverse = false;
    };

    struct Segment {
        Style style;
        std::string text;
    };

    struct Line {
        std::vector<Segment> segments;
    };

    std::deque<Line> scrollback;
    Line current_line;
    Style current_style;

    enum class State { Text, Escape, CSI, OSC } state = State::Text;
    std::string esc_buffer;

    size_t max_scrollback = 4000;

    // Input
    char input_buf[512];
    std::vector<std::string> history;
    int history_pos = -1;
    std::string outgoing;

    // Helpers
    void push_current_line();
    void append_char(char c);
    void backspace();
    void carriage_return();
    void apply_sgr(const std::vector<int>& codes);
    ImVec4 color_from_ansi(int code, bool bright) const;
    ImVec4 color_from_256(int code) const;
};
