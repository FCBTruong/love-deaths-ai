#pragma once

#include <SDL3/SDL.h>

#include "core/Camera2D.h"

class Player {
public:
    static constexpr float kJumpDuration = 0.42f;
    static constexpr float kAttackDuration = 0.18f;

    enum class ToolVisual {
        Blade,
        Fence,
        Soil,
        Seed,
        Rod
    };

    Player();

    void SetPosition(float x, float y);
    void SetToolVisual(ToolVisual toolVisual);
    void TriggerJump();
    void TriggerAttack();
    void Update(float dt, bool up, bool down, bool left, bool right, float speedMultiplier, bool preserveFacing);
    void ApplyDamage(int amount);
    void HealFull();
    void DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const;
    void Draw(SDL_Renderer* renderer, const Camera2D& camera) const;

    float CenterX() const;
    float CenterY() const;
    float FeetY() const;
    float FacingX() const;
    float FacingY() const;
    float X() const;
    float Y() const;
    int Health() const;
    int MaxHealth() const;
    bool IsAlive() const;

private:
    enum class FacingDirection {
        Down,
        Up,
        Side
    };

    float x_;
    float y_;
    float width_;
    float height_;
    float speed_;

    float animTime_;
    float idleTime_;
    bool moving_;
    float facingX_;
    FacingDirection facingDirection_;
    ToolVisual toolVisual_;

    float jumpTimer_;
    bool jumping_;
    float attackTimer_;
    bool attacking_;
    int health_;
    int maxHealth_;
};
