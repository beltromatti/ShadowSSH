#pragma once
#include "imgui.h"
#include <vterm.h>
#include <deque>
#include <string>
#include <vector>
#include <optional>

// Terminal wraps libvterm to emulate a modern VT with scrollback and ImGui rendering.
class Terminal {
public:
    Terminal(int cols = 100, int rows = 32);
    ~Terminal();

    void Resize(int cols, int rows);

    // Feed remote output into the emulator.
    void Feed(const std::string& data);

    // Render terminal contents inside current ImGui window.
    void Render();

    // Bytes to send upstream (keys typed by the user).
    std::string ConsumeOutgoing();

    void Reset();
    void ClearScrollback();

private:
    struct Cell {
        uint32_t codepoint = ' ';
        VTermColor fg;
        VTermColor bg;
        bool bold = false;
        bool underline = false;
        bool reverse = false;
    };

    struct Line {
        std::vector<Cell> cells;
    };

    VTerm* vt = nullptr;
    VTermScreen* screen = nullptr;
    VTermState* state = nullptr;
    int cols;
    int rows;
    VTermPos cursor_pos{0,0};

    std::deque<Line> scrollback;
    size_t max_scrollback = 4000;

    std::string outgoing;

    // Selection
    struct SelPos { int line = 0; int col = 0; };
    bool selecting = false;
    std::optional<SelPos> sel_start;
    std::optional<SelPos> sel_end;

    // State helpers
    void init_vterm();
    static void write_callback(const char* s, size_t len, void* user);
    static int damage_callback(VTermRect rect, void* user);
    static int moverect_callback(VTermRect dest, VTermRect src, void* user);
    static int movecursor_callback(VTermPos pos, VTermPos oldpos, int visible, void* user);
    static int settermprop_callback(VTermProp prop, VTermValue* val, void* user);
    static int bell_callback(void* user);
    static int resize_callback(int rows, int cols, void* user);
    static int sb_pushline_callback(int cols, const VTermScreenCell* cells, void* user);
    static int sb_popline_callback(int cols, VTermScreenCell* cells, void* user);

    // Rendering helpers
    void rebuild_screen(std::vector<Line>& lines_out, std::vector<std::string>& plain_out);
    ImVec4 color_to_vec(const VTermColor& c) const;
    std::string cell_text(const Cell& cell) const;
    bool has_selection() const;
    void clear_selection();
    void copy_selection_to_clipboard(const ImVec2& origin, float line_height, const std::vector<std::string>& lines);
    int point_to_col(const std::string& text, float x) const;
    SelPos mouse_to_pos(const ImVec2& mouse, const ImVec2& origin, float line_height, const std::vector<std::string>& lines) const;
    void paste_clipboard();
    void send_control_char(char c);

    void handle_input();
};
