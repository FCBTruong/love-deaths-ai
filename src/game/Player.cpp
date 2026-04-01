#include "game/Player.h"

#include <cmath>

Player::Player()
        : x_(0.0f),
            y_(0.0f),
            width_(12.0f),
            height_(14.0f),
            speed_(96.0f),
            animTime_(0.0f),
            idleTime_(0.0f),
            moving_(false),
            facingX_(1.0f),
            jumpTimer_(0.0f),
            jumping_(false),
            attackTimer_(0.0f),
            attacking_(false) {}

void Player::SetPosition(float x, float y) {
    x_ = x;
    y_ = y;
}

void Player::TriggerJump() {
    if (jumping_) {
        return;
    }
    jumping_ = true;
    jumpTimer_ = 0.0f;
}

void Player::TriggerAttack() {
    attacking_ = true;
    attackTimer_ = 0.0f;
}

void Player::Update(float dt, bool up, bool down, bool left, bool right, float speedMultiplier) {
    float moveX = 0.0f;
    float moveY = 0.0f;

    if (left) {
        moveX -= 1.0f;
    }
    if (right) {
        moveX += 1.0f;
    }
    if (up) {
        moveY -= 1.0f;
    }
    if (down) {
        moveY += 1.0f;
    }

    const float magnitude = std::sqrt((moveX * moveX) + (moveY * moveY));
    if (magnitude > 0.0f) {
        moveX /= magnitude;
        moveY /= magnitude;
        moving_ = true;
        animTime_ += dt * 10.0f;
        idleTime_ = 0.0f;

        if (std::fabs(moveX) > 0.01f) {
            facingX_ = moveX;
        }
    } else {
        moving_ = false;
        animTime_ = 0.0f;
        idleTime_ += dt;
    }

    const float finalSpeed = speed_ * speedMultiplier;
    x_ += moveX * finalSpeed * dt;
    y_ += moveY * finalSpeed * dt;

    if (jumping_) {
        jumpTimer_ += dt;
        if (jumpTimer_ >= kJumpDuration) {
            jumping_ = false;
            jumpTimer_ = 0.0f;
        }
    }

    if (attacking_) {
        attackTimer_ += dt;
        if (attackTimer_ >= kAttackDuration) {
            attacking_ = false;
            attackTimer_ = 0.0f;
        }
    }
}

void Player::DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const {
    const float screenX = std::floor(x_ - camera.x);
    const float screenY = std::floor(y_ - camera.y);

    float jumpFactor = 0.0f;
    if (jumping_) {
        const float t = jumpTimer_ / kJumpDuration;
        jumpFactor = std::sin(t * 3.14159265f);
    }
    const float scale = 1.0f - (jumpFactor * 0.45f);
    const float offsetX = (10.0f - (10.0f * scale)) * 0.5f;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<std::uint8_t>(78.0f * scale));
    SDL_FRect core{screenX + 1.0f + offsetX, screenY + 12.0f, 10.0f * scale, 3.0f * scale};
    SDL_RenderFillRect(renderer, &core);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<std::uint8_t>(46.0f * scale));
    SDL_FRect projected{screenX + 6.0f, screenY + 10.5f, 8.0f * scale, 2.0f * scale};
    SDL_RenderFillRect(renderer, &projected);
}

