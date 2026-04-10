#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include "ai/AiRuntimeManager.h"
#include "ai/NpcChatSystem.h"
#include "core/Camera2D.h"
#include "game/Animal.h"
#include "game/HostileAI.h"
#include "game/NpcAI.h"
#include "game/Pet.h"
#include "game/Player.h"
#include "game/TileMap.h"

class App {
public:
    App();
    ~App();

    bool Initialize();
    int Run();

private:
    void Shutdown();
    bool InitializeAudio();
    void ShutdownAudio();
    void PlayMikoSound(bool happy);
    void ProcessEvents(bool& running);
    void Update(float dt);
    void Render();
    void DrawFogOfWar(const Camera2D& camera, const SDL_FRect& destination);
    void UpdateEffects(float dt, bool moving);
    void DrawWorldEffects(const Camera2D& camera);
    void DrawHarvestFlyEffects(const Camera2D& camera, const SDL_FRect& destination);
    void DrawFences(const Camera2D& camera, bool drawBeforePlayer, float splitWorldY);
    void DrawSoilPlots(const Camera2D& camera, bool drawBeforePlayer, float splitWorldY);
    void DrawPlacementPreview(const Camera2D& camera);

    static constexpr int kWindowWidth = 1280;
    static constexpr int kWindowHeight = 720;
    static constexpr int kVirtualWidth = 320;
    static constexpr int kVirtualHeight = 180;

    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* frameTexture_;
    SDL_Texture* fogTexture_;
    SDL_AudioStream* audioStream_;

    TileMap map_;
    Player player_;
    Camera2D camera_;

    float cameraOffsetX_;
    float cameraOffsetY_;
    bool draggingCamera_;
    bool recenteringCamera_;
    int lastMouseX_;
    int lastMouseY_;

    bool interactPressedLast_;
    bool jumpPressedLast_;
    bool attackPressedLast_;
    bool talkPressedLast_;
    bool slot1PressedLast_;
    bool slot2PressedLast_;
    bool slot3PressedLast_;
    bool slot4PressedLast_;
    bool slot5PressedLast_;
    bool slot6PressedLast_;
    bool enterPressedLast_;
    bool suppressEnterOpen_;
    bool undoPressedLast_;
    bool fenceVerticalPlacement_;
    bool layerUpPressedLast_;
    bool layerDownPressedLast_;
    bool chatMode_;
    std::string statusText_;
    std::string chatInput_;
    std::string chatReply_;
    std::string playerChatText_;
    float playerChatTimer_;
    float statusTimer_;
    int currentLayer_;

    int appleCount_;
    int berryCount_;
    int pearCount_;
    int meatCount_;

    struct WaterSplash {
        float worldX;
        float worldY;
        float velX;
        float velY;
        float life;
        float maxLife;
        float size;
    };

    struct HarvestFly {
        HarvestResult type;
        float worldX;
        float worldY;
        float elapsed;
        float duration;
    };

    struct ChopDebris {
        float worldX;
        float worldY;
        float velX;
        float velY;
        float life;
        float maxLife;
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
        float size;
    };

    struct BloodParticle {
        float worldX;
        float worldY;
        float velX;
        float velY;
        float life;
        float maxLife;
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
        float size;
    };

    struct ArrowShot {
        float worldX;
        float worldY;
        float velX;
        float velY;
        float life;
        float maxLife;
    };

    struct Fence {
        float x;
        float y;
        float width;
        float height;
        int health;
    };

    struct SoilPlot {
        float x;
        float y;
        float width;
        float height;
        bool planted;
        HarvestResult seedType;
        float growTimer;
        float growDuration;
    };

    enum class HeldItem {
        Weapon,
        Fence,
        Soil,
        Seed,
        Rod,
        Bow
    };

    struct FishingCast {
        bool active;
        float worldX;
        float worldY;
        float timer;
        float biteTime;
        float totalTime;
    };

    std::vector<WaterSplash> splashes_;
    std::vector<HarvestFly> harvestFly_;
    std::vector<ChopDebris> chopDebris_;
    std::vector<BloodParticle> bloodParticles_;
    std::vector<ArrowShot> arrows_;
    std::vector<Fence> fences_;
    std::vector<SoilPlot> soilPlots_;
    std::vector<Animal> animals_;
    Pet miko_;
    std::vector<NpcAI> npcs_;
    std::vector<HostileAI> hostiles_;
    HeldItem heldItem_;
    FishingCast fishingCast_;
    std::future<npc_chat_system::AiFollowupResult> pendingAiFollowup_;
    std::optional<npc_chat_system::AiFollowupRequest> pendingAiRequest_;
    int nearbyNpcIndex_;
    int chatNpcIndex_;
    float splashSpawnTimer_;
    float hostileSpawnTimer_;
    float fogAnimTime_;
    float shakeTimer_;
    float shakeDuration_;
    float shakeMagnitude_;
    float shakeOffsetX_;
    float shakeOffsetY_;
    float mikoSoundCooldown_;
    AiRuntimeManager aiRuntime_;
    bool aiBackendReady_;
    std::string aiBackendStatus_;
};
