#include "core/PixelFont.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace {
void DrawDigit(SDL_Renderer* renderer, int digit, float x, float y, float s, SDL_Color color) {
    static const int kMasks[10] = {
        0b111101101101111,
        0b010010010010010,
        0b111001111100111,
        0b111001111001111,
        0b101101111001001,
        0b111100111001111,
        0b111100111101111,
        0b111001001001001,
        0b111101111101111,
        0b111101111001111
    };

    const int mask = kMasks[std::clamp(digit, 0, 9)];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {
            const int bit = 14 - ((row * 3) + col);
            if (((mask >> bit) & 1) == 0) {
                continue;
            }
            SDL_FRect p{x + (static_cast<float>(col) * s), y + (static_cast<float>(row) * s), s, s};
            SDL_RenderFillRect(renderer, &p);
        }
    }
}

const std::array<const char*, 5>& GlyphRows(char c) {
    static const std::array<const char*, 5> space{"000", "000", "000", "000", "000"};
    static const std::array<const char*, 5> dot{"000", "000", "000", "010", "010"};
    static const std::array<const char*, 5> comma{"000", "000", "000", "010", "100"};
    static const std::array<const char*, 5> dash{"000", "000", "111", "000", "000"};
    static const std::array<const char*, 5> colon{"000", "010", "000", "010", "000"};
    static const std::array<const char*, 5> bang{"010", "010", "010", "000", "010"};
    static const std::array<const char*, 5> apos{"010", "010", "000", "000", "000"};
    static const std::array<const char*, 5> at{"111", "101", "111", "101", "110"};

    static const std::array<const char*, 5> a{"010", "101", "111", "101", "101"};
    static const std::array<const char*, 5> b{"110", "101", "110", "101", "110"};
    static const std::array<const char*, 5> c0{"011", "100", "100", "100", "011"};
    static const std::array<const char*, 5> d{"110", "101", "101", "101", "110"};
    static const std::array<const char*, 5> e{"111", "100", "110", "100", "111"};
    static const std::array<const char*, 5> f{"111", "100", "110", "100", "100"};
    static const std::array<const char*, 5> g{"011", "100", "101", "101", "011"};
    static const std::array<const char*, 5> h{"101", "101", "111", "101", "101"};
    static const std::array<const char*, 5> i{"111", "010", "010", "010", "111"};
    static const std::array<const char*, 5> j{"001", "001", "001", "101", "010"};
    static const std::array<const char*, 5> k{"101", "101", "110", "101", "101"};
    static const std::array<const char*, 5> l{"100", "100", "100", "100", "111"};
    static const std::array<const char*, 5> m{"101", "111", "111", "101", "101"};
    static const std::array<const char*, 5> n{"101", "111", "111", "111", "101"};
    static const std::array<const char*, 5> o{"111", "101", "101", "101", "111"};
    static const std::array<const char*, 5> p{"110", "101", "110", "100", "100"};
    static const std::array<const char*, 5> q{"111", "101", "101", "111", "001"};
    static const std::array<const char*, 5> r{"110", "101", "110", "101", "101"};
    static const std::array<const char*, 5> s{"011", "100", "111", "001", "110"};
    static const std::array<const char*, 5> t{"111", "010", "010", "010", "010"};
    static const std::array<const char*, 5> u{"101", "101", "101", "101", "111"};
    static const std::array<const char*, 5> v{"101", "101", "101", "101", "010"};
    static const std::array<const char*, 5> w{"101", "101", "111", "111", "101"};
    static const std::array<const char*, 5> x{"101", "101", "010", "101", "101"};
    static const std::array<const char*, 5> y{"101", "101", "010", "010", "010"};
    static const std::array<const char*, 5> z{"111", "001", "010", "100", "111"};

    static const std::array<const char*, 5> n0{"111", "101", "101", "101", "111"};
    static const std::array<const char*, 5> n1{"010", "110", "010", "010", "111"};
    static const std::array<const char*, 5> n2{"111", "001", "111", "100", "111"};
    static const std::array<const char*, 5> n3{"111", "001", "111", "001", "111"};
    static const std::array<const char*, 5> n4{"101", "101", "111", "001", "001"};
    static const std::array<const char*, 5> n5{"111", "100", "111", "001", "111"};
    static const std::array<const char*, 5> n6{"111", "100", "111", "101", "111"};
    static const std::array<const char*, 5> n7{"111", "001", "010", "100", "100"};
    static const std::array<const char*, 5> n8{"111", "101", "111", "101", "111"};
    static const std::array<const char*, 5> n9{"111", "101", "111", "001", "111"};

    switch (std::toupper(static_cast<unsigned char>(c))) {
    case 'A': return a;
    case 'B': return b;
    case 'C': return c0;
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
}

namespace pixel_font {

void DrawNumber(SDL_Renderer* renderer, int value, float x, float y, float scale, SDL_Color color) {
    const int safeValue = std::max(0, value);
    if (safeValue < 10) {
        DrawDigit(renderer, safeValue, x, y, scale, color);
        return;
    }

    const int tens = (safeValue / 10) % 10;
    const int ones = safeValue % 10;
    DrawDigit(renderer, tens, x, y, scale, color);
    DrawDigit(renderer, ones, x + (4.0f * scale), y, scale, color);
}

float MeasureText(const std::string& text, float scale) {
    if (text.empty()) {
        return 0.0f;
    }
    return static_cast<float>(text.size()) * (4.0f * scale) - scale;
}

void DrawBitmapText(SDL_Renderer* renderer, const std::string& text, float x, float y, float scale, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    float cursorX = x;

    for (char c : text) {
        const auto& rows = GlyphRows(c);
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
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
        cursorX += 4.0f * scale;
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
