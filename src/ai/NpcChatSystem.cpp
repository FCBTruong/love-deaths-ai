#include "ai/NpcChatSystem.h"

#include "ai/LocalLLMClient.h"
#include "ai/NpcCommandAI.h"

namespace npc_chat_system {

CommandResult HandlePlayerChatCommand(const std::string& chatInput,
                                      int nearbyNpcIndex,
                                      std::vector<NpcAI>& npcs,
                                      const Player& player,
                                      const std::vector<HostileAI>& hostiles,
                                      bool aiBackendReady,
                                      const AiRuntimeManager::Config& aiConfig) {
    const std::string upperMessage = npc_command_ai::ToUpperCopy(chatInput);
    int commandNpcIndex = -1;
    for (std::size_t i = 0; i < npcs.size(); ++i) {
        if (npcs[i].IsAlive() && npcs[i].MatchesName(upperMessage)) {
            commandNpcIndex = static_cast<int>(i);
            break;
        }
    }

    if (commandNpcIndex < 0 && nearbyNpcIndex >= 0) {
        commandNpcIndex = nearbyNpcIndex;
    }

    if (commandNpcIndex < 0 || !npc_command_ai::LooksLikeNpcCommand(upperMessage)) {
        return CommandResult{
            std::string(),
            npc_command_ai::LooksLikeNpcCommand(upperMessage) ? "Name a bot or stand near one" : "You said: " + chatInput,
            npc_command_ai::LooksLikeNpcCommand(upperMessage) ? 2.0f : 1.4f,
            false};
    }

    NpcAI& targetNpc = npcs[static_cast<std::size_t>(commandNpcIndex)];
    std::string reply = targetNpc.ApplyPlayerCommand(chatInput, player.CenterX(), player.FeetY());
    std::string status = reply;
    float statusTimer = 2.4f;

    if (aiBackendReady) {
        LocalLLMClient client(aiConfig.baseUrl);
        const std::string modelName = npc_command_ai::ResolveRuntimeModelName(aiConfig);
        const std::string prompt = npc_command_ai::BuildNpcCommandPrompt(targetNpc, chatInput, player, hostiles);
        const std::string bodyJson =
            "{\"model\":\"" + LocalLLMClient::EscapeJson(modelName) +
            "\",\"messages\":[{\"role\":\"system\",\"content\":\"Reply with strict JSON only.\"},"
            "{\"role\":\"user\",\"content\":\"" + LocalLLMClient::EscapeJson(prompt) +
            "\"}],\"temperature\":0.2,\"stream\":false}";

        const LocalLLMClient::Response response =
            client.PostJson(LocalLLMClient::Request{"/v1/chat/completions", bodyJson, 12000});
        if (response.ok) {
            const std::string content = npc_command_ai::ExtractJsonStringValue(response.body, "content");
            const std::string speech = npc_command_ai::ExtractJsonStringValue(content, "speech");
            if (!speech.empty()) {
                targetNpc.SetAiSpeech(speech);
                reply = speech;
                status = "CMD OK | " + speech;
                statusTimer = 3.0f;
            }
        }
    }

    return CommandResult{reply, status, statusTimer, true};
}

}
