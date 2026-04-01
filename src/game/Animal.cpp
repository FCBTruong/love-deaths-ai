#include "game/Animal.h"

#include <algorithm>
#include <cmath>

#include "game/TileMap.h"

Animal::Animal(float x, float y, AnimalKind kind, std::uint32_t seed)
    : x_(x),
      y_(y),
      width_(11.0f),
      height_(8.0f),
      speed_(30.0f),
      dirX_(1.0f),
      dirY_(0.0f),
      facingX_(1.0f),
      moving_(false),
      behaviorTimer_(0.0f),
      animTime_(0.0f),
    leaping_(false),
    leapTimer_(0.0f),
    leapDuration_(0.0f),
    leapHeight_(0.0f),
    leapCooldown_(0.0f),
      kind_(kind),
      rng_(seed) {
    if (kind_ == AnimalKind::Rabbit) {
        width_ = 9.0f;
        height_ = 7.0f;
        speed_ = 38.0f;
    } else if (kind_ == AnimalKind::Fox) {
        width_ = 12.0f;
        height_ = 8.0f;
        speed_ = 33.0f;
    } else if (kind_ == AnimalKind::Boar) {
        width_ = 13.0f;
        height_ = 9.0f;
        speed_ = 26.0f;
    } else {
        width_ = 8.0f;
        height_ = 5.0f;
        speed_ = 25.0f;
    }

    PickNewBehavior();
}

void Animal::PickNewBehavior() {
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    std::uniform_real_distribution<float> moveTime(0.8f, 2.4f);
    std::uniform_real_distribution<float> idleTime(0.35f, 1.05f);
    std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);

    moving_ = chance(rng_) > 0.30f;
    behaviorTimer_ = moving_ ? moveTime(rng_) : idleTime(rng_);

    if (moving_) {
        const float a = angle(rng_);
        dirX_ = std::cos(a);
        dirY_ = std::sin(a);
        if (std::fabs(dirX_) > 0.1f) {
            facingX_ = dirX_;
        }
    }
}

void Animal::Update(float dt, const TileMap& map) {
    const bool fish = kind_ == AnimalKind::Fish;

    behaviorTimer_ -= dt;
    if (behaviorTimer_ <= 0.0f) {
        PickNewBehavior();
    }

    if (fish) {
        leapCooldown_ = std::max(0.0f, leapCooldown_ - dt);
        if (leaping_) {
            leapTimer_ += dt;
            if (leapTimer_ >= leapDuration_) {
                leaping_ = false;
                leapTimer_ = 0.0f;
                leapDuration_ = 0.0f;
                leapHeight_ = 0.0f;
                std::uniform_real_distribution<float> nextLeap(1.4f, 4.6f);
                leapCooldown_ = nextLeap(rng_);
            }
        } else if (moving_ && leapCooldown_ <= 0.0f) {
            std::uniform_real_distribution<float> chance(0.0f, 1.0f);
            if (chance(rng_) > 0.985f) {
                std::uniform_real_distribution<float> leapDuration(0.45f, 0.85f);
                std::uniform_real_distribution<float> leapHeight(7.0f, 13.0f);
                leaping_ = true;
                leapTimer_ = 0.0f;
                leapDuration_ = leapDuration(rng_);
                leapHeight_ = leapHeight(rng_);
            }
        }
    }

    if (!moving_) {
        animTime_ = 0.0f;
        return;
    }

    const float prevX = x_;
    const float prevY = y_;

    const float moveMultiplier = fish ? 1.0f : map.MovementMultiplierAt(x_ + (width_ * 0.5f), y_ + height_, 0);
    const float moveSpeed = speed_ * std::max(moveMultiplier, fish ? 0.95f : 0.55f);
    x_ += dirX_ * moveSpeed * dt;
    y_ += dirY_ * moveSpeed * dt;

    const float centerX = x_ + (width_ * 0.5f);
    const float feetY = y_ + height_;
    const bool hitInvalid = fish ? !map.IsWaterAt(centerX, feetY) : (map.IsBlockedAt(centerX, feetY, 0) || map.IsWaterAt(centerX, feetY));
    if (hitInvalid) {
        x_ = prevX;
        y_ = prevY;
        behaviorTimer_ = 0.0f;
        moving_ = false;
        return;
    }

    animTime_ += dt * 12.0f;
}

void Animal::DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const {
    const float screenX = std::floor(x_ - camera.x);
    const float screenY = std::floor(y_ - camera.y);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (kind_ == AnimalKind::Fish) {
        float leapArc = 0.0f;
        if (leaping_ && leapDuration_ > 0.0f) {
            const float t = std::clamp(leapTimer_ / leapDuration_, 0.0f, 1.0f);
            leapArc = std::sin(t * 3.14159265f);
        }

        const float rippleGrow = leapArc * 5.0f;
        SDL_SetRenderDrawColor(renderer, 12, 30, 52, static_cast<std::uint8_t>(72.0f + (leapArc * 38.0f)));
        SDL_FRect ripple{screenX - (rippleGrow * 0.5f), screenY + height_ - 0.4f, width_ + 1.0f + rippleGrow, 1.4f + leapArc};
        SDL_RenderFillRect(renderer, &ripple);

        SDL_SetRenderDrawColor(renderer, 154, 221, 255, static_cast<std::uint8_t>(45.0f + (leapArc * 65.0f)));
        SDL_FRect sheen{screenX + 1.0f - (rippleGrow * 0.25f), screenY + height_ - 0.9f, width_ - 1.0f + (rippleGrow * 0.5f), 0.8f};
        SDL_RenderFillRect(renderer, &sheen);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 65);
    SDL_FRect shadow{screenX + 1.0f, screenY + height_ - 1.2f, width_ - 1.0f, 2.2f};
    SDL_RenderFillRect(renderer, &shadow);
}

