#include "game/Pet.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

#include "game/TileMap.h"

namespace {
std::string ToUpperCopy(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

bool ContainsWord(const std::string& haystackUpper, const char* needleUpper) {
    return haystackUpper.find(needleUpper) != std::string::npos;
}

bool LooksLikeFollowCommand(const std::string& upper) {
    return ContainsWord(upper, "FOLLOW") || ContainsWord(upper, "THEO");
}
}

Pet::Pet(std::string name, float x, float y)
    : Pawn(x, y, 14.0f, 10.0f, 74.0f, 1.0f),
      name_(std::move(name)),
      facingDirection_(FacingDirection::Side),
      commandMode_(CommandMode::FollowPlayer),
      homeX_(x),
      homeY_(y),
      holdX_(x),
      holdY_(y),
      animTime_(0.0f),
      idleTime_(0.0f),
      bobPhase_(0.0f),
      avoidanceCooldown_(0.0f),
      preferLeftTimer_(0.0f),
      lastTargetX_(x),
      lastTargetY_(y) {}

void Pet::OnBeginPlay() {
    animTime_ = 0.0f;
    idleTime_ = 0.0f;
    bobPhase_ = 0.0f;
    facingDirection_ = FacingDirection::Side;
    commandMode_ = CommandMode::FollowPlayer;
    holdX_ = CenterX();
    holdY_ = FeetY();
    moving_ = false;
    avoidanceCooldown_ = 0.0f;
    preferLeftTimer_ = 0.0f;
    lastTargetX_ = CenterX();
    lastTargetY_ = FeetY();
}

void Pet::OnEndPlay() {
    moving_ = false;
}

bool Pet::TryMove(const TileMap& map, float dt, float moveX, float moveY, float minTerrainMultiplier) {
    const float prevX = x_;
    const float prevY = y_;
    const float terrainMultiplier = std::clamp(map.MovementMultiplierAt(CenterX(), FeetY(), 0), minTerrainMultiplier, 1.0f);
    x_ += moveX * speed_ * terrainMultiplier * dt;
    y_ += moveY * speed_ * terrainMultiplier * dt;

    if (map.IsWaterAt(CenterX(), FeetY()) || map.IsBlockedAt(CenterX(), FeetY(), 0)) {
        x_ = prevX;
        y_ = prevY;
        return false;
    }
    return true;
}

void Pet::Update(float dt, const TileMap& map, float targetX, float targetY, bool targetMoving) {
    Tick(dt);

    float desiredX = targetX - 14.0f;
    float desiredY = targetY + 6.0f;
    bool shouldTrackTargetMovement = targetMoving;
    if (commandMode_ == CommandMode::HoldPosition) {
        desiredX = holdX_;
        desiredY = holdY_;
        shouldTrackTargetMovement = false;
    } else if (commandMode_ == CommandMode::Sit) {
        desiredX = holdX_;
        desiredY = holdY_;
        shouldTrackTargetMovement = false;
    } else if (commandMode_ == CommandMode::MoveToPoint) {
        desiredX = holdX_;
        desiredY = holdY_;
        shouldTrackTargetMovement = true;
    } else if (commandMode_ == CommandMode::ReturnHome) {
        desiredX = homeX_;
        desiredY = homeY_;
        shouldTrackTargetMovement = false;
    }

    float moveX = desiredX - CenterX();
    float moveY = desiredY - FeetY();
    const float distance = std::sqrt((moveX * moveX) + (moveY * moveY));

    const float targetDeltaX = targetX - lastTargetX_;
    const float startMoveDistance = shouldTrackTargetMovement ? 16.0f : 22.0f;
    const float stopMoveDistance = 8.0f;
    avoidanceCooldown_ = std::max(0.0f, avoidanceCooldown_ - dt);
    preferLeftTimer_ = std::max(0.0f, preferLeftTimer_ - dt);

    if (commandMode_ == CommandMode::Sit) {
        moveX = 0.0f;
        moveY = 0.0f;
        moving_ = false;
    } else if (distance > startMoveDistance) {
        moveX /= std::max(distance, 0.001f);
        moveY /= std::max(distance, 0.001f);
        moving_ = true;
    } else if (distance < stopMoveDistance) {
        moveX = 0.0f;
        moveY = 0.0f;
        moving_ = false;
        if (commandMode_ == CommandMode::MoveToPoint) {
            commandMode_ = CommandMode::HoldPosition;
            holdX_ = CenterX();
            holdY_ = FeetY();
        } else if (commandMode_ == CommandMode::ReturnHome) {
            commandMode_ = CommandMode::HoldPosition;
            holdX_ = homeX_;
            holdY_ = homeY_;
        }
    } else if (!moving_) {
        moveX = 0.0f;
        moveY = 0.0f;
    } else {
        moveX /= std::max(distance, 0.001f);
        moveY /= std::max(distance, 0.001f);
    }

    if (moving_ && std::fabs(moveX) > 0.08f) {
        facingX_ = moveX;
    }
    if (moving_) {
        if (std::fabs(moveY) > std::fabs(moveX) + 0.06f) {
            facingDirection_ = moveY < 0.0f ? FacingDirection::Up : FacingDirection::Down;
        } else {
            facingDirection_ = FacingDirection::Side;
        }
    }

    if (moving_) {
        bool moved = TryMove(map, dt, moveX, moveY, 0.65f);
        if (!moved) {
            const float sideSign =
                preferLeftTimer_ > 0.0f ? -1.0f :
                targetDeltaX >= 0.0f ? 1.0f : -1.0f;
            const float altLeftX = -moveY * sideSign;
            const float altLeftY = moveX * sideSign;
            const float altRightX = moveY * sideSign;
            const float altRightY = -moveX * sideSign;

            moved = TryMove(map, dt, altLeftX, altLeftY, 0.7f);
            if (!moved) {
                moved = TryMove(map, dt, altRightX, altRightY, 0.7f);
            }
            if (!moved) {
                moved = TryMove(map, dt, (moveX * 0.55f) + (altLeftX * 0.75f), (moveY * 0.55f) + (altLeftY * 0.75f), 0.75f);
            }
            if (!moved) {
                moved = TryMove(map, dt, (moveX * 0.55f) + (altRightX * 0.75f), (moveY * 0.55f) + (altRightY * 0.75f), 0.75f);
            }

            if (moved) {
                facingX_ = (altLeftX >= 0.0f || altRightX >= 0.0f) ? 1.0f : -1.0f;
                preferLeftTimer_ = sideSign > 0.0f ? 0.55f : 0.0f;
                avoidanceCooldown_ = 0.28f;
            } else {
                moving_ = false;
                avoidanceCooldown_ = 0.35f;
                preferLeftTimer_ = sideSign > 0.0f ? 0.0f : 0.55f;
            }
        } else if (avoidanceCooldown_ <= 0.0f) {
            preferLeftTimer_ = 0.0f;
        }
    }

    if (moving_) {
        animTime_ += dt * 11.0f;
        idleTime_ = 0.0f;
    } else {
        idleTime_ += dt;
    }
    bobPhase_ += dt * 4.5f;
    lastTargetX_ = targetX;
    lastTargetY_ = targetY;
}

std::string Pet::ApplyPlayerCommand(const std::string& message, float playerX, float playerY) {
    const std::string upper = ToUpperCopy(message);
    const bool namedOnly =
        upper == "MIKO" || upper == "MIKO!" || upper == "MIKO?" || upper == "HEY MIKO" || upper == "COME MIKO" ||
        upper == "MIKO COME";

    if (LooksLikeFollowCommand(upper)) {
        commandMode_ = CommandMode::FollowPlayer;
        return name_ + " IS FOLLOWING YOU.";
    }

    if (ContainsWord(upper, "SIT") || ContainsWord(upper, "NGOI")) {
        commandMode_ = CommandMode::Sit;
        holdX_ = CenterX();
        holdY_ = FeetY();
        return name_ + " SITS DOWN.";
    }

    if (ContainsWord(upper, "STOP") || ContainsWord(upper, "STAY") || ContainsWord(upper, "DUNG") ||
        ContainsWord(upper, "HERE") || ContainsWord(upper, "DAY")) {
        commandMode_ = CommandMode::HoldPosition;
        holdX_ = CenterX();
        holdY_ = FeetY();
        return name_ + " IS STAYING HERE.";
    }

    if (ContainsWord(upper, "COME") || ContainsWord(upper, "LAI") || ContainsWord(upper, "NEAR") ||
        ContainsWord(upper, "GAN") || namedOnly) {
        commandMode_ = CommandMode::MoveToPoint;
        holdX_ = playerX;
        holdY_ = playerY;
        return name_ + " IS COMING TO YOU.";
    }

    if (ContainsWord(upper, "HOME") || ContainsWord(upper, "RETURN") || ContainsWord(upper, "BACK") ||
        ContainsWord(upper, "VE")) {
        commandMode_ = CommandMode::ReturnHome;
        return name_ + " IS GOING HOME.";
    }

    return name_ + " HEARD YOU, BUT THE ORDER IS UNCLEAR.";
}

std::string Pet::ApplyAiDirective(const std::string& intent, const std::string& speech, float playerX, float playerY) {
    const std::string upperIntent = ToUpperCopy(intent);
    const std::string spoken = speech.empty() ? (name_ + " perks up.") : speech;

    if (ContainsWord(upperIntent, "FOLLOW")) {
        commandMode_ = CommandMode::FollowPlayer;
        return spoken;
    }
    if (ContainsWord(upperIntent, "SIT")) {
        commandMode_ = CommandMode::Sit;
        holdX_ = CenterX();
        holdY_ = FeetY();
        return spoken;
    }
    if (ContainsWord(upperIntent, "HOLD") || ContainsWord(upperIntent, "STAY")) {
        commandMode_ = CommandMode::HoldPosition;
        holdX_ = CenterX();
        holdY_ = FeetY();
        return spoken;
    }
    if (ContainsWord(upperIntent, "COME")) {
        commandMode_ = CommandMode::MoveToPoint;
        holdX_ = playerX;
        holdY_ = playerY;
        return spoken;
    }
    if (ContainsWord(upperIntent, "RETURN") || ContainsWord(upperIntent, "HOME")) {
        commandMode_ = CommandMode::ReturnHome;
        return spoken;
    }

    return spoken;
}

bool Pet::MatchesName(const std::string& messageUpper) const {
    return ContainsWord(messageUpper, "MIKO");
}

void Pet::DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const {
    const float screenX = std::floor(x_ - camera.x);
    const float screenY = std::floor(y_ - camera.y);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 58);
    SDL_FRect shadow{screenX + 1.0f, screenY + height_ - 1.0f, width_ - 2.0f, 2.0f};
    SDL_RenderFillRect(renderer, &shadow);
}

