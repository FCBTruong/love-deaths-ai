#include "ai/NpcCommandAI.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>

namespace npc_command_ai {

std::string ToUpperCopy(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

bool LooksLikeNpcCommand(const std::string& upper) {
    return upper.find("FOLLOW") != std::string::npos || upper.find("THEO") != std::string::npos ||
           upper.find("SIT") != std::string::npos || upper.find("NGOI") != std::string::npos ||
           upper.find("STOP") != std::string::npos || upper.find("STAY") != std::string::npos ||
           upper.find("DUNG") != std::string::npos || upper.find("HERE") != std::string::npos ||
           upper.find("COME") != std::string::npos || upper.find("LAI") != std::string::npos ||
           upper.find("HOME") != std::string::npos || upper.find("RETURN") != std::string::npos ||
           upper.find("BACK") != std::string::npos || upper.find("VE") != std::string::npos;
}

std::string ExtractJsonStringValue(const std::string& source, const std::string& key) {
    const std::string pattern = "\"" + key + "\"";
    const std::size_t keyPos = source.find(pattern);
    if (keyPos == std::string::npos) {
        return std::string();
    }

    const std::size_t colonPos = source.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos) {
        return std::string();
    }

    const std::size_t firstQuote = source.find('"', colonPos + 1);
    if (firstQuote == std::string::npos) {
        return std::string();
    }

    std::string out;
    bool escaped = false;
    for (std::size_t i = firstQuote + 1; i < source.size(); ++i) {
        const char c = source[i];
        if (escaped) {
            switch (c) {
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case '\\':
            case '"':
            case '/':
                out.push_back(c);
                break;
            default:
                out.push_back(c);
                break;
            }
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (c == '"') {
            break;
        }
        out.push_back(c);
    }

    return out;
}

std::string BuildNpcCommandPrompt(
    const NpcAI& npc,
    const std::string& playerMessage,
    const Player& player,
    const std::vector<HostileAI>& hostiles
) {
    int nearbyHostiles = 0;
    for (const HostileAI& hostile : hostiles) {
        if (!hostile.IsAlive()) {
            continue;
        }
        const float dx = hostile.CenterX() - npc.CenterX();
        const float dy = hostile.CenterY() - npc.CenterY();
        if ((dx * dx) + (dy * dy) <= (56.0f * 56.0f)) {
            ++nearbyHostiles;
        }
    }

    std::ostringstream prompt;
    prompt << "You are " << npc.Name() << ", a villager in a survival game. "
           << "Your role is " << npc.Role() << ". "
           << "Your personality trait is " << npc.Trait() << ". "
           << "Your focus is " << npc.Focus() << ". "
           << "The player is speaking to you directly.\n"
           << "Player distance: "
           << static_cast<int>(std::lround(std::sqrt(
                  ((npc.CenterX() - player.CenterX()) * (npc.CenterX() - player.CenterX())) +
                  ((npc.FeetY() - player.FeetY()) * (npc.FeetY() - player.FeetY())))))
           << "\n"
           << "Nearby hostiles: " << nearbyHostiles << "\n"
           << "Player message: \"" << playerMessage << "\"\n"
           << "Choose exactly one intent token that best matches the player's request.\n"
           << "Valid intent tokens: follow_player, hold_position, come_to_player, return_home, wander, talk\n"
           << "Use short natural speech as the reply you would say out loud.\n"
           << "Reply with strict JSON only using this schema:\n"
           << "{\"thought\":\"...\",\"speech\":\"...\",\"intent\":\"follow_player\",\"target\":\"player or self\"}";
    return prompt.str();
}

std::string BuildPetCommandPrompt(
    const Pet& pet,
    const std::string& playerMessage,
    const Player& player,
    const std::vector<HostileAI>& hostiles
) {
    int nearbyHostiles = 0;
    for (const HostileAI& hostile : hostiles) {
        if (!hostile.IsAlive()) {
            continue;
        }
        const float dx = hostile.CenterX() - pet.CenterX();
        const float dy = hostile.CenterY() - pet.CenterY();
        if ((dx * dx) + (dy * dy) <= (56.0f * 56.0f)) {
            ++nearbyHostiles;
        }
    }

    std::ostringstream prompt;
    prompt << "You are " << pet.Name() << ", a loyal border collie companion in a survival game. "
           << "The player is speaking directly to you.\n"
           << "Player distance: "
           << static_cast<int>(std::lround(std::sqrt(
                  ((pet.CenterX() - player.CenterX()) * (pet.CenterX() - player.CenterX())) +
                  ((pet.FeetY() - player.FeetY()) * (pet.FeetY() - player.FeetY())))))
           << "\n"
           << "Nearby hostiles: " << nearbyHostiles << "\n"
           << "Player message: \"" << playerMessage << "\"\n"
           << "Choose exactly one intent token that best matches the player's request.\n"
           << "Valid intent tokens: follow_player, sit_down, hold_position, come_to_player, return_home, idle\n"
           << "Examples:\n"
           << "- \"follow me\" -> follow_player\n"
           << "- \"go with me\" -> follow_player\n"
           << "- \"come with me\" -> follow_player\n"
           << "- \"come here\" -> come_to_player\n"
           << "- \"stay here\" -> hold_position\n"
           << "- \"sit\" -> sit_down\n"
           << "Reply like a smart pet companion with short natural speech.\n"
           << "Reply with strict JSON only using this schema:\n"
           << "{\"thought\":\"...\",\"speech\":\"...\",\"intent\":\"<one_valid_token>\",\"target\":\"player or self\"}";
    return prompt.str();
}

std::string ResolveRuntimeModelName(const AiRuntimeManager::Config& config) {
    if (!config.modelName.empty()) {
        return config.modelName;
    }
    if (!config.modelPath.empty()) {
        return std::filesystem::path(config.modelPath).filename().string();
    }
    return "npc-brain.gguf";
}

}
