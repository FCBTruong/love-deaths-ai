#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <random>

#include "core/Camera2D.h"
#include "game/Pawn.h"

class TileMap;

enum class AnimalKind : std::uint8_t {
    Rabbit,
    Fox,
    Boar,
    Deer,
    Wolf,
    Chicken,
    Fish
};

class Animal : public Pawn {
public:
    Animal(float x, float y, AnimalKind kind, std::uint32_t seed);

    void Update(float dt, const TileMap& map);
    void DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const;
    void Draw(SDL_Renderer* renderer, const Camera2D& camera) const;

    AnimalKind Kind() const;
private:
    void OnBeginPlay() override;
    void OnEndPlay() override;

    void PickNewBehavior();

    float dirX_;
    float dirY_;
    float behaviorTimer_;
    float animTime_;
    bool leaping_;
    float leapTimer_;
    float leapDuration_;
    float leapHeight_;
    float leapCooldown_;

    AnimalKind kind_;
    std::mt19937 rng_;
};
