#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ai/AiRuntimeManager.h"
#include "game/HostileAI.h"
#include "game/NpcAI.h"
#include "game/Pet.h"
#include "game/Player.h"

namespace npc_chat_system {

struct CommandResult {
    std::string chatReply;
    std::string statusText;
    float statusTimer;
    bool handled;
    bool startAiFollowup;
    bool playMikoSound;
};

struct AiFollowupRequest {
    bool targetMiko;
    int targetNpcIndex;
    float playerX;
    float playerY;
    std::string targetName;
    std::string modelName;
    std::string prompt;
    std::string baseUrl;
};

struct AiFollowupResult {
    bool targetMiko;
    int targetNpcIndex;
    std::string targetName;
    bool ok;
    std::string speech;
    std::string intent;
    std::string error;
};

CommandResult HandlePlayerChatCommand(const std::string& chatInput,
                                      int nearbyNpcIndex,
                                      std::vector<NpcAI>& npcs,
                                      Pet& miko,
                                      const Player& player,
                                      const std::vector<HostileAI>& hostiles,
                                      bool aiBackendReady,
                                      const AiRuntimeManager::Config& aiConfig,
                                      std::optional<AiFollowupRequest>& outAiFollowup);

AiFollowupResult ExecuteAiFollowup(const AiFollowupRequest& request);

}