void Player::Draw(SDL_Renderer* renderer, const Camera2D& camera) const {
    const float screenX = std::floor(x_ - camera.x);
    const float screenY = std::floor(y_ - camera.y);

    float jumpFactor = 0.0f;
    if (jumping_) {
        const float t = jumpTimer_ / kJumpDuration;
        jumpFactor = std::sin(t * 3.14159265f);
    }
    const float jumpOffsetY = std::floor(jumpFactor * 7.0f);

    const int frame = static_cast<int>(animTime_) % 4;

    // Idle animation
    const float breathCycle = std::sin(idleTime_ * 1.4f);
    const float bobY = moving_ ? std::floor(std::sin(animTime_ * 2.2f) * 1.0f)
                                : std::floor(breathCycle * 0.5f);
    const float idleHandBob = moving_ ? 0.0f : std::floor(std::sin(idleTime_ * 1.4f + 0.4f) * 0.5f);
    // Blink: closed for ~0.12 s every ~3 s period
    const float blinkPhase = std::fmod(idleTime_, 3.1f);
    const bool blinking = !moving_ && (blinkPhase < 0.12f);

    const float legShift = (frame % 2 == 0) ? 1.0f : -1.0f;
    const float renderY = screenY + bobY - jumpOffsetY;

    SDL_SetRenderDrawColor(renderer, 22, 35, 106, 255);
    SDL_FRect legL{screenX + 2.0f + legShift, renderY + 11.0f, 3.0f, 3.0f};
    SDL_FRect legR{screenX + 7.0f - legShift, renderY + 11.0f, 3.0f, 3.0f};
    SDL_RenderFillRect(renderer, &legL);
    SDL_RenderFillRect(renderer, &legR);

    SDL_FRect body{screenX + 1.0f, renderY + 5.0f, 10.0f, 8.0f};
    SDL_SetRenderDrawColor(renderer, 28, 47, 138, 255);
    SDL_RenderFillRect(renderer, &body);

    SDL_FRect head{screenX + 2.0f, renderY, 8.0f, 6.0f};
    SDL_SetRenderDrawColor(renderer, 233, 199, 158, 255);
    SDL_RenderFillRect(renderer, &head);

    const float eyeX = (facingX_ >= 0.0f) ? (screenX + 7.0f) : (screenX + 4.0f);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    if (blinking) {
        // Draw closed eye as a thin 1px line
        SDL_FRect eyeClosed{eyeX, renderY + 3.0f, 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &eyeClosed);
    } else {
        SDL_FRect eye{eyeX, renderY + 2.0f, 1.0f, 1.0f};
        SDL_RenderFillRect(renderer, &eye);
    }

    const float handX = (facingX_ >= 0.0f) ? (screenX + 10.0f) : (screenX + 1.0f);
    const float handY = renderY + 8.0f + idleHandBob;
    SDL_SetRenderDrawColor(renderer, 98, 63, 45, 255);
    SDL_FRect handle{(facingX_ >= 0.0f) ? handX : (handX - 2.0f), handY, 2.0f, 1.0f};
    SDL_RenderFillRect(renderer, &handle);

    SDL_SetRenderDrawColor(renderer, 195, 205, 218, 255);
    SDL_FRect bladeIdle{(facingX_ >= 0.0f) ? (handX + 2.0f) : (handX - 4.0f), handY, 2.0f, 1.0f};
    SDL_RenderFillRect(renderer, &bladeIdle);

    if (attacking_) {
        const float t = attackTimer_ / kAttackDuration;
        const float swing = std::sin(t * 3.14159265f);
        const float reach = 5.0f + std::floor(swing * 5.0f);
        const float slashX = (facingX_ >= 0.0f) ? (handX + 2.0f) : (handX - reach - 2.0f);
        const float slashY = renderY + 6.0f - std::floor(swing * 1.5f);

        SDL_SetRenderDrawColor(renderer, 220, 226, 235, 255);
        SDL_FRect bladeSwing{slashX, slashY, reach, 2.0f};
        SDL_RenderFillRect(renderer, &bladeSwing);

        SDL_SetRenderDrawColor(renderer, 255, 242, 180, 155);
        SDL_FRect arc{(facingX_ >= 0.0f) ? (slashX + 1.0f) : (slashX - 1.0f), slashY - 1.0f, reach + 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &arc);
    }
}

float Player::CenterX() const {
    return x_ + (width_ * 0.5f);
}

float Player::CenterY() const {
    return y_ + (height_ * 0.5f);
}

float Player::FeetY() const {
    return y_ + height_;
}

float Player::FacingX() const {
    return facingX_ >= 0.0f ? 1.0f : -1.0f;
}

float Player::X() const {
    return x_;
}

float Player::Y() const {
    return y_;
}
