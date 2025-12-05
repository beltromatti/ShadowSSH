#include "Terminal.h"
#include <imgui_internal.h>
#include <cstring>
#include <algorithm>

namespace {

std::string utf8_from_codepoint(uint32_t cp) {
    char buf[5] = {0};
    if (cp < 0x80) {
        buf[0] = (char)cp;
    } else if (cp < 0x800) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
    } else {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
    }
    return std::string(buf);
}

VTermModifier imgui_mods() {
    ImGuiIO& io = ImGui::GetIO();
    int mods = 0;
    if (io.KeyShift) mods |= VTERM_MOD_SHIFT;
    if (io.KeyCtrl) mods |= VTERM_MOD_CTRL;
    if (io.KeyAlt || io.KeySuper) mods |= VTERM_MOD_ALT; // map Super to Alt/Meta
    return (VTermModifier)mods;
}

} // namespace

Terminal::Terminal(int c, int r) : cols(c), rows(r) {
    init_vterm();
}

Terminal::~Terminal() {
    if (vt) vterm_free(vt);
}

void Terminal::init_vterm() {
    if (vt) {
        vterm_free(vt);
        vt = nullptr;
    }
    vt = vterm_new(rows, cols);
    vterm_set_utf8(vt, 1);
    vterm_output_set_callback(vt, &Terminal::write_callback, this);

    screen = vterm_obtain_screen(vt);
    state = vterm_obtain_state(vt);
    static VTermScreenCallbacks cbs;
    cbs.damage = &Terminal::damage_callback;
    cbs.moverect = &Terminal::moverect_callback;
    cbs.movecursor = &Terminal::movecursor_callback;
    cbs.settermprop = &Terminal::settermprop_callback;
    cbs.bell = &Terminal::bell_callback;
    cbs.resize = &Terminal::resize_callback;
    cbs.sb_pushline = &Terminal::sb_pushline_callback;
    cbs.sb_popline = &Terminal::sb_popline_callback;
    vterm_screen_set_callbacks(screen, &cbs, this);
    vterm_screen_enable_altscreen(screen, 1);
    vterm_screen_reset(screen, 1);
}

void Terminal::Resize(int c, int r) {
    cols = c;
    rows = r;
    vterm_set_size(vt, rows, cols);
}

void Terminal::Feed(const std::string& data) {
    vterm_input_write(vt, data.data(), data.size());
}

std::string Terminal::ConsumeOutgoing() {
    std::string out = outgoing;
    outgoing.clear();
    return out;
}

void Terminal::Reset() {
    scrollback.clear();
    vterm_screen_reset(screen, 1);
}

void Terminal::ClearScrollback() {
    scrollback.clear();
}

// Callbacks
void Terminal::write_callback(const char* s, size_t len, void* user) {
    auto* t = static_cast<Terminal*>(user);
    t->outgoing.append(s, len);
}

int Terminal::damage_callback(VTermRect, void*) { return 1; }
int Terminal::moverect_callback(VTermRect, VTermRect, void*) { return 1; }
int Terminal::movecursor_callback(VTermPos, VTermPos, int, void*) { return 1; }
int Terminal::settermprop_callback(VTermProp, VTermValue*, void*) { return 1; }
int Terminal::bell_callback(void*) { return 1; }
int Terminal::resize_callback(int rows, int cols, void* user) {
    auto* t = static_cast<Terminal*>(user);
    t->rows = rows;
    t->cols = cols;
    return 1;
}

int Terminal::sb_pushline_callback(int cols, const VTermScreenCell* cells, void* user) {
    auto* t = static_cast<Terminal*>(user);
    Line line;
    line.cells.reserve(cols);
    for (int i = 0; i < cols; ++i) {
        Cell c;
        c.codepoint = cells[i].chars[0];
        c.fg = cells[i].fg;
        c.bg = cells[i].bg;
        c.bold = cells[i].attrs.bold;
        c.underline = cells[i].attrs.underline != 0;
        c.reverse = cells[i].attrs.reverse;
        line.cells.push_back(c);
    }
    t->scrollback.push_back(std::move(line));
    if (t->scrollback.size() > t->max_scrollback) t->scrollback.pop_front();
    return 1;
}

