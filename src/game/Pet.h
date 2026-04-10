#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "core/Camera2D.h"
#include "game/Pawn.h"

class TileMap;

class Pet : public Pawn {
public:
    Pet(std::string name, float x, float y);

    void Update(float dt, const TileMap& map, float targetX, float targetY, bool targetMoving);
    void DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const;
    void Draw(SDL_Renderer* renderer, const Camera2D& camera) const;

    std::string ApplyPlayerCommand(const std::string& message, float playerX, float playerY);
    std::string ApplyAiDirective(const std::string& intent, const std::string& speech, float playerX, float playerY);
    bool MatchesName(const std::string& messageUpper) const;
    const std::string& Name() const;

private:
    enum class FacingDirection {
        Down,
        Up,
        Side
    };

    enum class CommandMode {
        FollowPlayer,
        HoldPosition,
        Sit,
        MoveToPoint,
        ReturnHome
    };

    void OnBeginPlay() override;
    void OnEndPlay() override;
    bool TryMove(const TileMap& map, float dt, float moveX, float moveY, float minTerrainMultiplier);

    std::string name_;
    FacingDirection facingDirection_;
    CommandMode commandMode_;
    float homeX_;
    float homeY_;
    float holdX_;
    float holdY_;
    float animTime_;
    float idleTime_;
    float bobPhase_;
    float avoidanceCooldown_;
    float preferLeftTimer_;
    float lastTargetX_;
    float lastTargetY_;
};
