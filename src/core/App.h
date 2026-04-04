#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/Camera2D.h"
#include "game/Animal.h"
#include "game/HostileAI.h"
#include "game/NpcAI.h"
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
    void ProcessEvents(bool& running);
    void Update(float dt);
    void Render();
    void DrawFogOfWar(const Camera2D& camera, const SDL_FRect& destination);
    void DrawUI(const SDL_FRect& destination);
    void UpdateEffects(float dt, bool moving);
    void DrawWorldEffects(const Camera2D& camera);
    void DrawHarvestFlyEffects(const Camera2D& camera, const SDL_FRect& destination);
    void DrawNpcBubbles(const Camera2D& camera, const SDL_FRect& destination);
    void DrawNpcLocators(const Camera2D& camera, const SDL_FRect& destination);
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

    TileMap map_;
    Player player_;
    Camera2D camera_;

    float cameraOffsetX_;
    float cameraOffsetY_;
    bool draggingCamera_;
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
    bool undoPressedLast_;
    bool fenceVerticalPlacement_;
    bool layerUpPressedLast_;
    bool layerDownPressedLast_;
    bool chatMode_;
    std::string statusText_;
    std::string chatInput_;
    std::string chatReply_;
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
        Seed
    };

    std::vector<WaterSplash> splashes_;
    std::vector<HarvestFly> harvestFly_;
    std::vector<ChopDebris> chopDebris_;
    std::vector<BloodParticle> bloodParticles_;
    std::vector<Fence> fences_;
    std::vector<SoilPlot> soilPlots_;
    std::vector<Animal> animals_;
    std::vector<NpcAI> npcs_;
    std::vector<HostileAI> hostiles_;
    HeldItem heldItem_;
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
};
