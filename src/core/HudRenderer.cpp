#include "core/HudRenderer.h"

#include <algorithm>
#include <cmath>

#include "core/PixelFont.h"

namespace {
std::string FormatSignedInt(int value) {
    if (value >= 0) {
        return std::to_string(value);
    }
    return "-" + std::to_string(-value);
}
}

namespace hud_renderer {

void DrawNpcBubbles(SDL_Renderer* renderer, const Camera2D& camera, const SDL_FRect& destination,
                    const std::vector<NpcAI>& npcs, int virtualWidth) {
    const float scale = std::max(1.0f, std::floor(destination.w / static_cast<float>(virtualWidth)));

    for (const NpcAI& npc : npcs) {
        if (!npc.HasBubble()) {
            continue;
        }

        const std::vector<std::string> lines = pixel_font::WrapText(npc.BubbleText(), 18);
        float maxWidth = 0.0f;
        for (const std::string& line : lines) {
            maxWidth = std::max(maxWidth, pixel_font::MeasureText(line, scale));
        }

        const float bubbleW = std::max(38.0f * scale, maxWidth + (8.0f * scale));
        const float bubbleH = (static_cast<float>(lines.size()) * 6.0f * scale) + (6.0f * scale);
        const float screenX = destination.x + ((npc.CenterX() - camera.x) * scale);
        const float screenY = destination.y + ((npc.Y() - camera.y) * scale) - (18.0f * scale) - bubbleH;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 247, 244, 233, 234);
        SDL_FRect bubble{screenX - (bubbleW * 0.5f), screenY, bubbleW, bubbleH};
        SDL_RenderFillRect(renderer, &bubble);

        SDL_SetRenderDrawColor(renderer, 78, 67, 46, 255);
        SDL_FRect edgeTop{bubble.x, bubble.y, bubble.w, 1.0f * scale};
        SDL_FRect edgeLeft{bubble.x, bubble.y, 1.0f * scale, bubble.h};
        SDL_RenderFillRect(renderer, &edgeTop);
        SDL_RenderFillRect(renderer, &edgeLeft);

        SDL_FRect tail{screenX - (1.0f * scale), bubble.y + bubble.h, 2.0f * scale, 3.0f * scale};
        SDL_RenderFillRect(renderer, &tail);

        for (std::size_t i = 0; i < lines.size(); ++i) {
            pixel_font::DrawBitmapText(renderer, lines[i], bubble.x + (4.0f * scale),
                                       bubble.y + (3.0f * scale) + (static_cast<float>(i) * 6.0f * scale), scale,
                                       SDL_Color{36, 34, 30, 255});
        }
    }
}

void DrawPlayerChatBubble(SDL_Renderer* renderer, const Camera2D& camera, const SDL_FRect& destination, const Player& player,
                          const std::string& playerChatText, float playerChatTimer, int virtualWidth) {
    if (playerChatText.empty() || playerChatTimer <= 0.0f) {
        return;
    }

    const float scale = std::max(1.0f, std::floor(destination.w / static_cast<float>(virtualWidth)));
    const std::vector<std::string> lines = pixel_font::WrapText(playerChatText, 18);
    float maxWidth = 0.0f;
    for (const std::string& line : lines) {
        maxWidth = std::max(maxWidth, pixel_font::MeasureText(line, scale));
    }

    const float bubbleW = std::max(40.0f * scale, maxWidth + (8.0f * scale));
    const float bubbleH = (static_cast<float>(lines.size()) * 6.0f * scale) + (6.0f * scale);
    const float screenX = destination.x + ((player.CenterX() - camera.x) * scale);
    const float screenY = destination.y + ((player.Y() - camera.y) * scale) - (18.0f * scale) - bubbleH;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 242, 248, 255, 234);
    SDL_FRect bubble{screenX - (bubbleW * 0.5f), screenY, bubbleW, bubbleH};
    SDL_RenderFillRect(renderer, &bubble);

    SDL_SetRenderDrawColor(renderer, 42, 54, 72, 255);
    SDL_FRect edgeTop{bubble.x, bubble.y, bubble.w, 1.0f * scale};
    SDL_FRect edgeLeft{bubble.x, bubble.y, 1.0f * scale, bubble.h};
    SDL_RenderFillRect(renderer, &edgeTop);
    SDL_RenderFillRect(renderer, &edgeLeft);

    SDL_FRect tail{screenX - (1.0f * scale), bubble.y + bubble.h, 2.0f * scale, 3.0f * scale};
    SDL_RenderFillRect(renderer, &tail);

    for (std::size_t i = 0; i < lines.size(); ++i) {
        pixel_font::DrawBitmapText(renderer, lines[i], bubble.x + (4.0f * scale),
                                   bubble.y + (3.0f * scale) + (static_cast<float>(i) * 6.0f * scale), scale,
                                   SDL_Color{32, 38, 45, 255});
    }
}

