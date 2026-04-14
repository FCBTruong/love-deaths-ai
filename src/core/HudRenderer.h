#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "core/Camera2D.h"
#include "game/HostileAI.h"
#include "game/NpcAI.h"
#include "game/Player.h"

namespace hud_renderer {

void DrawNpcBubbles(SDL_Renderer* renderer, const Camera2D& camera, const SDL_FRect& destination,
                    const std::vector<NpcAI>& npcs, int virtualWidth);

void DrawPlayerChatBubble(SDL_Renderer* renderer, const Camera2D& camera, const SDL_FRect& destination, const Player& player,
                          const std::string& playerChatText, float playerChatTimer, int virtualWidth);

void DrawNpcLocators(SDL_Renderer* renderer, const Camera2D& camera, const SDL_FRect& destination,
                     const std::vector<NpcAI>& npcs, int nearbyNpcIndex, int virtualWidth);

void DrawUI(SDL_Renderer* renderer, const SDL_FRect& destination, int virtualWidth, const Player& player,
            int appleCount, int berryCount, int pearCount, int meatCount, const std::vector<HostileAI>& hostiles,
            const std::vector<NpcAI>& npcs, int nearbyNpcIndex, bool aiBackendReady, const std::string& aiBackendStatus,
            bool chatMode, int chatNpcIndex, const std::string& chatReply, const std::string& chatInput,
            const std::string& heldItemLabel, const std::string& phaseLabel, const std::string& objectiveLabel);

}
