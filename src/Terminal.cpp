#include "Terminal.h"
#include <imgui_internal.h>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstring>

Terminal::Terminal() {
    Reset();
    input_buf[0] = '\0';
}

void Terminal::Reset() {
    scrollback.clear();
    current_line.segments.clear();
    current_style = { ImVec4(0.85f, 0.85f, 0.85f, 1.0f), ImVec4(0.05f, 0.05f, 0.05f, 1.0f), false, false, false };
    state = State::Text;
    esc_buffer.clear();
}

void Terminal::Clear() {
    scrollback.clear();
    current_line.segments.clear();
}

void Terminal::push_current_line() {
    scrollback.push_back(current_line);
    plain.push_back(get_plain_text(current_line));
    if (scrollback.size() > max_scrollback) {
        scrollback.pop_front();
        plain.pop_front();
    }
    current_line.segments.clear();
}

void Terminal::append_char(char c) {
    if (current_line.segments.empty() || memcmp(&current_line.segments.back().style, &current_style, sizeof(Style)) != 0) {
        current_line.segments.push_back({ current_style, std::string(1, c) });
    } else {
        current_line.segments.back().text.push_back(c);
    }
}

void Terminal::backspace() {
    if (current_line.segments.empty()) return;
    auto& seg = current_line.segments.back();
    if (!seg.text.empty()) {
        seg.text.pop_back();
        if (seg.text.empty()) current_line.segments.pop_back();
    }
}

void Terminal::carriage_return() {
    current_line.segments.clear();
}

void Terminal::apply_sgr(const std::vector<int>& codes) {
    if (codes.empty()) {
        // Reset
        current_style = { ImVec4(0.85f,0.85f,0.85f,1.0f), ImVec4(0.05f,0.05f,0.05f,1.0f), false, false, false };
        return;
    }

    for (size_t i = 0; i < codes.size(); ++i) {
        int c = codes[i];
        if (c == 0) {
            current_style = { ImVec4(0.85f,0.85f,0.85f,1.0f), ImVec4(0.05f,0.05f,0.05f,1.0f), false, false, false };
        } else if (c == 1) {
            current_style.bold = true;
        } else if (c == 4) {
            current_style.underline = true;
        } else if (c == 7) {
            current_style.inverse = true;
        } else if (c >= 30 && c <= 37) {
            current_style.fg = color_from_ansi(c - 30, false);
        } else if (c >= 90 && c <= 97) {
            current_style.fg = color_from_ansi(c - 90, true);
        } else if (c == 39) {
            current_style.fg = ImVec4(0.85f,0.85f,0.85f,1.0f);
        } else if (c >= 40 && c <= 47) {
            current_style.bg = color_from_ansi(c - 40, false);
        } else if (c >= 100 && c <= 107) {
            current_style.bg = color_from_ansi(c - 100, true);
        } else if (c == 49) {
            current_style.bg = ImVec4(0.05f,0.05f,0.05f,1.0f);
        } else if (c == 38 || c == 48) {
            // 256-color: 38;5;n or 48;5;n
            if (i + 2 < codes.size() && codes[i+1] == 5) {
                int code = codes[i+2];
                ImVec4 col = color_from_256(code);
                if (c == 38) current_style.fg = col;
                else current_style.bg = col;
                i += 2;
            }
        }
    }
}

ImVec4 Terminal::color_from_ansi(int code, bool bright) const {
    static const ImVec4 base[] = {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.8f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.6f, 0.0f, 1.0f},
        {0.8f, 0.5f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.8f, 1.0f},
        {0.8f, 0.0f, 0.8f, 1.0f},
        {0.0f, 0.6f, 0.6f, 1.0f},
        {0.75f,0.75f,0.75f,1.0f}
    };
    ImVec4 c = base[std::clamp(code, 0, 7)];
    if (bright) {
        c.x = std::min(1.0f, c.x + 0.3f);
        c.y = std::min(1.0f, c.y + 0.3f);
        c.z = std::min(1.0f, c.z + 0.3f);
    }
    return c;
}

ImVec4 Terminal::color_from_256(int code) const {
    if (code < 16) return color_from_ansi(code % 8, code >= 8);
    if (code >= 16 && code <= 231) {
        int idx = code - 16;
        int r = (idx / 36) % 6;
        int g = (idx / 6) % 6;
        int b = idx % 6;
        return ImVec4(r/5.0f, g/5.0f, b/5.0f, 1.0f);
    }
    // grayscale
    float level = (code - 232) / 23.0f;
    return ImVec4(level, level, level, 1.0f);
}

