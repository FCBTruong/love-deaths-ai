#pragma once

#include <SDL3/SDL.h>

#include <cstddef>
#include <string>
#include <vector>

namespace pixel_font {

void DrawNumber(SDL_Renderer* renderer, int value, float x, float y, float scale, SDL_Color color);
float MeasureText(const std::string& text, float scale);
void DrawBitmapText(SDL_Renderer* renderer, const std::string& text, float x, float y, float scale, SDL_Color color);
std::vector<std::string> WrapText(const std::string& text, std::size_t maxChars);

}