void Pet::Draw(SDL_Renderer* renderer, const Camera2D& camera) const {
    const float screenX = std::floor(x_ - camera.x);
    const float screenY = std::floor(y_ - camera.y - (moving_ ? 0.0f : std::sin(bobPhase_) * 0.35f));
    const float legShift = moving_ ? ((static_cast<int>(animTime_) % 2 == 0) ? 1.0f : -1.0f) : 0.0f;
    const bool sitting = commandMode_ == CommandMode::Sit;
    if (facingDirection_ == FacingDirection::Up) {
        SDL_SetRenderDrawColor(renderer, 32, 36, 42, 255);
        SDL_FRect body{screenX + 3.0f, screenY + (sitting ? 4.0f : 3.0f), 8.0f, sitting ? 4.0f : 5.0f};
        SDL_FRect neck{screenX + 5.0f, screenY + 1.0f, 4.0f, 3.0f};
        SDL_FRect tail{screenX + 5.0f, screenY + (sitting ? 7.0f : 7.0f), 3.0f, sitting ? 1.0f : 2.0f};
        SDL_RenderFillRect(renderer, &body);
        SDL_RenderFillRect(renderer, &neck);
        SDL_RenderFillRect(renderer, &tail);

        SDL_SetRenderDrawColor(renderer, 238, 238, 234, 255);
        SDL_FRect head{screenX + 4.0f, screenY + 0.0f, 6.0f, 3.0f};
        SDL_FRect blaze{screenX + 6.0f, screenY + 0.0f, 2.0f, 4.0f};
        SDL_FRect ruff{screenX + 4.0f, screenY + 3.0f, 6.0f, 3.0f};
        SDL_FRect tailTip{screenX + 5.0f, screenY + 8.0f, 3.0f, 1.0f};
        SDL_RenderFillRect(renderer, &head);
        SDL_RenderFillRect(renderer, &blaze);
        SDL_RenderFillRect(renderer, &ruff);
        SDL_RenderFillRect(renderer, &tailTip);

        SDL_SetRenderDrawColor(renderer, 28, 32, 38, 255);
        SDL_FRect earL{screenX + 4.0f, screenY - 1.0f, 1.0f, 2.0f};
        SDL_FRect earR{screenX + 9.0f, screenY - 1.0f, 1.0f, 2.0f};
        SDL_FRect backPatch{screenX + 4.0f, screenY + 4.0f, 6.0f, 3.0f};
        SDL_RenderFillRect(renderer, &earL);
        SDL_RenderFillRect(renderer, &earR);
        SDL_RenderFillRect(renderer, &backPatch);

        SDL_SetRenderDrawColor(renderer, 46, 40, 36, 255);
        SDL_FRect legL{screenX + 4.0f + (sitting ? 0.0f : legShift), screenY + 8.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_FRect legML{screenX + 6.0f - (sitting ? 0.0f : legShift), screenY + 8.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_FRect legMR{screenX + 8.0f + (sitting ? 0.0f : legShift), screenY + 8.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_FRect legR{screenX + 10.0f - (sitting ? 0.0f : legShift), screenY + 8.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_RenderFillRect(renderer, &legL);
        SDL_RenderFillRect(renderer, &legML);
        SDL_RenderFillRect(renderer, &legMR);
        SDL_RenderFillRect(renderer, &legR);
    } else if (facingDirection_ == FacingDirection::Down) {
        SDL_SetRenderDrawColor(renderer, 32, 36, 42, 255);
        SDL_FRect body{screenX + 3.0f, screenY + (sitting ? 4.0f : 3.0f), 8.0f, sitting ? 4.0f : 5.0f};
        SDL_FRect tail{screenX + 5.0f, screenY + 1.0f, 3.0f, sitting ? 1.0f : 2.0f};
        SDL_RenderFillRect(renderer, &body);
        SDL_RenderFillRect(renderer, &tail);

        SDL_SetRenderDrawColor(renderer, 238, 238, 234, 255);
        SDL_FRect face{screenX + 4.0f, screenY + 1.0f, 6.0f, 4.0f};
        SDL_FRect blaze{screenX + 6.0f, screenY + 1.0f, 2.0f, 5.0f};
        SDL_FRect chest{screenX + 4.0f, screenY + 5.0f, 6.0f, sitting ? 2.0f : 3.0f};
        SDL_RenderFillRect(renderer, &face);
        SDL_RenderFillRect(renderer, &blaze);
        SDL_RenderFillRect(renderer, &chest);

        SDL_SetRenderDrawColor(renderer, 28, 32, 38, 255);
        SDL_FRect earL{screenX + 4.0f, screenY + 0.0f, 1.0f, 2.0f};
        SDL_FRect earR{screenX + 9.0f, screenY + 0.0f, 1.0f, 2.0f};
        SDL_FRect cheekL{screenX + 3.0f, screenY + 3.0f, 1.0f, 2.0f};
        SDL_FRect cheekR{screenX + 10.0f, screenY + 3.0f, 1.0f, 2.0f};
        SDL_RenderFillRect(renderer, &earL);
        SDL_RenderFillRect(renderer, &earR);
        SDL_RenderFillRect(renderer, &cheekL);
        SDL_RenderFillRect(renderer, &cheekR);

        SDL_SetRenderDrawColor(renderer, 24, 20, 18, 255);
        SDL_FRect eyeL{screenX + 5.0f, screenY + 3.0f, 1.0f, 1.0f};
        SDL_FRect eyeR{screenX + 8.0f, screenY + 3.0f, 1.0f, 1.0f};
        SDL_FRect nose{screenX + 6.0f, screenY + 5.0f, 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &eyeL);
        SDL_RenderFillRect(renderer, &eyeR);
        SDL_RenderFillRect(renderer, &nose);

        SDL_SetRenderDrawColor(renderer, 46, 40, 36, 255);
        SDL_FRect legL{screenX + 4.0f + (sitting ? 0.0f : legShift), screenY + 8.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_FRect legML{screenX + 6.0f - (sitting ? 0.0f : legShift), screenY + 8.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_FRect legMR{screenX + 8.0f + (sitting ? 0.0f : legShift), screenY + 8.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_FRect legR{screenX + 10.0f - (sitting ? 0.0f : legShift), screenY + 8.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_RenderFillRect(renderer, &legL);
        SDL_RenderFillRect(renderer, &legML);
        SDL_RenderFillRect(renderer, &legMR);
        SDL_RenderFillRect(renderer, &legR);
    } else {
        const bool facingRight = facingX_ >= 0.0f;
        const float headX = facingRight ? screenX + width_ - 5.0f : screenX + 1.0f;
        const float muzzleX = facingRight ? screenX + width_ - 2.0f : screenX - 1.0f;
        const float tailX = facingRight ? screenX - 2.0f : screenX + width_ - 1.0f;

        SDL_SetRenderDrawColor(renderer, 32, 36, 42, 255);
        SDL_FRect body{screenX + 2.0f, screenY + (sitting ? 4.0f : 3.0f), 8.0f, sitting ? 3.0f : 4.0f};
        SDL_FRect haunch{screenX + (facingRight ? 1.0f : 5.0f), screenY + (sitting ? 5.0f : 4.0f), 4.0f, sitting ? 3.0f : 3.0f};
        SDL_FRect head{headX, screenY + 2.0f, 4.0f, 3.0f};
        SDL_FRect tailBase{tailX, screenY + (sitting ? 4.0f : 2.0f), 3.0f, sitting ? 1.0f : 2.0f};
        SDL_RenderFillRect(renderer, &body);
        SDL_RenderFillRect(renderer, &haunch);
        SDL_RenderFillRect(renderer, &head);
        SDL_RenderFillRect(renderer, &tailBase);

        SDL_SetRenderDrawColor(renderer, 238, 238, 234, 255);
        SDL_FRect blaze{headX + (facingRight ? 1.0f : 2.0f), screenY + 2.0f, 1.0f, 3.0f};
        SDL_FRect cheek{headX + (facingRight ? 1.0f : 0.0f), screenY + 4.0f, 2.0f, 1.0f};
        SDL_FRect ruff{screenX + 4.0f, screenY + 3.0f, 3.0f, sitting ? 3.0f : 4.0f};
        SDL_FRect belly{screenX + 5.0f, screenY + 6.0f, 3.0f, 1.0f};
        SDL_FRect tailTip{tailX + (facingRight ? 1.0f : 0.0f), screenY + (sitting ? 3.0f : 1.0f), 2.0f, sitting ? 1.0f : 2.0f};
        SDL_RenderFillRect(renderer, &blaze);
        SDL_RenderFillRect(renderer, &cheek);
        SDL_RenderFillRect(renderer, &ruff);
        SDL_RenderFillRect(renderer, &belly);
        SDL_RenderFillRect(renderer, &tailTip);

        SDL_SetRenderDrawColor(renderer, 28, 32, 38, 255);
        SDL_FRect earFront{headX + (facingRight ? 2.0f : 1.0f), screenY + 0.0f, 1.0f, 3.0f};
        SDL_FRect earBack{headX + (facingRight ? 0.0f : 3.0f), screenY + 1.0f, 1.0f, 2.0f};
        SDL_FRect snout{muzzleX, screenY + 4.0f, 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &earFront);
        SDL_RenderFillRect(renderer, &earBack);
        SDL_RenderFillRect(renderer, &snout);

        SDL_SetRenderDrawColor(renderer, 46, 40, 36, 255);
        SDL_FRect legFront{screenX + (facingRight ? 8.0f : 3.0f) - (sitting ? 0.0f : legShift), screenY + 7.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_FRect legBack{screenX + (facingRight ? 3.0f : 8.0f) + (sitting ? 0.0f : legShift), screenY + 7.0f, 1.0f, sitting ? 2.0f : 2.0f};
        SDL_FRect legMidA{screenX + 5.0f + (sitting ? 0.0f : legShift), screenY + 7.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_FRect legMidB{screenX + 7.0f - (sitting ? 0.0f : legShift), screenY + 7.0f, 1.0f, sitting ? 1.0f : 2.0f};
        SDL_RenderFillRect(renderer, &legFront);
        SDL_RenderFillRect(renderer, &legBack);
        SDL_RenderFillRect(renderer, &legMidA);
        SDL_RenderFillRect(renderer, &legMidB);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_FRect sockA{legFront.x, screenY + 8.0f, 1.0f, 1.0f};
        SDL_FRect sockB{legBack.x, screenY + 8.0f, 1.0f, 1.0f};
        SDL_FRect sockC{legMidA.x, screenY + 8.0f, 1.0f, 1.0f};
        SDL_FRect sockD{legMidB.x, screenY + 8.0f, 1.0f, 1.0f};
        SDL_RenderFillRect(renderer, &sockA);
        SDL_RenderFillRect(renderer, &sockB);
        SDL_RenderFillRect(renderer, &sockC);
        SDL_RenderFillRect(renderer, &sockD);

        SDL_SetRenderDrawColor(renderer, 24, 20, 18, 255);
        SDL_FRect eye{headX + (facingRight ? 2.0f : 1.0f), screenY + 3.0f, 1.0f, 1.0f};
        SDL_FRect nose{muzzleX + (facingRight ? 1.0f : 0.0f), screenY + 4.0f, 1.0f, 1.0f};
        SDL_RenderFillRect(renderer, &eye);
        SDL_RenderFillRect(renderer, &nose);
    }
}

const std::string& Pet::Name() const {
    return name_;
}