void Terminal::Feed(const std::string& data) {
    handle_bracketed_paste(data);
    for (char ch : data) {
        unsigned char uc = static_cast<unsigned char>(ch);
        switch (state) {
            case State::Text:
                if (uc == '\x1b') {
                    state = State::Escape;
                    esc_buffer.clear();
                } else if (uc == '\n') {
                    push_current_line();
                } else if (uc == '\r') {
                    carriage_return();
                } else if (uc == '\b') {
                    backspace();
                } else if (uc == '\t') {
                    append_char(' ');
                    append_char(' ');
                    append_char(' ');
                    append_char(' ');
                } else if (uc >= 0x20) {
                    append_char(ch);
                }
                break;
            case State::Escape:
                if (uc == '[') {
                    state = State::CSI;
                } else if (uc == ']') {
                    state = State::OSC;
                } else {
                    state = State::Text;
                }
                break;
            case State::CSI:
                if ((uc >= '0' && uc <= '9') || uc == ';') {
                    esc_buffer.push_back(ch);
                } else {
                    // parse buffer
                    std::vector<int> codes;
                    std::stringstream ss(esc_buffer);
                    std::string seg;
                    while (std::getline(ss, seg, ';')) {
                        if (!seg.empty()) codes.push_back(std::stoi(seg));
                    }
                    if (uc == 'm') {
                        apply_sgr(codes);
                    } else if (uc == 'h' && !codes.empty() && codes[0] == 2004) { // bracketed paste on
                        bracketed_paste = true;
                    } else if (uc == 'l' && !codes.empty() && codes[0] == 2004) { // bracketed paste off
                        bracketed_paste = false;
                    } else if (uc == 'K') {
                        // clear line: drop current
                        carriage_return();
                    } else if (uc == 'J') {
                        // clear screen
                        Clear();
                    } else if (uc == 'G' || uc == '`' || uc == 'D' || uc == 'C' || uc == 'A' || uc == 'B') {
                        // basic cursor moves not fully simulated; ignore safely
                    }
                    esc_buffer.clear();
                    state = State::Text;
                }
                break;
            case State::OSC:
                // consume until bell or ESC
                if (uc == 0x07 || uc == '\x1b') {
                    esc_buffer.clear();
                    state = State::Text;
                }
                break;
        }
    }
}

void Terminal::handle_bracketed_paste(const std::string& data) {
    if (!bracketed_paste) return;
    const std::string start = "\x1b[200~";
    const std::string end = "\x1b[201~";
    size_t pos = 0;
    while (true) {
        size_t s = data.find(start, pos);
        if (s == std::string::npos) break;
        size_t e = data.find(end, s + start.size());
        if (e == std::string::npos) break;
        std::string payload = data.substr(s + start.size(), e - (s + start.size()));
        outgoing += payload;
        pos = e + end.size();
    }
}

std::string Terminal::ConsumeOutgoing() {
    std::string out = outgoing;
    outgoing.clear();
    return out;
}

std::string Terminal::get_plain_text(const Line& line) const {
    std::string res;
    for (const auto& seg : line.segments) res += seg.text;
    return res;
}

bool Terminal::has_selection() const {
    return select_start.has_value() && select_end.has_value();
}

void Terminal::clear_selection() {
    selecting = false;
    select_start.reset();
    select_end.reset();
}

int Terminal::point_to_col(const std::string& text, float x, float /*glyph_w*/) const {
    float acc = 0.0f;
    for (size_t i = 0; i < text.size(); ++i) {
        float w = ImGui::CalcTextSize(text.substr(i,1).c_str()).x;
        if (acc + w * 0.5f >= x) return (int)i;
        acc += w;
    }
    return (int)text.size();
}

void Terminal::copy_selection_to_clipboard(const ImVec2& origin, float line_height) {
    if (!has_selection() || plain.empty()) return;
    ImVec2 a = select_start.value();
    ImVec2 b = select_end.value();
    if (b.y < a.y || (b.y == a.y && b.x < a.x)) std::swap(a, b);
    int start_line = (int)((a.y - origin.y) / line_height);
    int end_line = (int)((b.y - origin.y) / line_height);
    start_line = std::max(0, start_line);
    end_line = std::min((int)plain.size() - 1, end_line);
    std::string clip;
    for (int i = start_line; i <= end_line; ++i) {
        const std::string& l = plain[i];
        int start_col = (i == start_line) ? point_to_col(l, a.x - origin.x, 0.0f) : 0;
        int end_col = (i == end_line) ? point_to_col(l, b.x - origin.x, 0.0f) : (int)l.size();
        start_col = std::clamp(start_col, 0, (int)l.size());
        end_col = std::clamp(end_col, 0, (int)l.size());
        if (start_col < end_col) clip.append(l.substr(start_col, end_col - start_col));
        if (i != end_line) clip.push_back('\n');
    }
    ImGui::SetClipboardText(clip.c_str());
}

