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
    if (scrollback.size() > max_scrollback) {
        scrollback.pop_front();
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

std::string Terminal::ConsumeOutgoing() {
    std::string out = outgoing;
    outgoing.clear();
    return out;
}

void Terminal::Render() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, current_style.bg);
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float input_height = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2;
    ImGui::BeginChild("scroll_region", ImVec2(avail.x, avail.y - input_height - ImGui::GetStyle().ItemSpacing.y), false, ImGuiWindowFlags_HorizontalScrollbar);

    long total_lines = static_cast<long>(scrollback.size()) + 1; // include current
    ImGuiListClipper clip;
    clip.Begin(total_lines);
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
    ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::PushItemWidth(avail.x);
    if (ImGui::InputTextWithHint("##term_input", "Type command and press Enter", input_buf, IM_ARRAYSIZE(input_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string cmd = input_buf;
        outgoing += cmd + "\n";
        if (!cmd.empty()) {
            history.push_back(cmd);
            if (history.size() > 200) history.erase(history.begin());
        }
        history_pos = -1;
        input_buf[0] = '\0';
    }

    // History navigation
    if (ImGui::IsItemActive()) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && !history.empty()) {
            if (history_pos == -1) history_pos = (int)history.size() - 1;
            else if (history_pos > 0) history_pos--;
            strncpy(input_buf, history[history_pos].c_str(), sizeof(input_buf));
            input_buf[sizeof(input_buf) - 1] = '\0';
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && history_pos != -1) {
            history_pos++;
            if (history_pos >= (int)history.size()) {
                history_pos = -1;
                input_buf[0] = '\0';
            } else {
                strncpy(input_buf, history[history_pos].c_str(), sizeof(input_buf));
                input_buf[sizeof(input_buf) - 1] = '\0';
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Send Ctrl+C")) {
        outgoing += "\x03";
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        Clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        Reset();
    }
}
