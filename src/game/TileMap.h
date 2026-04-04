#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "core/Camera2D.h"

enum class InteractionResult : std::uint8_t {
    None,
    ChoppedTree,
    BrokeRock,
    EnteredHouse
};

enum class HarvestResult : std::uint8_t {
    None,
    Apple,
    Berry,
    Pear,
    Meat
};

class TileMap {
public:
    explicit TileMap(int tileSize);

    void DrawGround(SDL_Renderer* renderer, const Camera2D& camera, int currentLayer) const;
    void DrawShadows(SDL_Renderer* renderer, const Camera2D& camera, int currentLayer, const std::vector<SDL_FRect>* clearedGround = nullptr) const;
    void DrawProps(
        SDL_Renderer* renderer,
        const Camera2D& camera,
        float splitWorldY,
        bool drawBeforePlayer,
        int currentLayer,
        const std::vector<SDL_FRect>* clearedGround = nullptr
    ) const;

    float MovementMultiplierAt(float worldX, float worldY, int currentLayer) const;
    bool IsWaterAt(float worldX, float worldY) const;
    bool IsBlockedAt(float worldX, float worldY, int currentLayer) const;
    bool HasBlockingObstacleInRect(float worldX, float worldY, float width, float height, int currentLayer) const;
    bool CanMoveLayer(float worldX, float worldY, int currentLayer, bool moveUp, int& outLayer) const;
    InteractionResult InteractAt(float worldX, float worldY, float facingX, float* outHitX = nullptr, float* outHitY = nullptr);
    HarvestResult AttackAt(
        float worldX,
        float worldY,
        int currentLayer,
        float facingX,
        float* outHitX = nullptr,
        float* outHitY = nullptr
    );

private:
    static long long PropKey(int tx, int ty);
    static int WorldToTile(float worldPos, int tileSize);

    int tileSize_;
    std::unordered_set<long long> removedProps_;
};