void DrawNpcLocators(SDL_Renderer* renderer, const Camera2D& camera, const SDL_FRect& destination,
                     const std::vector<NpcAI>& npcs, int nearbyNpcIndex, int virtualWidth) {
    const float scale = std::max(1.0f, std::floor(destination.w / static_cast<float>(virtualWidth)));
    const float left = destination.x + (4.0f * scale);
    const float right = destination.x + destination.w - (4.0f * scale);
    const float top = destination.y + (4.0f * scale);
    const float bottom = destination.y + destination.h - (4.0f * scale);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (std::size_t index = 0; index < npcs.size(); ++index) {
        const NpcAI& npc = npcs[index];
        float markerX = destination.x + ((npc.CenterX() - camera.x) * scale);
        float markerY = destination.y + ((npc.Y() - camera.y) * scale) - (6.0f * scale);

        markerX = std::clamp(markerX, left, right);
        markerY = std::clamp(markerY, top, bottom);

        const bool nearest = static_cast<int>(index) == nearbyNpcIndex;
        SDL_SetRenderDrawColor(renderer, nearest ? 255 : 244, nearest ? 226 : 193, nearest ? 122 : 96, 245);
        SDL_FRect pin{markerX - (1.0f * scale), markerY - (1.0f * scale), 3.0f * scale, 3.0f * scale};
        SDL_RenderFillRect(renderer, &pin);

        SDL_SetRenderDrawColor(renderer, 61, 42, 26, 255);
        SDL_FRect stem{markerX, markerY + (2.0f * scale), 1.0f * scale, 3.0f * scale};
        SDL_RenderFillRect(renderer, &stem);

        pixel_font::DrawBitmapText(renderer, npc.Name(), markerX - (pixel_font::MeasureText(npc.Name(), scale) * 0.5f),
                                   markerY - (7.0f * scale), scale,
                                   nearest ? SDL_Color{255, 244, 212, 255} : SDL_Color{248, 235, 197, 235});
    }
}