void Animal::Draw(SDL_Renderer* renderer, const Camera2D& camera) const {
    float jumpOffset = 0.0f;
    if (kind_ == AnimalKind::Fish && leaping_ && leapDuration_ > 0.0f) {
        const float t = std::clamp(leapTimer_ / leapDuration_, 0.0f, 1.0f);
        jumpOffset = std::floor(std::sin(t * 3.14159265f) * leapHeight_);
    }

    const float screenX = std::floor(x_ - camera.x);
    const float screenY = std::floor(y_ - camera.y - jumpOffset);
    const float legShift = moving_ ? ((static_cast<int>(animTime_) % 2 == 0) ? 1.0f : -1.0f) : 0.0f;

    if (kind_ == AnimalKind::Rabbit) {
        SDL_SetRenderDrawColor(renderer, 232, 232, 236, 255);
        SDL_FRect body{screenX + 1.0f, screenY + 2.0f, width_ - 2.0f, height_ - 2.0f};
        SDL_RenderFillRect(renderer, &body);

        SDL_SetRenderDrawColor(renderer, 250, 198, 210, 255);
        SDL_FRect earA{screenX + (facingX_ >= 0.0f ? 5.0f : 2.0f), screenY - 1.0f, 1.0f, 3.0f};
        SDL_FRect earB{earA.x + 2.0f, screenY - 1.0f, 1.0f, 3.0f};
        SDL_RenderFillRect(renderer, &earA);
        SDL_RenderFillRect(renderer, &earB);
    } else if (kind_ == AnimalKind::Fox) {
        SDL_SetRenderDrawColor(renderer, 227, 131, 66, 255);
        SDL_FRect body{screenX + 1.0f, screenY + 2.0f, width_ - 3.0f, height_ - 2.0f};
        SDL_RenderFillRect(renderer, &body);

        SDL_SetRenderDrawColor(renderer, 255, 236, 220, 255);
        SDL_FRect tail{screenX + (facingX_ >= 0.0f ? -2.0f : width_ - 1.0f), screenY + 3.0f, 3.0f, 2.0f};
        SDL_RenderFillRect(renderer, &tail);
    } else if (kind_ == AnimalKind::Boar) {
        SDL_SetRenderDrawColor(renderer, 149, 104, 83, 255);
        SDL_FRect body{screenX, screenY + 2.0f, width_, height_ - 1.0f};
        SDL_RenderFillRect(renderer, &body);

        SDL_SetRenderDrawColor(renderer, 121, 82, 64, 255);
        SDL_FRect nose{screenX + (facingX_ >= 0.0f ? width_ - 1.0f : -1.0f), screenY + 4.0f, 2.0f, 2.0f};
        SDL_RenderFillRect(renderer, &nose);
    } else {
        const float swimWiggle = moving_ ? ((static_cast<int>(animTime_ * 2.0f) % 2 == 0) ? 1.0f : -1.0f) : 0.0f;
        const float leapStretch = jumpOffset > 0.0f ? 1.0f : 0.0f;

        SDL_SetRenderDrawColor(renderer, 68, 162, 214, 255);
        SDL_FRect body{screenX + 1.0f, screenY + 1.0f - leapStretch, width_ - 2.0f, height_ - 1.0f + leapStretch};
        SDL_RenderFillRect(renderer, &body);

        SDL_SetRenderDrawColor(renderer, 102, 198, 240, 255);
        SDL_FRect finTop{screenX + 3.0f, screenY - leapStretch, 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &finTop);

        SDL_SetRenderDrawColor(renderer, 52, 140, 191, 255);
        const float tailX = facingX_ >= 0.0f ? screenX - 1.0f : screenX + width_ - 1.0f;
        SDL_FRect tailA{tailX, screenY + 2.0f + swimWiggle, 2.0f, 1.0f};
        SDL_FRect tailB{tailX, screenY + 3.0f - swimWiggle, 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &tailA);
        SDL_RenderFillRect(renderer, &tailB);

        const float eyeXFish = facingX_ >= 0.0f ? (screenX + width_ - 3.0f) : (screenX + 2.0f);
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_FRect eyeFish{eyeXFish, screenY + 2.0f, 1.0f, 1.0f};
        SDL_RenderFillRect(renderer, &eyeFish);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 46, 35, 30, 255);
    SDL_FRect legL{screenX + 2.0f + legShift, screenY + height_ - 1.0f, 2.0f, 1.0f};
    SDL_FRect legR{screenX + width_ - 4.0f - legShift, screenY + height_ - 1.0f, 2.0f, 1.0f};
    SDL_RenderFillRect(renderer, &legL);
    SDL_RenderFillRect(renderer, &legR);

    const float eyeX = facingX_ >= 0.0f ? screenX + width_ - 3.0f : screenX + 2.0f;
    SDL_SetRenderDrawColor(renderer, 28, 28, 28, 255);
    SDL_FRect eye{eyeX, screenY + 3.0f, 1.0f, 1.0f};
    SDL_RenderFillRect(renderer, &eye);
}

AnimalKind Animal::Kind() const {
    return kind_;
}

float Animal::X() const {
    return x_;
}

float Animal::Y() const {
    return y_;
}

float Animal::CenterX() const {
    return x_ + (width_ * 0.5f);
}

float Animal::CenterY() const {
    return y_ + (height_ * 0.5f);
}

float Animal::FeetY() const {
    return y_ + height_;
}
