#pragma once

#include <SDL3/SDL.h>

#include "core/Camera2D.h"

class Player {
public:
    static constexpr float kJumpDuration = 0.42f;
    static constexpr float kAttackDuration = 0.18f;

    Player();

    void SetPosition(float x, float y);
    void TriggerJump();
    void TriggerAttack();
    void Update(float dt, bool up, bool down, bool left, bool right, float speedMultiplier);
    void DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const;
    void Draw(SDL_Renderer* renderer, const Camera2D& camera) const;

    float CenterX() const;
    float CenterY() const;
    float FeetY() const;
    float FacingX() const;
    float X() const;
    float Y() const;

private:
    float x_;
    float y_;
    float width_;
    float height_;
    float speed_;

    float animTime_;
    float idleTime_;
    bool moving_;
    float facingX_;

    float jumpTimer_;
    bool jumping_;
    float attackTimer_;
    bool attacking_;
};
