#include "Terminal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <sstream>
#include <algorithm>
#include <cmath>

Terminal::Terminal() {
    // Init screen
    screen.resize(rows);
    for(auto& line : screen) line.resize(cols);
}

void Terminal::Write(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex);
    for (char c : data) {
        uint32_t cp;
        if (utf8.decode(c, cp)) {
            ProcessCodepoint(cp);
        }
    }
}

void Terminal::ProcessCodepoint(uint32_t c) {
    if (state == State::Text) {
        if (c == 0x1B) { // ESC
            state = State::Escape;
        } else if (c == '\n') {
            NewLine();
        } else if (c == '\r') {
            cursor_x = 0;
        } else if (c == '\b') {
            Backspace();
        } else if (c == 0x07) { // BEL
            // Flash?
        } else if (c == '\t') {
            int next_tab = (cursor_x / 8 + 1) * 8;
            if (next_tab >= cols) next_tab = cols - 1;
            while (cursor_x < next_tab) PutChar(' ');
        } else if (c >= 32) {
            PutChar(c);
        }
    } else if (state == State::Escape) {
        if (c == '[') {
            state = State::CSI;
            param_buffer.clear();
        } else if (c == ']') {
            state = State::Osc;
            param_buffer.clear();
        } else {
            state = State::Text; // Reset
        }
    } else if (state == State::CSI) {
        if ((c >= '0' && c <= '9') || c == ';' || c == '?' || c == '>' || c == ' ') {
            param_buffer += (char)c;
        } else {
            ExecuteCSI((char)c);
            state = State::Text;
        }
    } else if (state == State::Osc) {
        if (c == 0x07 || c == 0x1B) { // End on BEL or ESC
             // Handle OSC if needed (Title etc)
             state = State::Text;
        }
        // Consume
    }
}

void Terminal::ExecuteCSI(char cmd) {
    std::vector<int> args;
    std::stringstream ss(param_buffer);
    std::string segment;
    
    bool private_mode = (!param_buffer.empty() && param_buffer[0] == '?');
    // Strip ? or >
    std::string clean_params = param_buffer;
    if (!clean_params.empty() && (clean_params[0] == '?' || clean_params[0] == '>')) {
        clean_params = clean_params.substr(1);
    }

    ss.str(clean_params);
    while(std::getline(ss, segment, ';')) {
        try {
            if (!segment.empty()) args.push_back(std::stoi(segment));
        } catch (...) {}
    }
    if (args.empty()) args.push_back(0);

    switch (cmd) {
        case 'm': // Color/Attribute
            for (size_t i=0; i<args.size(); ++i) {
                int code = args[i];
                if (code == 38 && i+2 < args.size() && args[i+1] == 5) { // 256 Color FG
                    current_fg = Get256Color(args[i+2]);
                    i += 2;
                } else if (code == 48 && i+2 < args.size() && args[i+1] == 5) { // 256 Color BG
                    current_bg = Get256Color(args[i+2]);
                    i += 2;
                } else {
                    SetColor(code);
                }
            }
            break;
        case 'J': // Clear
            if (args[0] == 2) {
                 // Clear all: Move screen to history? No, just clear.
                 // Standard Xterm: 2J clears screen, doesn't clear history.
                 // 3J clears scrollback.
                 for(auto& row : screen) std::fill(row.begin(), row.end(), TermCell{' ', current_fg, current_bg});
                 cursor_x = 0; cursor_y = 0;
            } else if (args[0] == 0) { // Cursor to end
                 // Current line
                 for(int x=cursor_x; x<cols; x++) screen[cursor_y][x] = {' ', current_fg, current_bg};
                 // Next lines
                 for(int y=cursor_y+1; y<rows; y++) 
                     std::fill(screen[y].begin(), screen[y].end(), TermCell{' ', current_fg, current_bg});
            }
            break;
        case 'K': // Clear Line
            if (args[0] == 0) { // Cursor to end
                 for(int x=cursor_x; x<cols; x++) screen[cursor_y][x] = {' ', current_fg, current_bg};
            } else if (args[0] == 1) { // Start to cursor
                 for(int x=0; x<=cursor_x; x++) screen[cursor_y][x] = {' ', current_fg, current_bg};
            } else { // All
                 std::fill(screen[cursor_y].begin(), screen[cursor_y].end(), TermCell{' ', current_fg, current_bg});
            }
            break;
        case 'A': cursor_y = std::max(0, cursor_y - (args[0]?args[0]:1)); break;
        case 'B': cursor_y = std::min(rows-1, cursor_y + (args[0]?args[0]:1)); break;
        case 'C': cursor_x = std::min(cols-1, cursor_x + (args[0]?args[0]:1)); break;
        case 'D': cursor_x = std::max(0, cursor_x - (args[0]?args[0]:1)); break;
        case 'H': // Pos
        case 'f':
             if (args.size() >= 2) {
                 cursor_y = std::min(rows-1, std::max(0, args[0] - 1));
                 cursor_x = std::min(cols-1, std::max(0, args[1] - 1));
             } else { cursor_y = 0; cursor_x = 0; }
             break;
        case 'n': // DSR
            if (args[0] == 6) {
                // Response
                input_queue += "\033[" + std::to_string(cursor_y + 1) + ";" + std::to_string(cursor_x + 1) + "R";
            }
            break;
        case 'h':
            if (private_mode && args[0] == 25) cursor_visible = true;
            break;
        case 'l':
             if (private_mode && args[0] == 25) cursor_visible = false;
             break;
    }
}

void Terminal::PutChar(uint32_t c) {
    if (cursor_x >= cols) {
        cursor_x = 0;
        NewLine();
    }
    screen[cursor_y][cursor_x] = {c, current_fg, current_bg};
    cursor_x++;
}