int Terminal::sb_popline_callback(int, VTermScreenCell*, void* user) {
    auto* t = static_cast<Terminal*>(user);
    if (!t->scrollback.empty()) t->scrollback.pop_front();
    return 1;
}

ImVec4 Terminal::color_to_vec(const VTermColor& c_in) const {
    VTermColor c = c_in;
    vterm_screen_convert_color_to_rgb(screen, &c);
    return ImVec4(c.rgb.red / 255.0f, c.rgb.green / 255.0f, c.rgb.blue / 255.0f, 1.0f);
}

static bool colors_equal(VTermScreen* screen, const VTermColor& a_in, const VTermColor& b_in) {
    VTermColor a = a_in, b = b_in;
    vterm_screen_convert_color_to_rgb(screen, &a);
    vterm_screen_convert_color_to_rgb(screen, &b);
    return a.rgb.red == b.rgb.red && a.rgb.green == b.rgb.green && a.rgb.blue == b.rgb.blue;
}

std::string Terminal::cell_text(const Cell& cell) const {
    if (cell.codepoint == 0 || cell.codepoint == 0x20) return " ";
    return utf8_from_codepoint(cell.codepoint);
}

bool Terminal::has_selection() const {
    return sel_start.has_value() && sel_end.has_value();
}

void Terminal::clear_selection() {
    selecting = false;
    sel_start.reset();
    sel_end.reset();
}

int Terminal::point_to_col(const std::string& text, float x) const {
    float acc = 0.0f;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = text[i];
        size_t adv = 1;
        if ((c & 0xE0) == 0xC0) adv = 2;
        else if ((c & 0xF0) == 0xE0) adv = 3;
        else if ((c & 0xF8) == 0xF0) adv = 4;
        float w = ImGui::CalcTextSize(text.substr(i, adv).c_str()).x;
        if (acc + w * 0.5f >= x) return (int)i;
        acc += w;
        i += adv;
    }
    return (int)text.size();
}

static int trimmed_len(const std::string& s) {
    int len = (int)s.size();
    while (len > 0 && std::isspace((unsigned char)s[len-1])) len--;
    return len;
}

Terminal::SelPos Terminal::mouse_to_pos(const ImVec2& mouse, const ImVec2& origin, float line_height, const std::vector<std::string>& lines) const {
    SelPos pos;
    int line = (int)((mouse.y - origin.y) / line_height);
    line = std::clamp(line, 0, (int)lines.size() - 1);
    pos.line = line;
    float relx = mouse.x - origin.x;
    int col = point_to_col(lines[line], relx);
    int maxcol = trimmed_len(lines[line]);
    pos.col = std::clamp(col, 0, maxcol);
    return pos;
}

void Terminal::copy_selection_to_clipboard(const ImVec2& origin, float line_height, const std::vector<std::string>& lines) {
    if (!has_selection() || lines.empty()) return;
    SelPos a = sel_start.value();
    SelPos b = sel_end.value();
    if (b.line < a.line || (b.line == a.line && b.col < a.col)) std::swap(a, b);
    int start_line = std::clamp(a.line, 0, (int)lines.size() - 1);
    int end_line = std::clamp(b.line, 0, (int)lines.size() - 1);
    std::string clip;
    for (int i = start_line; i <= end_line; ++i) {
        const std::string& l = lines[i];
        int start_col = (i == start_line) ? a.col : 0;
        int end_col = (i == end_line) ? b.col : (int)l.size();
        start_col = std::clamp(start_col, 0, (int)l.size());
        end_col = std::clamp(end_col, 0, (int)l.size());
        if (start_col < end_col) clip.append(l.substr(start_col, end_col - start_col));
        if (i != end_line) clip.push_back('\n');
    }
    ImGui::SetClipboardText(clip.c_str());
}