void DrawUI(SDL_Renderer* renderer, const SDL_FRect& destination, int virtualWidth, const Player& player,
            int appleCount, int berryCount, int pearCount, int meatCount, const std::vector<HostileAI>& hostiles,
            const std::vector<NpcAI>& npcs, int nearbyNpcIndex, bool aiBackendReady, const std::string& aiBackendStatus,
            bool chatMode, int chatNpcIndex, const std::string& chatReply, const std::string& chatInput,
            const std::string& heldItemLabel) {
    const float scale = std::max(1.0f, std::floor(destination.w / static_cast<float>(virtualWidth)) * 0.5f);

    {
        const float hotbarW = 192.0f * scale;
        const float hotbarH = 16.0f * scale;
        const float hotbarX = destination.x + ((destination.w - hotbarW) * 0.5f);
        const float hotbarY = destination.y + destination.h - hotbarH - (6.0f * scale);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 16, 20, 26, 222);
        SDL_FRect hotbar{hotbarX, hotbarY, hotbarW, hotbarH};
        SDL_RenderFillRect(renderer, &hotbar);

        const auto drawSlot = [&](float x, const char* key, const char* label, bool active) {
            SDL_SetRenderDrawColor(renderer, active ? 206 : 72, active ? 171 : 82, active ? 112 : 94, active ? 255 : 220);
            SDL_FRect slot{x, hotbarY + (2.0f * scale), 36.0f * scale, 12.0f * scale};
            SDL_RenderFillRect(renderer, &slot);

            SDL_SetRenderDrawColor(renderer, active ? 255 : 136, active ? 239 : 148, active ? 204 : 160, 255);
            SDL_FRect top{slot.x, slot.y, slot.w, 1.0f * scale};
            SDL_RenderFillRect(renderer, &top);

            pixel_font::DrawBitmapText(renderer, key, slot.x + (3.0f * scale), slot.y + (2.0f * scale), scale,
                                       SDL_Color{25, 20, 16, 255});
            pixel_font::DrawBitmapText(renderer, label, slot.x + (10.0f * scale), slot.y + (2.0f * scale), scale,
                                       SDL_Color{25, 20, 16, 255});
        };

        drawSlot(hotbarX + (2.0f * scale), "1", "BLADE", heldItemLabel == "BLADE");
        drawSlot(hotbarX + (40.0f * scale), "2", "FENCE", heldItemLabel == "FENCE");
        drawSlot(hotbarX + (78.0f * scale), "3", "SOIL", heldItemLabel == "SOIL");
        drawSlot(hotbarX + (116.0f * scale), "4", "SEED", heldItemLabel == "SEED");
        drawSlot(hotbarX + (154.0f * scale), "5", "ROD", heldItemLabel == "ROD");
    }

    {
        const float bannerW = 118.0f * scale;
        const float bannerH = 10.0f * scale;
        const float bannerX = destination.x + ((destination.w - bannerW) * 0.5f);
        const float bannerY = destination.y + (4.0f * scale);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 22, 28, 36, 228);
        SDL_FRect banner{bannerX, bannerY, bannerW, bannerH};
        SDL_RenderFillRect(renderer, &banner);

        SDL_SetRenderDrawColor(renderer, 234, 198, 118, 255);
        SDL_FRect top{bannerX, bannerY, bannerW, 1.0f * scale};
        SDL_RenderFillRect(renderer, &top);

        pixel_font::DrawBitmapText(renderer,
                                   "NPC " + std::to_string(static_cast<int>(npcs.size())) + "  NEAR " +
                                       std::to_string(std::max(0, nearbyNpcIndex + 1)),
                                   bannerX + (4.0f * scale), bannerY + (2.0f * scale), scale,
                                   SDL_Color{248, 240, 223, 255});
    }

    {
        const float aiW = 126.0f * scale;
        const float aiH = 10.0f * scale;
        const float aiX = destination.x + ((destination.w - aiW) * 0.5f);
        const float aiY = destination.y + (16.0f * scale);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 18, 24, 30, 222);
        SDL_FRect panel{aiX, aiY, aiW, aiH};
        SDL_RenderFillRect(renderer, &panel);

        SDL_SetRenderDrawColor(renderer, aiBackendReady ? 138 : 214, aiBackendReady ? 205 : 142,
                               aiBackendReady ? 152 : 108, 255);
        SDL_FRect top{aiX, aiY, aiW, 1.0f * scale};
        SDL_RenderFillRect(renderer, &top);

        pixel_font::DrawBitmapText(renderer, aiBackendStatus, aiX + (4.0f * scale), aiY + (2.0f * scale), scale,
                                   aiBackendReady ? SDL_Color{220, 243, 226, 255} : SDL_Color{255, 227, 214, 255});
    }

    const float panelX = destination.x + (6.0f * scale);
    const float panelY = destination.y + (6.0f * scale);
    const float panelW = 127.0f * scale;
    const float panelH = 24.0f * scale;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 13, 18, 210);
    SDL_FRect panel{panelX, panelY, panelW, panelH};
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 164, 178, 190, 230);
    SDL_FRect borderTop{panelX, panelY, panelW, 1.0f * scale};
    SDL_FRect borderLeft{panelX, panelY, 1.0f * scale, panelH};
    SDL_RenderFillRect(renderer, &borderTop);
    SDL_RenderFillRect(renderer, &borderLeft);

    const float slotW = 29.0f * scale;
    const float slotH = 18.0f * scale;
    const float slotY = panelY + (3.0f * scale);

    const auto drawResourceSlot = [&](int index, SDL_Color iconColor, int count, bool meatIcon) {
        const float slotX = panelX + (3.0f * scale) + (index * 31.0f * scale);
        SDL_SetRenderDrawColor(renderer, 23, 31, 42, 235);
        SDL_FRect slot{slotX, slotY, slotW, slotH};
        SDL_RenderFillRect(renderer, &slot);

        SDL_SetRenderDrawColor(renderer, iconColor.r, iconColor.g, iconColor.b, 255);
        if (meatIcon) {
            SDL_FRect meatCore{slotX + (3.0f * scale), slotY + (5.0f * scale), 5.0f * scale, 4.0f * scale};
            SDL_RenderFillRect(renderer, &meatCore);
            SDL_SetRenderDrawColor(renderer, 243, 224, 205, 255);
            SDL_FRect bone{slotX + (8.0f * scale), slotY + (6.0f * scale), 2.0f * scale, 2.0f * scale};
            SDL_RenderFillRect(renderer, &bone);
        } else {
            SDL_FRect fruit{slotX + (3.0f * scale), slotY + (5.0f * scale), 4.0f * scale, 4.0f * scale};
            SDL_RenderFillRect(renderer, &fruit);
        }

        pixel_font::DrawNumber(renderer, count, slotX + (10.0f * scale), slotY + (4.0f * scale), 1.0f * scale,
                               SDL_Color{242, 245, 248, 255});
    };

    drawResourceSlot(0, SDL_Color{222, 66, 65, 255}, appleCount, false);
    drawResourceSlot(1, SDL_Color{166, 89, 227, 255}, berryCount, false);
    drawResourceSlot(2, SDL_Color{238, 209, 80, 255}, pearCount, false);
    drawResourceSlot(3, SDL_Color{219, 121, 93, 255}, meatCount, true);

    {
        int zombieCount = 0;
        int darkRaiderCount = 0;
        int monsterCount = 0;
        for (const HostileAI& hostile : hostiles) {
            if (!hostile.IsAlive()) {
                continue;
            }
            if (hostile.Kind() == HostileKind::Zombie) {
                ++zombieCount;
            } else if (hostile.Kind() == HostileKind::Marauder) {
                ++darkRaiderCount;
            } else {
                ++monsterCount;
            }
        }

        const float threatX = panelX;
        const float threatY = panelY + panelH + (4.0f * scale);
        const float threatW = 116.0f * scale;
        const float threatH = 18.0f * scale;
        SDL_SetRenderDrawColor(renderer, 18, 16, 20, 218);
        SDL_FRect threat{threatX, threatY, threatW, threatH};
        SDL_RenderFillRect(renderer, &threat);
        pixel_font::DrawBitmapText(renderer, "HP " + std::to_string(player.Health()), threatX + (4.0f * scale),
                                   threatY + (3.0f * scale), scale, SDL_Color{247, 222, 214, 255});
        pixel_font::DrawBitmapText(renderer, "Z " + std::to_string(zombieCount), threatX + (28.0f * scale),
                                   threatY + (10.0f * scale), scale, SDL_Color{180, 223, 158, 255});
        pixel_font::DrawBitmapText(renderer, "R " + std::to_string(darkRaiderCount), threatX + (54.0f * scale),
                                   threatY + (10.0f * scale), scale, SDL_Color{232, 192, 143, 255});
        pixel_font::DrawBitmapText(renderer, "M " + std::to_string(monsterCount), threatX + (82.0f * scale),
                                   threatY + (10.0f * scale), scale, SDL_Color{191, 168, 239, 255});
    }

    if (nearbyNpcIndex >= 0) {
        const NpcAI& npc = npcs[static_cast<std::size_t>(nearbyNpcIndex)];
        const float infoW = 112.0f * scale;
        const float infoH = 34.0f * scale;
        const float infoX = destination.x + destination.w - infoW - (6.0f * scale);
        const float infoY = destination.y + (6.0f * scale);

        SDL_SetRenderDrawColor(renderer, 14, 21, 29, 222);
        SDL_FRect info{infoX, infoY, infoW, infoH};
        SDL_RenderFillRect(renderer, &info);

        SDL_SetRenderDrawColor(renderer, 197, 176, 133, 255);
        SDL_FRect top{infoX, infoY, infoW, 1.0f * scale};
        SDL_FRect left{infoX, infoY, 1.0f * scale, infoH};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &left);

        pixel_font::DrawBitmapText(renderer, npc.Name(), infoX + (4.0f * scale), infoY + (4.0f * scale), scale,
                                   SDL_Color{244, 237, 221, 255});
        pixel_font::DrawBitmapText(renderer, npc.Role(), infoX + (4.0f * scale), infoY + (11.0f * scale), scale,
                                   SDL_Color{151, 203, 235, 255});
        pixel_font::DrawBitmapText(renderer, "TRAIT: " + npc.Trait(), infoX + (4.0f * scale), infoY + (19.0f * scale),
                                   scale, SDL_Color{215, 224, 233, 255});
        pixel_font::DrawBitmapText(renderer, "FOCUS: " + npc.Focus(), infoX + (4.0f * scale), infoY + (26.0f * scale),
                                   scale, SDL_Color{189, 212, 164, 255});
    }

    {
        const float radarW = 120.0f * scale;
        const float radarH = (10.0f + (static_cast<float>(npcs.size()) * 8.0f)) * scale;
        const float radarX = destination.x + destination.w - radarW - (6.0f * scale);
        const float radarY = destination.y + destination.h - radarH - (6.0f * scale);

        SDL_SetRenderDrawColor(renderer, 11, 18, 24, 222);
        SDL_FRect radar{radarX, radarY, radarW, radarH};
        SDL_RenderFillRect(renderer, &radar);

        SDL_SetRenderDrawColor(renderer, 158, 182, 204, 255);
        SDL_FRect top{radarX, radarY, radarW, 1.0f * scale};
        SDL_FRect left{radarX, radarY, 1.0f * scale, radarH};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &left);

        pixel_font::DrawBitmapText(renderer, "NPC RADAR", radarX + (4.0f * scale), radarY + (3.0f * scale), scale,
                                   SDL_Color{232, 238, 245, 255});

        for (std::size_t index = 0; index < npcs.size(); ++index) {
            const NpcAI& npc = npcs[index];
            const int dx = static_cast<int>(std::lround(npc.CenterX() - player.CenterX()));
            const int dy = static_cast<int>(std::lround(npc.FeetY() - player.FeetY()));
            const int dist = static_cast<int>(std::lround(std::sqrt(static_cast<float>((dx * dx) + (dy * dy)))));
            const float rowY = radarY + (10.0f * scale) + (static_cast<float>(index) * 8.0f * scale);

            if (static_cast<int>(index) == nearbyNpcIndex) {
                SDL_SetRenderDrawColor(renderer, 75, 105, 132, 200);
                SDL_FRect highlight{radarX + (2.0f * scale), rowY - (1.0f * scale), radarW - (4.0f * scale),
                                    7.0f * scale};
                SDL_RenderFillRect(renderer, &highlight);
            }

            pixel_font::DrawBitmapText(renderer, npc.Name(), radarX + (4.0f * scale), rowY, scale,
                                       SDL_Color{244, 228, 187, 255});
            pixel_font::DrawBitmapText(renderer, "X" + FormatSignedInt(dx) + " Y" + FormatSignedInt(dy),
                                       radarX + (28.0f * scale), rowY, scale, SDL_Color{180, 206, 228, 255});
            pixel_font::DrawBitmapText(renderer, "D" + std::to_string(dist), radarX + radarW - (18.0f * scale), rowY,
                                       scale, SDL_Color{202, 222, 171, 255});
        }
    }

    if (chatMode) {
        const float chatW = destination.w - (24.0f * scale);
        const float chatH = 34.0f * scale;
        const float chatX = destination.x + (12.0f * scale);
        const float chatY = destination.y + destination.h - chatH - (8.0f * scale);

        SDL_SetRenderDrawColor(renderer, 10, 14, 20, 232);
        SDL_FRect chat{chatX, chatY, chatW, chatH};
        SDL_RenderFillRect(renderer, &chat);

        SDL_SetRenderDrawColor(renderer, 90, 145, 194, 255);
        SDL_FRect top{chatX, chatY, chatW, 1.0f * scale};
        SDL_RenderFillRect(renderer, &top);

        if (chatNpcIndex >= 0) {
            const NpcAI& npc = npcs[static_cast<std::size_t>(chatNpcIndex)];
            pixel_font::DrawBitmapText(renderer, npc.Name() + " CHAT", chatX + (4.0f * scale), chatY + (4.0f * scale), scale,
                                       SDL_Color{233, 240, 247, 255});

            const std::vector<std::string> replyLines = pixel_font::WrapText(chatReply, 32);
            if (!replyLines.empty()) {
                pixel_font::DrawBitmapText(renderer, replyLines[0], chatX + (4.0f * scale), chatY + (11.0f * scale), scale,
                                           SDL_Color{174, 227, 196, 255});
            }
        } else {
            pixel_font::DrawBitmapText(renderer, "PLAYER CHAT", chatX + (4.0f * scale), chatY + (4.0f * scale), scale,
                                       SDL_Color{233, 240, 247, 255});
        }

        std::string inputLine = "YOU: " + chatInput;
        if (static_cast<int>(SDL_GetTicks() / 350U) % 2 == 0 && chatInput.size() < 42U) {
            inputLine += "_";
        }

        if (chatNpcIndex < 0) {
            pixel_font::DrawBitmapText(renderer, inputLine, chatX + (4.0f * scale), chatY + (11.0f * scale), scale,
                                       SDL_Color{242, 233, 210, 255});
        } else {
            pixel_font::DrawBitmapText(renderer, inputLine, chatX + (4.0f * scale), chatY + (21.0f * scale), scale,
                                       SDL_Color{242, 233, 210, 255});
        }

        pixel_font::DrawBitmapText(renderer, "ENTER SEND  ESC CLOSE", chatX + chatW - (81.0f * scale), chatY + (4.0f * scale),
                                   scale, SDL_Color{143, 153, 167, 255});
    }
}

}