void Terminal::Render() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, current_style.bg);
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("scroll_region", ImVec2(avail.x, avail.y), true, ImGuiWindowFlags_HorizontalScrollbar);

    long total_lines = static_cast<long>(scrollback.size()) + 1; // include current
    ImGuiListClipper clip;
    clip.Begin(total_lines);
    ImVec2 child_origin = ImGui::GetCursorScreenPos();
    float line_height = ImGui::GetTextLineHeight();
    bool hovered = ImGui::IsWindowHovered();
    while (clip.Step()) {
        for (int idx = clip.DisplayStart; idx < clip.DisplayEnd; ++idx) {
            const Line* line;
            if (idx < (int)scrollback.size()) line = &scrollback[idx];
            else line = &current_line;

            ImGui::PushID(idx);
            bool first = true;
            for (const auto& seg : line->segments) {
                if (!first) ImGui::SameLine(0, 0);
                first = false;
                ImVec4 fg = seg.style.inverse ? seg.style.bg : seg.style.fg;
                ImVec4 bg = seg.style.inverse ? seg.style.fg : seg.style.bg;
                ImVec2 pos = ImGui::GetCursorScreenPos();
                float text_height = ImGui::GetTextLineHeight();
                ImVec2 size = ImGui::CalcTextSize(seg.text.c_str());
                if (bg.w > 0.9f && (bg.x > 0.08f || bg.y > 0.08f || bg.z > 0.08f)) {
                    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + text_height), ImGui::ColorConvertFloat4ToU32(bg));
                }
                ImGui::PushStyleColor(ImGuiCol_Text, fg);
                ImGui::TextUnformatted(seg.text.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::NewLine();
            ImGui::PopID();
        }
    }

    // Selection handling
    if (hovered && ImGui::IsMouseClicked(0)) {
        clear_selection();
        selecting = true;
        select_start = ImGui::GetIO().MousePos;
        select_end = select_start;
    }
    if (selecting && ImGui::IsMouseDown(0)) {
        select_end = ImGui::GetIO().MousePos;
    } else if (selecting && ImGui::IsMouseReleased(0)) {
        select_end = ImGui::GetIO().MousePos;
        selecting = false;
    }

    if (has_selection()) {
        ImVec2 a = select_start.value();
        ImVec2 b = select_end.value();
        ImGui::GetWindowDrawList()->AddRectFilled(a, b, ImGui::GetColorU32(ImVec4(0.2f, 0.4f, 1.0f, 0.35f)));
    }

    // Auto-scroll
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - line_height * 2) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Inline input
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2,2));
    if (ImGui::InputText("##inline_term", input_buf, IM_ARRAYSIZE(input_buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
        [](ImGuiInputTextCallbackData* data)->int {
            Terminal* term = reinterpret_cast<Terminal*>(data->UserData);
            if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                if (data->EventKey == ImGuiKey_UpArrow) {
                    if (!term->history.empty()) {
                        if (term->history_pos == -1) term->history_pos = (int)term->history.size() - 1;
                        else if (term->history_pos > 0) term->history_pos--;
                        std::string h = term->history[term->history_pos];
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, h.c_str());
                    }
                } else if (data->EventKey == ImGuiKey_DownArrow) {
                    if (term->history_pos != -1) {
                        term->history_pos++;
                        if (term->history_pos >= (int)term->history.size()) {
                            term->history_pos = -1;
                            data->DeleteChars(0, data->BufTextLen);
                        } else {
                            std::string h = term->history[term->history_pos];
                            data->DeleteChars(0, data->BufTextLen);
                            data->InsertChars(0, h.c_str());
                        }
                    }
                }
            }
            return 0;
        }, this)) {
        std::string cmd = input_buf;
        outgoing += cmd + "\n";
        if (!cmd.empty()) {
            history.push_back(cmd);
            if (history.size() > 200) history.erase(history.begin());
        }
        history_pos = -1;
        input_buf[0] = '\0';
        Feed(cmd + "\n");
    }
    ImGui::PopStyleVar();

    // Copy selection
    if (has_selection() && ImGui::GetIO().KeySuper && ImGui::IsKeyPressed(ImGuiKey_C)) {
        copy_selection_to_clipboard(child_origin, line_height);
    }
}
