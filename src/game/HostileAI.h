#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

#include "core/Camera2D.h"
#include "game/Pawn.h"

class TileMap;

enum class HostileKind : std::uint8_t {
    Zombie,
    Marauder,
    Ghoul,
    Wraith
};

class HostileAI : public Pawn {
public:
    HostileAI(float x, float y, HostileKind kind, std::uint32_t seed);

    void Update(float dt, const TileMap& map, float targetX, float targetY, bool hasTarget);
    void DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const;
    void Draw(SDL_Renderer* renderer, const Camera2D& camera) const;

    bool IsReadyToAttack(float targetX, float targetY) const;
    bool IsEmerging() const;
    void ResetAttackCooldown();
    void ApplyDamage(int amount);
    bool IsAlive() const;
    HostileKind Kind() const;
    const std::string& Label() const;
private:
    void OnBeginPlay() override;
    void OnEndPlay() override;

    float animTime_;
    float attackCooldown_;
    float emergeTimer_;
    float emergeDuration_;
    int health_;
    HostileKind kind_;
    std::string label_;
};
