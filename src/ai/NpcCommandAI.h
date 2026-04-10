#pragma once

#include <string>
#include <vector>

#include "ai/AiRuntimeManager.h"
#include "game/HostileAI.h"
#include "game/NpcAI.h"
#include "game/Pet.h"
#include "game/Player.h"

namespace npc_command_ai {

std::string ToUpperCopy(const std::string& value);
bool LooksLikeNpcCommand(const std::string& upper);
std::string ExtractJsonStringValue(const std::string& source, const std::string& key);
std::string BuildNpcCommandPrompt(const NpcAI& npc, const std::string& playerMessage, const Player& player,
                                  const std::vector<HostileAI>& hostiles);
std::string BuildPetCommandPrompt(const Pet& pet, const std::string& playerMessage, const Player& player,
                                  const std::vector<HostileAI>& hostiles);
std::string ResolveRuntimeModelName(const AiRuntimeManager::Config& config);

}