void Terminal::rebuild_screen(std::vector<Line>& lines_out, std::vector<std::string>& plain_out) {
    lines_out.clear();
    plain_out.clear();
    // Start with scrollback
    for (const auto& l : scrollback) {
        lines_out.push_back(l);
        std::string p;
        for (const auto& c : l.cells) p += cell_text(c);
        plain_out.push_back(std::move(p));
    }

    // Current screen
    int cur_rows, cur_cols;
    vterm_get_size(vt, &cur_rows, &cur_cols);
    vterm_state_get_cursorpos(state, &cursor_pos);
    Line line;
    line.cells.resize(cur_cols);
    for (int r = 0; r < cur_rows; ++r) {
        for (int c = 0; c < cur_cols; ++c) {
            VTermPos pos{ (int)r, (int)c };
            VTermScreenCell cell;
            vterm_screen_get_cell(screen, pos, &cell);
            Cell out;
            out.codepoint = cell.chars[0] ? cell.chars[0] : ' ';
            out.fg = cell.fg;
            out.bg = cell.bg;
            out.bold = cell.attrs.bold;
            out.underline = cell.attrs.underline != 0;
            out.reverse = cell.attrs.reverse;
            line.cells[c] = out;
        }
        lines_out.push_back(line);
        std::string p;
        for (const auto& c : line.cells) p += cell_text(c);
        plain_out.push_back(std::move(p));
    }
}

void Terminal::handle_input() {
    ImGuiIO& io = ImGui::GetIO();
    VTermModifier mods = imgui_mods();
    bool cmdDown = io.KeySuper;
    bool ctrlOnly = io.KeyCtrl && !io.KeySuper;

    // Printable chars
    for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
        ImWchar c = io.InputQueueCharacters[i];
        if (c >= 0x20) {
            vterm_keyboard_unichar(vt, c, mods);
        }
    }

    auto send_key = [&](VTermKey k) { vterm_keyboard_key(vt, k, mods); };

    if (ImGui::IsKeyPressed(ImGuiKey_Enter)) send_key(VTERM_KEY_ENTER);
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) send_key(VTERM_KEY_BACKSPACE);
    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) send_key(VTERM_KEY_TAB);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) send_key(VTERM_KEY_UP);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) send_key(VTERM_KEY_DOWN);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) send_key(VTERM_KEY_LEFT);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) send_key(VTERM_KEY_RIGHT);
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) send_key(VTERM_KEY_HOME);
    if (ImGui::IsKeyPressed(ImGuiKey_End)) send_key(VTERM_KEY_END);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) send_key(VTERM_KEY_PAGEUP);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) send_key(VTERM_KEY_PAGEDOWN);

    // Copy is handled in Render when Cmd+C and selection exists

    // Ctrl+ combos to send control chars
    if (ctrlOnly) {
        if (ImGui::IsKeyPressed(ImGuiKey_C)) send_control_char('\x03'); // ETX
        if (ImGui::IsKeyPressed(ImGuiKey_Z)) send_control_char('\x1A'); // SUB
        if (ImGui::IsKeyPressed(ImGuiKey_D)) send_control_char('\x04'); // EOT
    }

    // Cmd+V paste
    if (cmdDown && ImGui::IsKeyPressed(ImGuiKey_V)) {
        paste_clipboard();
    }
}

