#include "ai/AiDebugLog.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace ai_debug_log {

void Write(const std::string& message) {
    namespace fs = std::filesystem;

    fs::path root = fs::current_path();
    if (const char* basePathRaw = SDL_GetBasePath(); basePathRaw != nullptr) {
        root = fs::path(basePathRaw).parent_path();
    }

    const fs::path logDir = root / "logs";
    std::error_code ec;
    fs::create_directories(logDir, ec);

    std::ofstream out(logDir / "ai-command.log", std::ios::app);
    if (!out) {
        return;
    }

    out << "[" << SDL_GetTicks() << "] " << message << '\n';
}

}
