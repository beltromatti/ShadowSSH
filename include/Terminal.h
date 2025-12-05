#pragma once
#include "imgui.h"
#include <deque>
#include <string>
#include <vector>
#include <optional>

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
    std::deque<std::string> plain;
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
    bool bracketed_paste = false;

    // Selection
    bool selecting = false;
    std::optional<ImVec2> select_start;
    std::optional<ImVec2> select_end;

    // Helpers
    void push_current_line();
    void append_char(char c);
    void backspace();
    void carriage_return();
    void apply_sgr(const std::vector<int>& codes);
    ImVec4 color_from_ansi(int code, bool bright) const;
    ImVec4 color_from_256(int code) const;
    std::string strip_bracketed(const std::string& data);
    std::string get_plain_text(const Line& line) const;
    bool has_selection() const;
    void clear_selection();
    void copy_selection_to_clipboard(const ImVec2& origin, float line_height, const std::vector<std::string>& lines);
    int point_to_col(const std::string& text, float x) const;
};