void Terminal::Render() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f,0.05f,0.05f,1.0f));
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("scroll_region", ImVec2(avail.x, avail.y), true, ImGuiWindowFlags_HorizontalScrollbar);

    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    if (focused) handle_input();

    std::vector<Line> lines;
    std::vector<std::string> plain;
    rebuild_screen(lines, plain);
    std::vector<int> trimmed;
    trimmed.reserve(plain.size());
    for (const auto& l : plain) trimmed.push_back(trimmed_len(l));

    ImVec2 origin = ImGui::GetCursorScreenPos();
    float line_height = ImGui::GetTextLineHeightWithSpacing();

    // Draw lines
    ImGuiListClipper clipper;
    clipper.Begin((int)lines.size(), line_height);
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            ImGui::PushID(i);
            float x_cursor = ImGui::GetCursorPosX();
            const auto& line = lines[i];
            size_t idx = 0;
            while (idx < line.cells.size()) {
                Cell base = line.cells[idx];
                std::string text = cell_text(base);
                size_t j = idx + 1;
                for (; j < line.cells.size(); ++j) {
                    const auto& c = line.cells[j];
                    if (!colors_equal(screen, c.fg, base.fg) || !colors_equal(screen, c.bg, base.bg) ||
                        c.bold != base.bold || c.reverse != base.reverse || c.underline != base.underline) break;
                    text += cell_text(c);
                }
                ImVec4 fg = color_to_vec(base.reverse ? base.bg : base.fg);
                ImVec4 bg = color_to_vec(base.reverse ? base.fg : base.bg);
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImVec2 size = ImGui::CalcTextSize(text.c_str());
                if (bg.x > 0.06f || bg.y > 0.06f || bg.z > 0.06f) {
                    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + line_height), ImGui::ColorConvertFloat4ToU32(bg));
                }
                ImGui::PushStyleColor(ImGuiCol_Text, fg);
                ImGui::TextUnformatted(text.c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine(0,0);
                idx = j;
            }
            ImGui::NewLine();
            ImGui::PopID();
        }
    }

    // Selection handling
    bool hovered = ImGui::IsWindowHovered();
    if (hovered && ImGui::IsMouseClicked(0)) {
        clear_selection();
        selecting = true;
        sel_start = mouse_to_pos(ImGui::GetIO().MousePos, origin, line_height, plain);
        sel_end = sel_start;
    }
    if (selecting && ImGui::IsMouseDown(0)) {
        sel_end = mouse_to_pos(ImGui::GetIO().MousePos, origin, line_height, plain);
    } else if (selecting && ImGui::IsMouseReleased(0)) {
        sel_end = mouse_to_pos(ImGui::GetIO().MousePos, origin, line_height, plain);
        selecting = false;
    }
    if (has_selection()) {
        SelPos a = sel_start.value();
        SelPos b = sel_end.value();
        if (b.line < a.line || (b.line == a.line && b.col < a.col)) std::swap(a, b);
        for (int line_idx = a.line; line_idx <= b.line && line_idx < (int)plain.size(); ++line_idx) {
            int maxc = trimmed[line_idx];
            if (maxc <= 0) continue;
            int start_col = (line_idx == a.line) ? a.col : 0;
            int end_col = (line_idx == b.line) ? b.col : maxc;
            start_col = std::clamp(start_col, 0, maxc);
            end_col = std::clamp(end_col, 0, maxc);
            if (start_col == end_col) continue;
            float y = origin.y + line_idx * line_height;
            float x_start = origin.x + ImGui::CalcTextSize(plain[line_idx].substr(0, start_col).c_str()).x;
            float x_end = origin.x + ImGui::CalcTextSize(plain[line_idx].substr(0, end_col).c_str()).x;
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x_start, y), ImVec2(x_end, y + line_height), ImGui::GetColorU32(ImVec4(0.2f,0.4f,1.0f,0.35f)));
        }
    }

    // Auto-scroll
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - line_height * 2) {
        ImGui::SetScrollHereY(1.0f);
    }

    // Copy shortcut
    if (has_selection() && ImGui::GetIO().KeySuper && ImGui::IsKeyPressed(ImGuiKey_C)) {
        copy_selection_to_clipboard(origin, line_height, plain);
    }

    if (ImGui::BeginPopupContextWindow()) {
        if (has_selection()) {
            if (ImGui::MenuItem("Copy")) {
                copy_selection_to_clipboard(origin, line_height, plain);
            }
        }
        if (ImGui::MenuItem("Paste")) {
            paste_clipboard();
        }
        ImGui::EndPopup();
    }

    // Draw blinking cursor only when focused
    if (focused) {
        float t = ImGui::GetTime();
        bool blink_on = fmodf(t, 1.0f) < 0.5f;
        if (blink_on) {
            int line = (int)scrollback.size() + cursor_pos.row;
            if (line >= 0 && line < (int)lines.size()) {
                float y = origin.y + line * line_height;
                std::string prefix = plain[line].substr(0, cursor_pos.col);
                float x = origin.x + ImGui::CalcTextSize(prefix.c_str()).x;
                ImVec2 p1(x, y);
                ImVec2 p2(x + 2.0f, y + line_height - 2.0f);
                ImGui::GetWindowDrawList()->AddRectFilled(p1, p2, ImGui::GetColorU32(ImVec4(0.9f,0.9f,0.9f,1.0f)));
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void Terminal::paste_clipboard() {
    const char* clip = ImGui::GetClipboardText();
    if (!clip) return;
    vterm_keyboard_start_paste(vt);
    vterm_input_write(vt, clip, strlen(clip));
    vterm_keyboard_end_paste(vt);
}

void Terminal::send_control_char(char c) {
    vterm_input_write(vt, &c, 1);
}
