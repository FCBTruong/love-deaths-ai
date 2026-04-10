#pragma once

#include "game/Actor.h"

class Pawn : public Actor {
public:
    Pawn(float x, float y, float width, float height, float speed, float facingX)
        : Actor(x, y, width, height), speed_(speed), facingX_(facingX), moving_(false) {}

    virtual ~Pawn() = default;

    void Tick(float dt) override {
        Actor::Tick(dt);
    }

    float Speed() const { return speed_; }
    float FacingX() const { return facingX_; }
    bool IsMoving() const { return moving_; }

protected:
    float speed_;
    float facingX_;
    bool moving_;
};