void Terminal::NewLine() {
    cursor_y++;
    if (cursor_y >= rows) {
        Scroll();
        cursor_y = rows - 1;
    }
}

void Terminal::Scroll() {
    scrollback.push_back(screen[0]);
    if (scrollback.size() > 5000) scrollback.pop_front();
    
    for (int y = 0; y < rows - 1; y++) {
        screen[y] = screen[y+1];
    }
    screen[rows-1].assign(cols, {' ', current_fg, current_bg});
}

void Terminal::Backspace() {
    if (cursor_x > 0) cursor_x--;
}

void Terminal::SetColor(int code) {
    if (code == 0) { current_fg = 0xFFD0D0D0; current_bg = 0xFF101010; }
    else if (code >= 30 && code <= 37) {
         const uint32_t colors[] = { 0xFF000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA, 0xFFAA0000, 0xFFAA00AA, 0xFFAAAA00, 0xFFAAAAAA };
         current_fg = colors[code-30];
    } else if (code >= 40 && code <= 47) {
         const uint32_t colors[] = { 0xFF000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA, 0xFFAA0000, 0xFFAA00AA, 0xFFAAAA00, 0xFFAAAAAA };
         current_bg = colors[code-40];
    }
    // ... Add Bright colors
}

uint32_t Terminal::Get256Color(int code) {
    // Basic implementation of Xterm 256 color palette mapping
    if (code < 16) return 0xFFFFFFFF; // Simplify for prototype
    return 0xFFFFFFFF; 
}

std::string Terminal::CheckInput() {
    std::lock_guard<std::mutex> lock(mutex);
    std::string out = input_queue;
    input_queue.clear();
    return out;
}

void Terminal::Draw(const char* title, float width, float height) {
    ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoScrollbar);
    
    std::lock_guard<std::mutex> lock(mutex);
    
    float char_height = ImGui::GetTextLineHeight();
    long total_lines = scrollback.size() + rows;
    
    // Use Clipper for performant scrolling of history
    ImGuiListClipper clipper;
    clipper.Begin(total_lines, char_height);
    
    // We need to handle input globally for the window
    if (ImGui::IsWindowFocused()) {
         ImGuiIO& io = ImGui::GetIO();
         if (io.InputQueueCharacters.Size > 0) {
             for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
                 char c = (char)io.InputQueueCharacters[i];
                 input_queue += c;
             }
         }
         if (ImGui::IsKeyPressed(ImGuiKey_Enter)) input_queue += "\n";
         if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) input_queue += "\b";
         if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) input_queue += "\033[A";
         if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) input_queue += "\033[B";
         if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) input_queue += "\033[D";
         if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) input_queue += "\033[C";
         if (ImGui::IsKeyPressed(ImGuiKey_Tab)) input_queue += "\t";
         if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) input_queue += "\x03";
    }

    while (clipper.Step()) {
        for (int y = clipper.DisplayStart; y < clipper.DisplayEnd; y++) {
             const auto& line = (y < scrollback.size()) ? scrollback[y] : screen[y - scrollback.size()];
             
             // Render line
             // Optimization: Instead of drawing rects, we construct strings for same-colored segments
             // This is much faster and allows proper font kerning.
             
             // Simple fallback for now: Draw Text.
             // We need to position cursor manually because we are in a Clipper
             ImVec2 pos = ImGui::GetCursorScreenPos();
             // But Clipper just spaces out the dummy area.
             // We should draw relative to window pos + scroll.
             // Actually, standard ImGui usage with clipper implies we use Text() calls.
             
             std::string line_text;
             // Just dumping text ignores colors. We need horizontal loop.
             // Let's try the color span approach again.
             
             for (int x = 0; x < cols; x++) {
                 const auto& cell = line[x];
                 ImGui::PushStyleColor(ImGuiCol_Text, cell.fg);
                 
                 // Background?
                 if (cell.bg != 0xFF101010) { // Not default
                     // Draw BG rect
                     ImVec2 p = ImGui::GetCursorScreenPos();
                     ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + ImGui::CalcTextSize("M").x, p.y + char_height), cell.bg);
                 }
                 
                 char utf8_buf[5] = {0};
                 if (cell.codepoint <= 0x7F) utf8_buf[0] = (char)cell.codepoint;
                 else { 
                     // Naive re-encode or just use replacement. 
                     // ImGui handles UTF8 text, but our codepoint is uint32. 
                     // We need to encode it back to utf8 char* for ImGui::Text.
                     // Or use AddText with codepoint.
                     // ImGui doesn't expose AddText(codepoint) publicly in simple API.
                     // We can just cast if < 255, but for Box Drawing we need proper encoding.
                     // Minimal encode:
                     if (cell.codepoint < 0x80) {
                         utf8_buf[0] = (char)cell.codepoint;
                     } else if (cell.codepoint < 0x800) {
                         utf8_buf[0] = (char)(0xC0 | (cell.codepoint >> 6));
                         utf8_buf[1] = (char)(0x80 | (cell.codepoint & 0x3F));
                     } else if (cell.codepoint < 0x10000) {
                         utf8_buf[0] = (char)(0xE0 | (cell.codepoint >> 12));
                         utf8_buf[1] = (char)(0x80 | ((cell.codepoint >> 6) & 0x3F));
                         utf8_buf[2] = (char)(0x80 | (cell.codepoint & 0x3F));
                     }
                 }
                 
                 ImGui::TextUnformatted(utf8_buf);
                 ImGui::SameLine(0,0);
                 ImGui::PopStyleColor();
             }
             ImGui::NewLine();
        }
    }
    
    // Auto-scroll logic
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - char_height * 2) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::End();
}
