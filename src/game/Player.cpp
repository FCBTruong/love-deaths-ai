#include "game/Player.h"

#include <algorithm>
#include <cmath>

Player::Player()
        : Pawn(0.0f, 0.0f, 12.0f, 14.0f, 96.0f, 1.0f),
          animTime_(0.0f),
          idleTime_(0.0f),
          facingDirection_(FacingDirection::Down),
          toolVisual_(ToolVisual::Blade),
          jumpTimer_(0.0f),
          jumping_(false),
          swimming_(false),
          attackTimer_(0.0f),
          attacking_(false),
          health_(8),
          maxHealth_(8) {}

void Player::OnBeginPlay() {
    animTime_ = 0.0f;
    idleTime_ = 0.0f;
    jumping_ = false;
    jumpTimer_ = 0.0f;
    swimming_ = false;
    attacking_ = false;
    attackTimer_ = 0.0f;
    moving_ = false;
}

void Player::OnEndPlay() {
    moving_ = false;
    jumping_ = false;
    attacking_ = false;
}

void Player::SetToolVisual(ToolVisual toolVisual) {
    toolVisual_ = toolVisual;
}

void Player::SetSwimming(bool swimming) {
    swimming_ = swimming;
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

void Player::ApplyDamage(int amount) {
    health_ = std::max(0, health_ - std::max(amount, 0));
}

void Player::HealFull() {
    health_ = maxHealth_;
}

void Player::Update(float dt, bool up, bool down, bool left, bool right, float speedMultiplier, bool preserveFacing) {
    Tick(dt);

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
        if (preserveFacing) {
            moveX /= magnitude;
            moveY /= magnitude;
            moving_ = true;
            animTime_ += dt * 10.0f;
            idleTime_ = 0.0f;
        } else {
        FacingDirection desiredDirection = FacingDirection::Side;
        float desiredFacingX = facingX_;

        if (std::fabs(moveY) > std::fabs(moveX) + 0.08f) {
            desiredDirection = moveY < 0.0f ? FacingDirection::Up : FacingDirection::Down;
        } else {
            desiredDirection = FacingDirection::Side;
            if (std::fabs(moveX) > 0.01f) {
                desiredFacingX = moveX;
            }
        }

        const bool sideFlip =
            desiredDirection == FacingDirection::Side &&
            facingDirection_ == FacingDirection::Side &&
            ((desiredFacingX >= 0.0f) != (facingX_ >= 0.0f));
        const bool changedDirection =
            desiredDirection != facingDirection_ || sideFlip;

        if (changedDirection) {
            facingDirection_ = desiredDirection;
            facingX_ = desiredFacingX;
            moving_ = false;
            animTime_ = 0.0f;
            idleTime_ += dt;
        } else {
            moveX /= magnitude;
            moveY /= magnitude;
            moving_ = true;
            animTime_ += dt * 10.0f;
            idleTime_ = 0.0f;
        }
        if (!moving_) {
            moveX = 0.0f;
            moveY = 0.0f;
        }
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

    if (swimming_) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 26);
        SDL_FRect rippleShadow{screenX + 1.0f, screenY + 10.0f, 10.0f, 2.0f};
        SDL_RenderFillRect(renderer, &rippleShadow);
        return;
    }

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

    const bool facingUp = facingDirection_ == FacingDirection::Up;
    const bool facingDown = facingDirection_ == FacingDirection::Down;
    const bool facingSide = facingDirection_ == FacingDirection::Side;

    const float legShift = (frame % 2 == 0) ? 1.0f : -1.0f;
    const float renderY = screenY + bobY - jumpOffsetY;
    const float swimLineY = renderY + 8.0f;
    const float paddleShift = moving_ ? ((frame % 2 == 0) ? 1.0f : -1.0f) : 0.0f;
    const auto drawBootCaps = [&](float x0, float y0, float width) {
        SDL_SetRenderDrawColor(renderer, 47, 32, 25, 255);
        SDL_FRect boot{x0, y0, width, 1.0f};
        SDL_RenderFillRect(renderer, &boot);
    };

    const auto drawHeldToolSide = [&](float handX, float handY) {
        if (toolVisual_ == ToolVisual::Blade) {
            SDL_SetRenderDrawColor(renderer, 98, 63, 45, 255);
            SDL_FRect handle{(facingX_ >= 0.0f) ? handX : (handX - 2.0f), handY, 2.0f, 1.0f};
            SDL_RenderFillRect(renderer, &handle);

            SDL_SetRenderDrawColor(renderer, 195, 205, 218, 255);
            SDL_FRect bladeIdle{(facingX_ >= 0.0f) ? (handX + 2.0f) : (handX - 4.0f), handY, 2.0f, 1.0f};
            SDL_RenderFillRect(renderer, &bladeIdle);
        } else if (toolVisual_ == ToolVisual::Fence) {
            SDL_SetRenderDrawColor(renderer, 129, 92, 58, 255);
            SDL_FRect post{(facingX_ >= 0.0f) ? handX : (handX - 2.0f), handY - 1.0f, 2.0f, 5.0f};
            SDL_RenderFillRect(renderer, &post);
            SDL_SetRenderDrawColor(renderer, 171, 129, 82, 255);
            SDL_FRect rail{(facingX_ >= 0.0f) ? (handX + 1.0f) : (handX - 4.0f), handY, 3.0f, 1.0f};
            SDL_RenderFillRect(renderer, &rail);
        } else if (toolVisual_ == ToolVisual::Soil) {
            SDL_SetRenderDrawColor(renderer, 148, 106, 64, 255);
            SDL_FRect bag{(facingX_ >= 0.0f) ? handX : (handX - 3.0f), handY - 1.0f, 3.0f, 3.0f};
            SDL_RenderFillRect(renderer, &bag);
            SDL_SetRenderDrawColor(renderer, 91, 63, 38, 255);
            SDL_FRect dirt{(facingX_ >= 0.0f) ? (handX + 1.0f) : (handX - 1.0f), handY + 2.0f, 2.0f, 1.0f};
            SDL_RenderFillRect(renderer, &dirt);
        } else if (toolVisual_ == ToolVisual::Seed) {
            SDL_SetRenderDrawColor(renderer, 198, 171, 96, 255);
            SDL_FRect pouch{(facingX_ >= 0.0f) ? handX : (handX - 2.0f), handY - 1.0f, 2.0f, 3.0f};
            SDL_RenderFillRect(renderer, &pouch);
            SDL_SetRenderDrawColor(renderer, 124, 92, 43, 255);
            SDL_FRect seed{(facingX_ >= 0.0f) ? (handX + 2.0f) : (handX - 2.0f), handY + 1.0f, 1.0f, 1.0f};
            SDL_RenderFillRect(renderer, &seed);
        } else if (toolVisual_ == ToolVisual::Rod) {
            SDL_SetRenderDrawColor(renderer, 125, 87, 52, 255);
            SDL_FRect rod{(facingX_ >= 0.0f) ? handX : (handX - 6.0f), handY - 2.0f, 6.0f, 1.0f};
            SDL_RenderFillRect(renderer, &rod);
            SDL_SetRenderDrawColor(renderer, 226, 230, 235, 255);
            SDL_FRect line{(facingX_ >= 0.0f) ? (handX + 5.0f) : (handX - 1.0f), handY - 1.0f, 1.0f, 3.0f};
            SDL_RenderFillRect(renderer, &line);
        } else if (toolVisual_ == ToolVisual::Bow) {
            SDL_SetRenderDrawColor(renderer, 121, 80, 47, 255);
            SDL_FRect bowBody{(facingX_ >= 0.0f) ? handX : (handX - 4.0f), handY - 2.0f, 1.0f, 5.0f};
            SDL_FRect bowTip{(facingX_ >= 0.0f) ? (handX + 2.0f) : (handX - 2.0f), handY - 1.0f, 1.0f, 3.0f};
            SDL_RenderFillRect(renderer, &bowBody);
            SDL_RenderFillRect(renderer, &bowTip);
            SDL_SetRenderDrawColor(renderer, 223, 226, 231, 255);
            SDL_FRect string{(facingX_ >= 0.0f) ? (handX + 1.0f) : (handX - 1.0f), handY - 2.0f, 1.0f, 5.0f};
            SDL_RenderFillRect(renderer, &string);
        }
    };

    const auto drawHeldToolVertical = [&](float handX, float handY, bool upside) {
        if (toolVisual_ == ToolVisual::Blade) {
            SDL_SetRenderDrawColor(renderer, 98, 63, 45, 255);
            SDL_FRect handle{handX, handY, 1.0f, 3.0f};
            SDL_RenderFillRect(renderer, &handle);
            SDL_SetRenderDrawColor(renderer, 195, 205, 218, 255);
            SDL_FRect bladeIdle{handX, upside ? (handY - 3.0f) : (handY + 3.0f), 1.0f, 3.0f};
            SDL_RenderFillRect(renderer, &bladeIdle);
        } else if (toolVisual_ == ToolVisual::Fence) {
            SDL_SetRenderDrawColor(renderer, 129, 92, 58, 255);
            SDL_FRect post{handX, handY - 1.0f, 2.0f, 5.0f};
            SDL_RenderFillRect(renderer, &post);
            SDL_SetRenderDrawColor(renderer, 171, 129, 82, 255);
            SDL_FRect rail{handX - 1.0f, handY + 1.0f, 4.0f, 1.0f};
            SDL_RenderFillRect(renderer, &rail);
        } else if (toolVisual_ == ToolVisual::Soil) {
            SDL_SetRenderDrawColor(renderer, 148, 106, 64, 255);
            SDL_FRect bag{handX - 1.0f, handY - 1.0f, 3.0f, 3.0f};
            SDL_RenderFillRect(renderer, &bag);
            SDL_SetRenderDrawColor(renderer, 91, 63, 38, 255);
            SDL_FRect dirt{handX, handY + 2.0f, 1.0f, 1.0f};
            SDL_RenderFillRect(renderer, &dirt);
        } else if (toolVisual_ == ToolVisual::Seed) {
            SDL_SetRenderDrawColor(renderer, 198, 171, 96, 255);
            SDL_FRect pouch{handX, handY - 1.0f, 2.0f, 3.0f};
            SDL_RenderFillRect(renderer, &pouch);
            SDL_SetRenderDrawColor(renderer, 124, 92, 43, 255);
            SDL_FRect seed{handX + 1.0f, handY + 2.0f, 1.0f, 1.0f};
            SDL_RenderFillRect(renderer, &seed);
        } else if (toolVisual_ == ToolVisual::Rod) {
            SDL_SetRenderDrawColor(renderer, 125, 87, 52, 255);
            SDL_FRect rod{handX, upside ? (handY - 5.0f) : handY, 1.0f, 6.0f};
            SDL_RenderFillRect(renderer, &rod);
            SDL_SetRenderDrawColor(renderer, 226, 230, 235, 255);
            SDL_FRect line{handX + 1.0f, upside ? (handY - 6.0f) : (handY + 6.0f), 1.0f, 2.0f};
            SDL_RenderFillRect(renderer, &line);
        } else if (toolVisual_ == ToolVisual::Bow) {
            SDL_SetRenderDrawColor(renderer, 121, 80, 47, 255);
            SDL_FRect bowStem{handX, upside ? (handY - 5.0f) : handY, 1.0f, 6.0f};
            SDL_FRect bowArc{handX + 1.0f, upside ? (handY - 4.0f) : (handY + 1.0f), 1.0f, 4.0f};
            SDL_RenderFillRect(renderer, &bowStem);
            SDL_RenderFillRect(renderer, &bowArc);
            SDL_SetRenderDrawColor(renderer, 223, 226, 231, 255);
            SDL_FRect string{handX + 1.0f, upside ? (handY - 5.0f) : handY, 1.0f, 6.0f};
            SDL_RenderFillRect(renderer, &string);
        }
    };

    if (swimming_) {
        SDL_SetRenderDrawColor(renderer, 233, 199, 158, 255);
        if (facingUp) {
            SDL_FRect head{screenX + 2.0f, renderY + 1.0f, 8.0f, 5.0f};
            SDL_RenderFillRect(renderer, &head);
            SDL_SetRenderDrawColor(renderer, 20, 14, 11, 255);
            SDL_FRect hair{screenX + 2.0f, renderY + 0.0f, 8.0f, 3.0f};
            SDL_RenderFillRect(renderer, &hair);
        } else {
            SDL_FRect head{screenX + 2.0f, renderY, 8.0f, 6.0f};
            SDL_RenderFillRect(renderer, &head);
            SDL_SetRenderDrawColor(renderer, 20, 14, 11, 255);
            SDL_FRect hair{screenX + 2.0f, renderY, 8.0f, 2.0f};
            SDL_RenderFillRect(renderer, &hair);
            if (facingSide) {
                SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
                SDL_FRect eye{(facingX_ >= 0.0f) ? (screenX + 7.0f) : (screenX + 4.0f), renderY + 2.0f, 1.0f, 1.0f};
                SDL_RenderFillRect(renderer, &eye);
            } else {
                SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
                SDL_FRect eyeL{screenX + 4.0f, renderY + 2.0f, 1.0f, 1.0f};
                SDL_FRect eyeR{screenX + 7.0f, renderY + 2.0f, 1.0f, 1.0f};
                SDL_RenderFillRect(renderer, &eyeL);
                SDL_RenderFillRect(renderer, &eyeR);
            }
        }

        SDL_SetRenderDrawColor(renderer, 28, 47, 138, 255);
        SDL_FRect shoulders{screenX + 2.0f, renderY + 5.0f, 8.0f, 3.0f};
        SDL_RenderFillRect(renderer, &shoulders);
        SDL_SetRenderDrawColor(renderer, 57, 85, 194, 255);
        SDL_FRect clothLight{screenX + 3.0f, renderY + 6.0f, 3.0f, 1.0f};
        SDL_RenderFillRect(renderer, &clothLight);

        SDL_SetRenderDrawColor(renderer, 233, 199, 158, 255);
        if (facingSide) {
            SDL_FRect armFront{screenX + (facingX_ >= 0.0f ? 8.0f : 1.0f), renderY + 7.0f + paddleShift, 2.0f, 1.0f};
            SDL_FRect armBack{screenX + (facingX_ >= 0.0f ? 2.0f : 7.0f), renderY + 8.0f - paddleShift, 2.0f, 1.0f};
            SDL_RenderFillRect(renderer, &armFront);
            SDL_RenderFillRect(renderer, &armBack);
        } else {
            SDL_FRect armL{screenX + 1.0f + paddleShift, renderY + 7.0f, 2.0f, 1.0f};
            SDL_FRect armR{screenX + 9.0f - paddleShift, renderY + 7.0f, 2.0f, 1.0f};
            SDL_RenderFillRect(renderer, &armL);
            SDL_RenderFillRect(renderer, &armR);
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 70, 145, 219, 170);
        SDL_FRect waterBand{screenX - 1.0f, swimLineY, 14.0f, 5.0f};
        SDL_RenderFillRect(renderer, &waterBand);
        SDL_SetRenderDrawColor(renderer, 184, 231, 255, 190);
        SDL_FRect foamA{screenX + 1.0f + paddleShift, swimLineY - 1.0f, 3.0f, 1.0f};
        SDL_FRect foamB{screenX + 7.0f - paddleShift, swimLineY - 1.0f, 3.0f, 1.0f};
        SDL_RenderFillRect(renderer, &foamA);
        SDL_RenderFillRect(renderer, &foamB);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        return;
    }

    if (facingUp) {
        SDL_SetRenderDrawColor(renderer, 22, 35, 106, 255);
        SDL_FRect legL{screenX + 3.0f + (legShift * 0.5f), renderY + 11.0f, 2.0f, 3.0f};
        SDL_FRect legR{screenX + 7.0f - (legShift * 0.5f), renderY + 11.0f, 2.0f, 3.0f};
        SDL_RenderFillRect(renderer, &legL);
        SDL_RenderFillRect(renderer, &legR);
        drawBootCaps(legL.x, renderY + 13.0f, 2.0f);
        drawBootCaps(legR.x, renderY + 13.0f, 2.0f);

        SDL_SetRenderDrawColor(renderer, 233, 199, 158, 255);
        SDL_FRect neck{screenX + 4.0f, renderY + 4.0f, 4.0f, 2.0f};
        SDL_RenderFillRect(renderer, &neck);

        SDL_SetRenderDrawColor(renderer, 20, 14, 11, 255);
        SDL_FRect hair{screenX + 2.0f, renderY, 8.0f, 4.0f};
        SDL_RenderFillRect(renderer, &hair);
        SDL_SetRenderDrawColor(renderer, 68, 49, 35, 255);
        SDL_FRect hairLight{screenX + 3.0f, renderY + 1.0f, 3.0f, 1.0f};
        SDL_RenderFillRect(renderer, &hairLight);

        SDL_SetRenderDrawColor(renderer, 233, 199, 158, 255);
        SDL_FRect headBack{screenX + 2.0f, renderY + 2.0f, 8.0f, 4.0f};
        SDL_RenderFillRect(renderer, &headBack);
        SDL_SetRenderDrawColor(renderer, 248, 220, 183, 255);
        SDL_FRect neckLight{screenX + 5.0f, renderY + 4.0f, 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &neckLight);

        SDL_SetRenderDrawColor(renderer, 28, 47, 138, 255);
        SDL_FRect body{screenX + 2.0f, renderY + 5.0f, 8.0f, 7.0f};
        SDL_RenderFillRect(renderer, &body);

        SDL_SetRenderDrawColor(renderer, 18, 31, 92, 255);
        SDL_FRect shoulders{screenX + 1.0f, renderY + 5.0f, 10.0f, 2.0f};
        SDL_RenderFillRect(renderer, &shoulders);
        SDL_SetRenderDrawColor(renderer, 56, 82, 188, 255);
        SDL_FRect coatLight{screenX + 3.0f, renderY + 6.0f, 2.0f, 5.0f};
        SDL_FRect coatSeam{screenX + 6.0f, renderY + 6.0f, 1.0f, 6.0f};
        SDL_RenderFillRect(renderer, &coatLight);
        SDL_RenderFillRect(renderer, &coatSeam);
        SDL_SetRenderDrawColor(renderer, 119, 84, 41, 255);
        SDL_FRect belt{screenX + 2.0f, renderY + 9.0f, 8.0f, 1.0f};
        SDL_RenderFillRect(renderer, &belt);

        const float handY = renderY + 8.0f + idleHandBob;
        drawHeldToolVertical(screenX + 8.0f, handY, true);

        if (attacking_ && toolVisual_ == ToolVisual::Blade) {
            const float t = attackTimer_ / kAttackDuration;
            const float swing = std::sin(t * 3.14159265f);
            const float reach = 4.0f + std::floor(swing * 4.0f);
            const float slashY = handY - reach;
            SDL_SetRenderDrawColor(renderer, 220, 226, 235, 255);
            SDL_FRect bladeSwing{screenX + 8.0f, slashY, 1.0f, reach + 1.0f};
            SDL_RenderFillRect(renderer, &bladeSwing);
            SDL_SetRenderDrawColor(renderer, 255, 242, 180, 155);
            SDL_FRect arc{screenX + 7.0f, slashY, 3.0f, 1.0f};
            SDL_RenderFillRect(renderer, &arc);
        }
    } else if (facingDown) {
        SDL_SetRenderDrawColor(renderer, 22, 35, 106, 255);
        SDL_FRect legL{screenX + 2.0f + legShift, renderY + 11.0f, 3.0f, 3.0f};
        SDL_FRect legR{screenX + 7.0f - legShift, renderY + 11.0f, 3.0f, 3.0f};
        SDL_RenderFillRect(renderer, &legL);
        SDL_RenderFillRect(renderer, &legR);
        drawBootCaps(legL.x, renderY + 13.0f, 3.0f);
        drawBootCaps(legR.x, renderY + 13.0f, 3.0f);

        SDL_FRect body{screenX + 1.0f, renderY + 5.0f, 10.0f, 8.0f};
        SDL_SetRenderDrawColor(renderer, 28, 47, 138, 255);
        SDL_RenderFillRect(renderer, &body);
        SDL_SetRenderDrawColor(renderer, 57, 85, 194, 255);
        SDL_FRect coatLight{screenX + 2.0f, renderY + 6.0f, 3.0f, 6.0f};
        SDL_FRect coatLightR{screenX + 7.0f, renderY + 6.0f, 2.0f, 5.0f};
        SDL_RenderFillRect(renderer, &coatLight);
        SDL_RenderFillRect(renderer, &coatLightR);
        SDL_SetRenderDrawColor(renderer, 118, 84, 41, 255);
        SDL_FRect belt{screenX + 2.0f, renderY + 9.0f, 8.0f, 1.0f};
        SDL_RenderFillRect(renderer, &belt);

        SDL_FRect head{screenX + 2.0f, renderY, 8.0f, 6.0f};
        SDL_SetRenderDrawColor(renderer, 233, 199, 158, 255);
        SDL_RenderFillRect(renderer, &head);
        SDL_SetRenderDrawColor(renderer, 249, 222, 186, 255);
        SDL_FRect faceLight{screenX + 4.0f, renderY + 1.0f, 3.0f, 2.0f};
        SDL_RenderFillRect(renderer, &faceLight);
        SDL_SetRenderDrawColor(renderer, 20, 14, 11, 255);
        SDL_FRect hairline{screenX + 2.0f, renderY, 8.0f, 2.0f};
        SDL_RenderFillRect(renderer, &hairline);
        SDL_SetRenderDrawColor(renderer, 72, 54, 39, 255);
        SDL_FRect hairLight{screenX + 3.0f, renderY + 1.0f, 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &hairLight);

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        if (blinking) {
            SDL_FRect eyeL{screenX + 4.0f, renderY + 3.0f, 1.0f, 1.0f};
            SDL_FRect eyeR{screenX + 7.0f, renderY + 3.0f, 1.0f, 1.0f};
            SDL_RenderFillRect(renderer, &eyeL);
            SDL_RenderFillRect(renderer, &eyeR);
        } else {
            SDL_FRect eyeL{screenX + 4.0f, renderY + 2.0f, 1.0f, 1.0f};
            SDL_FRect eyeR{screenX + 7.0f, renderY + 2.0f, 1.0f, 1.0f};
            SDL_RenderFillRect(renderer, &eyeL);
            SDL_RenderFillRect(renderer, &eyeR);
        }

        const float handY = renderY + 8.0f + idleHandBob;
        drawHeldToolVertical(screenX + 9.0f, handY, false);

        if (attacking_ && toolVisual_ == ToolVisual::Blade) {
            const float t = attackTimer_ / kAttackDuration;
            const float swing = std::sin(t * 3.14159265f);
            const float reach = 4.0f + std::floor(swing * 4.0f);
            const float slashY = handY + 1.0f;
            SDL_SetRenderDrawColor(renderer, 220, 226, 235, 255);
            SDL_FRect bladeSwing{screenX + 9.0f, slashY, 1.0f, reach + 1.0f};
            SDL_RenderFillRect(renderer, &bladeSwing);
            SDL_SetRenderDrawColor(renderer, 255, 242, 180, 155);
            SDL_FRect arc{screenX + 8.0f, slashY + reach, 3.0f, 1.0f};
            SDL_RenderFillRect(renderer, &arc);
        }
    } else if (facingSide) {
        SDL_SetRenderDrawColor(renderer, 22, 35, 106, 255);
        SDL_FRect legL{screenX + 2.0f + legShift, renderY + 11.0f, 3.0f, 3.0f};
        SDL_FRect legR{screenX + 7.0f - legShift, renderY + 11.0f, 3.0f, 3.0f};
        SDL_RenderFillRect(renderer, &legL);
        SDL_RenderFillRect(renderer, &legR);
        drawBootCaps(legL.x, renderY + 13.0f, 3.0f);
        drawBootCaps(legR.x, renderY + 13.0f, 3.0f);

        SDL_FRect body{screenX + 1.0f, renderY + 5.0f, 10.0f, 8.0f};
        SDL_SetRenderDrawColor(renderer, 28, 47, 138, 255);
        SDL_RenderFillRect(renderer, &body);
        SDL_SetRenderDrawColor(renderer, 57, 85, 194, 255);
        SDL_FRect coatLight{screenX + (facingX_ >= 0.0f ? 2.0f : 5.0f), renderY + 6.0f, 3.0f, 6.0f};
        SDL_RenderFillRect(renderer, &coatLight);
        SDL_SetRenderDrawColor(renderer, 118, 84, 41, 255);
        SDL_FRect belt{screenX + 2.0f, renderY + 9.0f, 8.0f, 1.0f};
        SDL_RenderFillRect(renderer, &belt);

        SDL_FRect head{screenX + 2.0f, renderY, 8.0f, 6.0f};
        SDL_SetRenderDrawColor(renderer, 233, 199, 158, 255);
        SDL_RenderFillRect(renderer, &head);
        SDL_SetRenderDrawColor(renderer, 248, 220, 183, 255);
        SDL_FRect cheekLight{screenX + (facingX_ >= 0.0f ? 5.0f : 3.0f), renderY + 1.0f, 2.0f, 2.0f};
        SDL_RenderFillRect(renderer, &cheekLight);
        SDL_SetRenderDrawColor(renderer, 20, 14, 11, 255);
        SDL_FRect hairTop{screenX + 2.0f, renderY, 8.0f, 2.0f};
        SDL_RenderFillRect(renderer, &hairTop);
        SDL_SetRenderDrawColor(renderer, 72, 54, 39, 255);
        SDL_FRect sideHair{screenX + (facingX_ >= 0.0f ? 3.0f : 6.0f), renderY + 1.0f, 1.0f, 2.0f};
        SDL_RenderFillRect(renderer, &sideHair);

        const float eyeX = (facingX_ >= 0.0f) ? (screenX + 7.0f) : (screenX + 4.0f);
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        if (blinking) {
            SDL_FRect eyeClosed{eyeX, renderY + 3.0f, 2.0f, 1.0f};
            SDL_RenderFillRect(renderer, &eyeClosed);
        } else {
            SDL_FRect eye{eyeX, renderY + 2.0f, 1.0f, 1.0f};
            SDL_RenderFillRect(renderer, &eye);
        }

        const float handX = (facingX_ >= 0.0f) ? (screenX + 10.0f) : (screenX + 1.0f);
        const float handY = renderY + 8.0f + idleHandBob;
        drawHeldToolSide(handX, handY);

        if (attacking_ && toolVisual_ == ToolVisual::Blade) {
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
}

float Player::FacingX() const {
    return facingX_ >= 0.0f ? 1.0f : -1.0f;
}

float Player::FacingY() const {
    if (facingDirection_ == FacingDirection::Up) {
        return -1.0f;
    }
    if (facingDirection_ == FacingDirection::Down) {
        return 1.0f;
    }
    return 0.0f;
}

int Player::Health() const {
    return health_;
}

int Player::MaxHealth() const {
    return maxHealth_;
}

bool Player::IsAlive() const {
    return health_ > 0;
}
