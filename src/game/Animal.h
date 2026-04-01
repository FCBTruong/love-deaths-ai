#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <random>

#include "core/Camera2D.h"

class TileMap;

enum class AnimalKind : std::uint8_t {
    Rabbit,
    Fox,
    Boar,
    Fish
};

class Animal {
public:
    Animal(float x, float y, AnimalKind kind, std::uint32_t seed);

    void Update(float dt, const TileMap& map);
    void DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const;
    void Draw(SDL_Renderer* renderer, const Camera2D& camera) const;

    AnimalKind Kind() const;
    float X() const;
    float Y() const;
    float CenterX() const;
    float CenterY() const;
    float FeetY() const;

private:
    void PickNewBehavior();

    float x_;
    float y_;
    float width_;
    float height_;
    float speed_;

    float dirX_;
    float dirY_;
    float facingX_;

    bool moving_;
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
