#include "ai/NpcChatSystem.h"

#include "ai/AiDebugLog.h"
#include "ai/LocalLLMClient.h"
#include "ai/NpcCommandAI.h"

namespace npc_chat_system {

namespace {
constexpr float kNpcHearingRange = 92.0f;
constexpr float kPetHearingRange = 108.0f;

float DistSq(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return (dx * dx) + (dy * dy);
}

bool CanHearPet(const Pet& pet, const Player& player) {
    return DistSq(pet.CenterX(), pet.FeetY(), player.CenterX(), player.FeetY()) <= (kPetHearingRange * kPetHearingRange);
}

bool CanHearNpc(const NpcAI& npc, const Player& player) {
    return DistSq(npc.CenterX(), npc.FeetY(), player.CenterX(), player.FeetY()) <= (kNpcHearingRange * kNpcHearingRange);
}

bool IsExactIntent(const std::string& intent, std::initializer_list<const char*> validIntents) {
    for (const char* valid : validIntents) {
        if (intent == valid) {
            return true;
        }
    }
    return false;
}

bool IsClearLocalCommandReply(const std::string& reply) {
    return reply.find("UNCLEAR") == std::string::npos;
}
}

CommandResult HandlePlayerChatCommand(const std::string& chatInput,
                                      int nearbyNpcIndex,
                                      std::vector<NpcAI>& npcs,
                                      Pet& miko,
                                      const Player& player,
                                      const std::vector<HostileAI>& hostiles,
                                      bool aiBackendReady,
                                      const AiRuntimeManager::Config& aiConfig,
                                      std::optional<AiFollowupRequest>& outAiFollowup) {
    outAiFollowup.reset();

    const std::string upperMessage = npc_command_ai::ToUpperCopy(chatInput);
    ai_debug_log::Write("CHAT_IN message=\"" + chatInput + "\" nearbyNpcIndex=" + std::to_string(nearbyNpcIndex));

    const bool petTargeted = miko.MatchesName(upperMessage);
    if (petTargeted) {
        if (!CanHearPet(miko, player)) {
            ai_debug_log::Write("ROUTE target=MIKO state=too_far");
            const std::string reply = miko.Name() + " is too far away to hear you.";
            return CommandResult{reply, reply, 2.2f, true, false, false};
        }
        ai_debug_log::Write("ROUTE target=MIKO");
        std::string reply = miko.ApplyPlayerCommand(chatInput, player.CenterX(), player.FeetY());
        ai_debug_log::Write("GAME_APPLY_LOCAL target=MIKO action_reply=\"" + reply + "\"");
        const bool localRecognized = IsClearLocalCommandReply(reply);

        if (aiBackendReady && !localRecognized) {
            outAiFollowup = AiFollowupRequest{
                true,
                -1,
                player.CenterX(),
                player.FeetY(),
                miko.Name(),
                npc_command_ai::ResolveRuntimeModelName(aiConfig),
                npc_command_ai::BuildPetCommandPrompt(miko, chatInput, player, hostiles),
                aiConfig.baseUrl};
            ai_debug_log::Write("AI_ENQUEUED target=MIKO");
            const std::string pendingText = miko.Name() + " is thinking...";
            return CommandResult{pendingText, "AI pending | " + pendingText, 1.6f, true, true, false};
        } else {
            ai_debug_log::Write(
                "AI_SKIPPED target=MIKO reason=" +
                std::string(!aiBackendReady ? "backend_not_ready" : "local_command_recognized"));
        }

        return CommandResult{reply, reply, 2.4f, true, false, true};
    }

    int namedNpcIndex = -1;
    for (std::size_t i = 0; i < npcs.size(); ++i) {
        if (npcs[i].MatchesName(upperMessage)) {
            namedNpcIndex = static_cast<int>(i);
            break;
        }
    }

    int commandNpcIndex = namedNpcIndex;
    bool routeToNearestPet = false;
    if (commandNpcIndex < 0 && npc_command_ai::LooksLikeNpcCommand(upperMessage)) {
        float bestDistSq = DistSq(miko.CenterX(), miko.FeetY(), player.CenterX(), player.FeetY());
        routeToNearestPet = true;

        for (std::size_t i = 0; i < npcs.size(); ++i) {
            if (!npcs[i].IsAlive()) {
                continue;
            }
            if (!CanHearNpc(npcs[i], player)) {
                continue;
            }
            const float npcDistSq = DistSq(npcs[i].CenterX(), npcs[i].FeetY(), player.CenterX(), player.FeetY());
            if (npcDistSq < bestDistSq) {
                bestDistSq = npcDistSq;
                commandNpcIndex = static_cast<int>(i);
                routeToNearestPet = false;
            }
        }

        if (commandNpcIndex < 0 && nearbyNpcIndex >= 0 && nearbyNpcIndex < static_cast<int>(npcs.size()) &&
            npcs[static_cast<std::size_t>(nearbyNpcIndex)].IsAlive() &&
            CanHearNpc(npcs[static_cast<std::size_t>(nearbyNpcIndex)], player)) {
            commandNpcIndex = nearbyNpcIndex;
            routeToNearestPet = false;
        }
    }

    if (!npc_command_ai::LooksLikeNpcCommand(upperMessage)) {
        ai_debug_log::Write(
            "ROUTE_NONE looks_like_command=" + std::string(npc_command_ai::LooksLikeNpcCommand(upperMessage) ? "true" : "false"));
        return CommandResult{
            std::string(),
            "You said: " + chatInput,
            1.4f,
            false,
            false,
            false};
    }

    if (routeToNearestPet) {
        ai_debug_log::Write("ROUTE target=MIKO source=nearest");
        std::string reply = miko.ApplyPlayerCommand(chatInput, player.CenterX(), player.FeetY());
        ai_debug_log::Write("GAME_APPLY_LOCAL target=MIKO action_reply=\"" + reply + "\"");
        const bool localRecognized = IsClearLocalCommandReply(reply);

        if (aiBackendReady && !localRecognized) {
            outAiFollowup = AiFollowupRequest{
                true,
                -1,
                player.CenterX(),
                player.FeetY(),
                miko.Name(),
                npc_command_ai::ResolveRuntimeModelName(aiConfig),
                npc_command_ai::BuildPetCommandPrompt(miko, chatInput, player, hostiles),
                aiConfig.baseUrl};
            ai_debug_log::Write("AI_ENQUEUED target=MIKO");
            const std::string pendingText = miko.Name() + " is thinking...";
            return CommandResult{pendingText, "AI pending | " + pendingText, 1.6f, true, true, false};
        }

        ai_debug_log::Write(
            "AI_SKIPPED target=MIKO reason=" +
            std::string(!aiBackendReady ? "backend_not_ready" : "local_command_recognized"));
        return CommandResult{reply, reply, 2.4f, true, false, true};
    }

    if (commandNpcIndex < 0) {
        ai_debug_log::Write("ROUTE_NONE reason=no_available_target");
        return CommandResult{
            std::string(),
            "No nearby bot can take that order",
            1.8f,
            false,
            false,
            false};
    }

    NpcAI& targetNpc = npcs[static_cast<std::size_t>(commandNpcIndex)];
    if (!targetNpc.IsAlive()) {
        ai_debug_log::Write("ROUTE target_npc=\"" + targetNpc.Name() + "\" state=dead");
        const std::string message = targetNpc.Name() + " cannot follow any order now.";
        return CommandResult{message, message, 2.2f, true, false, false};
    }
    if (!CanHearNpc(targetNpc, player)) {
        ai_debug_log::Write("ROUTE target_npc=\"" + targetNpc.Name() + "\" state=too_far");
        const std::string message = targetNpc.Name() + " is too far away to hear you.";
        return CommandResult{message, message, 2.2f, true, false, false};
    }

    ai_debug_log::Write(
        "ROUTE target_npc=\"" + targetNpc.Name() + "\" index=" + std::to_string(commandNpcIndex) +
        (namedNpcIndex < 0 ? " source=nearest" : " source=named"));
    std::string reply = targetNpc.ApplyPlayerCommand(chatInput, player.CenterX(), player.FeetY());
    ai_debug_log::Write("GAME_APPLY_LOCAL target_npc=\"" + targetNpc.Name() + "\" action_reply=\"" + reply + "\"");
    const bool localRecognized = IsClearLocalCommandReply(reply);

    if (aiBackendReady && !localRecognized) {
        outAiFollowup = AiFollowupRequest{
            false,
            commandNpcIndex,
            player.CenterX(),
            player.FeetY(),
            targetNpc.Name(),
            npc_command_ai::ResolveRuntimeModelName(aiConfig),
            npc_command_ai::BuildNpcCommandPrompt(targetNpc, chatInput, player, hostiles),
            aiConfig.baseUrl};
        ai_debug_log::Write("AI_ENQUEUED target_npc=\"" + targetNpc.Name() + "\"");
    } else {
        ai_debug_log::Write(
            "AI_SKIPPED target_npc=\"" + targetNpc.Name() + "\" reason=" +
            std::string(!aiBackendReady ? "backend_not_ready" : "local_command_recognized"));
    }

    return CommandResult{reply, reply, 2.4f, true, outAiFollowup.has_value(), false};
}

AiFollowupResult ExecuteAiFollowup(const AiFollowupRequest& request) {
    ai_debug_log::Write(
        std::string("AI_REQUEST target=") + (request.targetMiko ? "MIKO" : request.targetName) +
        " model=\"" + request.modelName + "\" prompt=\"" + request.prompt + "\"");

    LocalLLMClient client(request.baseUrl);
    const std::string bodyJson =
        "{\"model\":\"" + LocalLLMClient::EscapeJson(request.modelName) +
        "\",\"messages\":[{\"role\":\"system\",\"content\":\"Reply with strict JSON only.\"},"
        "{\"role\":\"user\",\"content\":\"" + LocalLLMClient::EscapeJson(request.prompt) +
        "\"}],\"temperature\":0.2,\"stream\":false}";

    const LocalLLMClient::Response response =
        client.PostJson(LocalLLMClient::Request{"/v1/chat/completions", bodyJson, 12000});

    ai_debug_log::Write(
        std::string("AI_RESPONSE target=") + (request.targetMiko ? "MIKO" : request.targetName) +
        " ok=" + std::string(response.ok ? "true" : "false") +
        " status=" + std::to_string(response.statusCode) +
        " error=\"" + response.error + "\" body=\"" + response.body + "\"");

    AiFollowupResult result{request.targetMiko, request.targetNpcIndex, request.targetName, false, std::string(), std::string(), response.error};
    if (!response.ok) {
        return result;
    }

    const std::string content = npc_command_ai::ExtractJsonStringValue(response.body, "content");
    const std::string speech = npc_command_ai::ExtractJsonStringValue(content, "speech");
    const std::string intent = npc_command_ai::ExtractJsonStringValue(content, "intent");

    ai_debug_log::Write(
        std::string("AI_PARSE target=") + (request.targetMiko ? "MIKO" : request.targetName) +
        " content=\"" + content + "\" speech=\"" + speech + "\" intent=\"" + intent + "\"");

    result.ok = true;
    result.speech = speech;
    result.intent = intent;
    return result;
}

}
