#pragma once

#include <string>
#include <vector>

#include "ai/AiRuntimeManager.h"
#include "game/HostileAI.h"
#include "game/NpcAI.h"
#include "game/Player.h"

namespace npc_chat_system {

struct CommandResult {
    std::string chatReply;
    std::string statusText;
    float statusTimer;
    bool handled;
};

CommandResult HandlePlayerChatCommand(const std::string& chatInput,
                                      int nearbyNpcIndex,
                                      std::vector<NpcAI>& npcs,
                                      const Player& player,
                                      const std::vector<HostileAI>& hostiles,
                                      bool aiBackendReady,
                                      const AiRuntimeManager::Config& aiConfig);

}
