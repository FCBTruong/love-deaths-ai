#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/Camera2D.h"
#include "game/Animal.h"
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
    bool layerUpPressedLast_;
    bool layerDownPressedLast_;
    std::string statusText_;
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

    std::vector<WaterSplash> splashes_;
    std::vector<HarvestFly> harvestFly_;
    std::vector<ChopDebris> chopDebris_;
    std::vector<Animal> animals_;
    float splashSpawnTimer_;
    float shakeTimer_;
    float shakeDuration_;
    float shakeMagnitude_;
    float shakeOffsetX_;
    float shakeOffsetY_;
};
