#pragma once
#include "imgui.h"
#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <iostream>

// Simple UTF-8 State Machine Helper
struct UTF8Decoder {
    uint32_t codepoint = 0;
    int bytes_remaining = 0;

    // Returns true if a complete codepoint is ready
    bool decode(char c, uint32_t& out_codepoint) {
        unsigned char uc = (unsigned char)c;
        if (bytes_remaining == 0) {
            if (uc <= 0x7F) {
                out_codepoint = uc;
                return true;
            } else if ((uc & 0xE0) == 0xC0) {
                codepoint = uc & 0x1F;
                bytes_remaining = 1;
            } else if ((uc & 0xF0) == 0xE0) {
                codepoint = uc & 0x0F;
                bytes_remaining = 2;
            } else if ((uc & 0xF8) == 0xF0) {
                codepoint = uc & 0x07;
                bytes_remaining = 3;
            } else {
                // Invalid start byte, treat as replacement char or ignore
                out_codepoint = 0xFFFD; 
                return true;
            }
            return false;
        } else {
            if ((uc & 0xC0) == 0x80) {
                codepoint = (codepoint << 6) | (uc & 0x3F);
                bytes_remaining--;
                if (bytes_remaining == 0) {
                    out_codepoint = codepoint;
                    return true;
                }
            } else {
                // Invalid sequence, reset
                bytes_remaining = 0;
                out_codepoint = 0xFFFD;
                return true;
            }
            return false;
        }
    }
};

// Terminal Cell
struct TermCell {
    uint32_t codepoint = ' ';
    uint32_t fg = 0xFFD0D0D0; // Default Light Grey
    uint32_t bg = 0xFF101010; // Default Dark
};

class Terminal {
public:
    Terminal();
    
    void Write(const std::string& data);
    void Draw(const char* title, float width, float height);
    std::string CheckInput();

private:
    std::mutex mutex;
    
    // Configuration
    int cols = 80;
    int rows = 24;

    // Buffers
    // We store lines as vectors of cells.
    // scrollback contains lines that have scrolled off the top.
    // screen contains the active rows.
    std::deque<std::vector<TermCell>> scrollback;
    std::vector<std::vector<TermCell>> screen; // Fixed size: rows

    // Cursor
    int cursor_x = 0;
    int cursor_y = 0; // Relative to screen top
    bool cursor_visible = true;
    
    // Style
    uint32_t current_fg = 0xFFD0D0D0;
    uint32_t current_bg = 0xFF101010;
    
    // Parsing
    enum class State { Text, Escape, CSI, Osc, PrivateMode };
    State state = State::Text;
    std::string param_buffer;
    UTF8Decoder utf8;
    
    // Input
    std::string input_queue;

    // Selection
    bool selecting = false;
    int sel_start_x = -1, sel_start_y = -1; // y is absolute (scrollback index)
    int sel_end_x = -1, sel_end_y = -1;

    // Methods
    void ProcessCodepoint(uint32_t c);
    void ExecuteCSI(char cmd);
    void ExecuteOSC(); // Just clears buffer for now
    
    void PutChar(uint32_t c);
    void NewLine();
    void Backspace();
    void Scroll();
    
    void SetColor(int code);
    uint32_t Get256Color(int code);

    // Selection Helpers
    long GetTotalLines() { return scrollback.size() + screen.size(); }
    const std::vector<TermCell>& GetLine(long idx) {
        if (idx < scrollback.size()) return scrollback[idx];
        return screen[idx - scrollback.size()];
    }
};
