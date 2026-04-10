#pragma once

#include <SDL3/SDL.h>

#include "core/Camera2D.h"
#include "game/Pawn.h"

class Player : public Pawn {
public:
    static constexpr float kJumpDuration = 0.42f;
    static constexpr float kAttackDuration = 0.18f;

    enum class ToolVisual {
        Blade,
        Fence,
        Soil,
        Seed,
        Rod,
        Bow
    };

    Player();

    void SetToolVisual(ToolVisual toolVisual);
    void SetSwimming(bool swimming);
    void TriggerJump();
    void TriggerAttack();
    void Update(float dt, bool up, bool down, bool left, bool right, float speedMultiplier, bool preserveFacing);
    void ApplyDamage(int amount);
    void HealFull();
    void DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const;
    void Draw(SDL_Renderer* renderer, const Camera2D& camera) const;

    float FacingX() const;
    float FacingY() const;
    int Health() const;
    int MaxHealth() const;
    bool IsAlive() const;

private:
    void OnBeginPlay() override;
    void OnEndPlay() override;

    enum class FacingDirection {
        Down,
        Up,
        Side
    };

    float animTime_;
    float idleTime_;
    FacingDirection facingDirection_;
    ToolVisual toolVisual_;

    float jumpTimer_;
    bool jumping_;
    bool swimming_;
    float attackTimer_;
    bool attacking_;
    int health_;
    int maxHealth_;
};
