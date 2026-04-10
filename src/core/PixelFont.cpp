#include "core/PixelFont.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace {
using Glyph = std::array<const char*, 5>;

const Glyph& GlyphRows(char c) {
    static const Glyph space{"0000", "0000", "0000", "0000", "0000"};
    static const Glyph dot{"0000", "0000", "0000", "0010", "0010"};
    static const Glyph comma{"0000", "0000", "0000", "0010", "0100"};
    static const Glyph dash{"0000", "0000", "1110", "0000", "0000"};
    static const Glyph colon{"0000", "0010", "0000", "0010", "0000"};
    static const Glyph bang{"0010", "0010", "0010", "0000", "0010"};
    static const Glyph apos{"0010", "0010", "0000", "0000", "0000"};
    static const Glyph at{"0110", "1001", "1011", "1000", "0111"};

    static const Glyph a{"0110", "1001", "1111", "1001", "1001"};
    static const Glyph b{"1110", "1001", "1110", "1001", "1110"};
    static const Glyph cGlyph{"0111", "1000", "1000", "1000", "0111"};
    static const Glyph d{"1110", "1001", "1001", "1001", "1110"};
    static const Glyph e{"1111", "1000", "1110", "1000", "1111"};
    static const Glyph f{"1111", "1000", "1110", "1000", "1000"};
    static const Glyph g{"0111", "1000", "1011", "1001", "0111"};
    static const Glyph h{"1001", "1001", "1111", "1001", "1001"};
    static const Glyph i{"1110", "0100", "0100", "0100", "1110"};
    static const Glyph j{"0011", "0001", "0001", "1001", "0110"};
    static const Glyph k{"1001", "1010", "1100", "1010", "1001"};
    static const Glyph l{"1000", "1000", "1000", "1000", "1111"};
    static const Glyph m{"1001", "1111", "1111", "1001", "1001"};
    static const Glyph n{"1001", "1101", "1011", "1001", "1001"};
    static const Glyph o{"0110", "1001", "1001", "1001", "0110"};
    static const Glyph p{"1110", "1001", "1110", "1000", "1000"};
    static const Glyph q{"0110", "1001", "1001", "1011", "0111"};
    static const Glyph r{"1110", "1001", "1110", "1010", "1001"};
    static const Glyph s{"0111", "1000", "0110", "0001", "1110"};
    static const Glyph t{"1111", "0100", "0100", "0100", "0100"};
    static const Glyph u{"1001", "1001", "1001", "1001", "0110"};
    static const Glyph v{"1001", "1001", "1001", "0110", "0110"};
    static const Glyph w{"1001", "1001", "1111", "1111", "1001"};
    static const Glyph x{"1001", "1001", "0110", "1001", "1001"};
    static const Glyph y{"1001", "1001", "0110", "0100", "0100"};
    static const Glyph z{"1111", "0001", "0010", "0100", "1111"};

    static const Glyph n0{"0110", "1001", "1001", "1001", "0110"};
    static const Glyph n1{"0010", "0110", "0010", "0010", "0111"};
    static const Glyph n2{"1110", "0001", "0110", "1000", "1111"};
    static const Glyph n3{"1110", "0001", "0110", "0001", "1110"};
    static const Glyph n4{"1001", "1001", "1111", "0001", "0001"};
    static const Glyph n5{"1111", "1000", "1110", "0001", "1110"};
    static const Glyph n6{"0111", "1000", "1110", "1001", "0110"};
    static const Glyph n7{"1111", "0001", "0010", "0100", "0100"};
    static const Glyph n8{"0110", "1001", "0110", "1001", "0110"};
    static const Glyph n9{"0110", "1001", "0111", "0001", "1110"};

    switch (std::toupper(static_cast<unsigned char>(c))) {
    case 'A': return a;
    case 'B': return b;
    case 'C': return cGlyph;
    case 'D': return d;
    case 'E': return e;
    case 'F': return f;
    case 'G': return g;
    case 'H': return h;
    case 'I': return i;
    case 'J': return j;
    case 'K': return k;
    case 'L': return l;
    case 'M': return m;
    case 'N': return n;
    case 'O': return o;
    case 'P': return p;
    case 'Q': return q;
    case 'R': return r;
    case 'S': return s;
    case 'T': return t;
    case 'U': return u;
    case 'V': return v;
    case 'W': return w;
    case 'X': return x;
    case 'Y': return y;
    case 'Z': return z;
    case '0': return n0;
    case '1': return n1;
    case '2': return n2;
    case '3': return n3;
    case '4': return n4;
    case '5': return n5;
    case '6': return n6;
    case '7': return n7;
    case '8': return n8;
    case '9': return n9;
    case '.': return dot;
    case ',': return comma;
    case '-': return dash;
    case ':': return colon;
    case '!': return bang;
    case '\'': return apos;
    case '@': return at;
    default: return space;
    }
}

constexpr float kGlyphAdvance = 5.0f;
}

namespace pixel_font {

void DrawNumber(SDL_Renderer* renderer, int value, float x, float y, float scale, SDL_Color color) {
    DrawBitmapText(renderer, std::to_string(std::max(0, value)), x, y, scale, color);
}

float MeasureText(const std::string& text, float scale) {
    if (text.empty()) {
        return 0.0f;
    }
    return (static_cast<float>(text.size()) * kGlyphAdvance * scale) - scale;
}

void DrawBitmapText(SDL_Renderer* renderer, const std::string& text, float x, float y, float scale, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    float cursorX = x;

    for (char c : text) {
        const auto& rows = GlyphRows(c);
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 4; ++col) {
                if (rows[row][col] != '1') {
                    continue;
                }
                SDL_FRect px{
                    cursorX + (static_cast<float>(col) * scale),
                    y + (static_cast<float>(row) * scale),
                    scale,
                    scale
                };
                SDL_RenderFillRect(renderer, &px);
            }
        }
        cursorX += kGlyphAdvance * scale;
    }
}

std::vector<std::string> WrapText(const std::string& text, std::size_t maxChars) {
    std::vector<std::string> lines;
    std::string current;
    std::string word;

    auto flushWord = [&]() {
        if (word.empty()) {
            return;
        }
        if (current.empty()) {
            current = word;
        } else if ((current.size() + 1U + word.size()) <= maxChars) {
            current += " " + word;
        } else {
            lines.push_back(current);
            current = word;
        }
        word.clear();
    };

    for (char c : text) {
        if (c == ' ') {
            flushWord();
        } else {
            word.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }

    flushWord();
    if (!current.empty()) {
        lines.push_back(current);
    }
    if (lines.empty()) {
        lines.push_back("");
    }
    return lines;
}

}
