#include "game/HostileAI.h"

#include <algorithm>
#include <cmath>

#include "game/TileMap.h"

namespace {
float DistanceSq(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return (dx * dx) + (dy * dy);
}
}

HostileAI::HostileAI(float x, float y, HostileKind kind, std::uint32_t seed)
    : x_(x),
      y_(y),
      width_(kind == HostileKind::Ghoul ? 14.0f : (kind == HostileKind::Wraith ? 10.0f : 12.0f)),
      height_(kind == HostileKind::Ghoul ? 16.0f : (kind == HostileKind::Wraith ? 15.0f : 14.0f)),
      speed_(
          kind == HostileKind::Zombie ? 26.0f :
          kind == HostileKind::Marauder ? 36.0f :
          kind == HostileKind::Ghoul ? 31.0f : 42.0f),
      facingX_((seed % 2U) == 0U ? 1.0f : -1.0f),
      animTime_(0.0f),
      attackCooldown_(0.0f),
      emergeTimer_(0.85f),
      emergeDuration_(0.85f),
      health_(
          kind == HostileKind::Zombie ? 5 :
          kind == HostileKind::Marauder ? 4 :
          kind == HostileKind::Ghoul ? 7 : 3),
      kind_(kind),
      label_(
          kind == HostileKind::Zombie ? "ZOMBIE" :
          kind == HostileKind::Marauder ? "MARAUDER" :
          kind == HostileKind::Ghoul ? "GHOUL" : "WRAITH") {}

void HostileAI::SetPosition(float x, float y) {
    x_ = x;
    y_ = y;
}

void HostileAI::Update(float dt, const TileMap& map, float targetX, float targetY, bool hasTarget) {
    if (!IsAlive()) {
        return;
    }

    attackCooldown_ = std::max(0.0f, attackCooldown_ - dt);
    emergeTimer_ = std::max(0.0f, emergeTimer_ - dt);

    if (IsEmerging()) {
        animTime_ += dt * 5.5f;
        return;
    }

    if (!hasTarget) {
        return;
    }

    float moveX = targetX - CenterX();
    float moveY = targetY - FeetY();
    const float distance = std::sqrt((moveX * moveX) + (moveY * moveY));
    if (distance < 1.0f) {
        return;
    }

    moveX /= distance;
    moveY /= distance;
    if (std::fabs(moveX) > 0.05f) {
        facingX_ = moveX;
    }

    const float prevX = x_;
    const float prevY = y_;
    const float moveMultiplier = map.MovementMultiplierAt(CenterX(), FeetY(), 0);
    const float finalSpeed = speed_ * std::max(moveMultiplier, 0.7f);
    x_ += moveX * finalSpeed * dt;
    y_ += moveY * finalSpeed * dt;

    if (map.IsBlockedAt(CenterX(), FeetY(), 0) || map.IsWaterAt(CenterX(), FeetY())) {
        x_ = prevX;
        y_ = prevY;
        return;
    }

    animTime_ += dt * (
        kind_ == HostileKind::Zombie ? 8.0f :
        kind_ == HostileKind::Marauder ? 10.5f :
        kind_ == HostileKind::Ghoul ? 6.4f : 14.0f);
}

void HostileAI::DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const {
    const float screenX = std::floor(x_ - camera.x);
    const float screenY = std::floor(y_ - camera.y);
    const float emergeRatio = emergeDuration_ > 0.0f ? (1.0f - (emergeTimer_ / emergeDuration_)) : 1.0f;
    const float shadowScale = std::clamp(0.25f + (emergeRatio * 0.75f), 0.25f, 1.0f);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, kind_ == HostileKind::Wraith ? 48 : 82);
    SDL_FRect shadow{
        screenX + ((width_ * (1.0f - shadowScale)) * 0.5f),
        screenY + height_ - 2.0f,
        width_ * shadowScale,
        (kind_ == HostileKind::Ghoul ? 3.0f : 2.4f) * shadowScale
    };
    SDL_RenderFillRect(renderer, &shadow);

    if (IsEmerging()) {
        const float crack = std::clamp(0.2f + (emergeRatio * 0.8f), 0.2f, 1.0f);
        const float groundY = screenY + height_ - 1.0f;
        SDL_SetRenderDrawColor(renderer, 42, 28, 18, 205);
        SDL_FRect crackL{screenX - 1.0f, groundY, (width_ * 0.55f) * crack, 1.5f};
        SDL_FRect crackR{screenX + width_ - ((width_ * 0.55f) * crack) + 1.0f, groundY, (width_ * 0.55f) * crack, 1.5f};
        SDL_RenderFillRect(renderer, &crackL);
        SDL_RenderFillRect(renderer, &crackR);
    }
}

void HostileAI::Draw(SDL_Renderer* renderer, const Camera2D& camera) const {
    const float screenX = std::floor(x_ - camera.x);
    const float screenY = std::floor(y_ - camera.y);
    const float emergeRatio = emergeDuration_ > 0.0f ? (1.0f - (emergeTimer_ / emergeDuration_)) : 1.0f;
    const float emergeBuried = IsEmerging() ? std::floor((1.0f - emergeRatio) * (height_ * 0.75f)) : 0.0f;
    const float step = (static_cast<int>(animTime_) % 2 == 0) ? 1.0f : -1.0f;
    const float eyeX = facingX_ >= 0.0f ? (screenX + width_ - 4.0f) : (screenX + 2.0f);

    if (kind_ == HostileKind::Zombie) {
        SDL_SetRenderDrawColor(renderer, 55, 93, 63, 255);
        SDL_FRect torso{screenX + 2.0f, screenY + 5.0f, width_ - 4.0f, 7.0f};
        SDL_FRect shoulder{screenX + 1.0f, screenY + 6.0f, width_ - 2.0f, 3.0f};
        SDL_RenderFillRect(renderer, &torso);
        SDL_RenderFillRect(renderer, &shoulder);

        SDL_SetRenderDrawColor(renderer, 130, 159, 109, 255);
        SDL_FRect head{screenX + 2.0f, screenY, width_ - 4.0f, 6.0f};
        SDL_RenderFillRect(renderer, &head);

        SDL_SetRenderDrawColor(renderer, 89, 44, 41, 255);
        SDL_FRect tear{screenX + width_ - 5.0f, screenY + 3.0f, 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &tear);

        SDL_SetRenderDrawColor(renderer, 206, 240, 102, 255);
        SDL_FRect eye{eyeX, screenY + 2.0f, 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &eye);
    } else if (kind_ == HostileKind::Marauder) {
        SDL_SetRenderDrawColor(renderer, 74, 34, 27, 255);
        SDL_FRect cloak{screenX + 1.0f, screenY + 4.0f, width_ - 2.0f, 9.0f};
        SDL_RenderFillRect(renderer, &cloak);

        SDL_SetRenderDrawColor(renderer, 38, 23, 20, 255);
        SDL_FRect hood{screenX + 2.0f, screenY, width_ - 4.0f, 6.0f};
        SDL_FRect brim{screenX + 1.0f, screenY + 2.0f, width_ - 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &hood);
        SDL_RenderFillRect(renderer, &brim);

        SDL_SetRenderDrawColor(renderer, 237, 101, 71, 255);
        SDL_FRect eye{eyeX, screenY + 2.0f, 2.0f, 1.0f};
        SDL_RenderFillRect(renderer, &eye);

        SDL_SetRenderDrawColor(renderer, 156, 112, 64, 255);
        SDL_FRect blade{facingX_ >= 0.0f ? screenX + width_ - 1.0f : screenX - 2.0f, screenY + 7.0f, 2.0f, 4.0f};
        SDL_RenderFillRect(renderer, &blade);
    } else if (kind_ == HostileKind::Ghoul) {
        SDL_SetRenderDrawColor(renderer, 71, 64, 83, 255);
        SDL_FRect back{screenX + 2.0f, screenY + 6.0f, width_ - 5.0f, 7.0f};
        SDL_FRect hump{screenX + 3.0f, screenY + 2.0f, width_ - 7.0f, 5.0f};
        SDL_RenderFillRect(renderer, &back);
        SDL_RenderFillRect(renderer, &hump);

        SDL_SetRenderDrawColor(renderer, 153, 138, 125, 255);
        SDL_FRect skull{screenX + width_ - 6.0f, screenY + 1.0f, 4.0f, 4.0f};
        SDL_RenderFillRect(renderer, &skull);

        SDL_SetRenderDrawColor(renderer, 210, 213, 122, 255);
        SDL_FRect eye{facingX_ >= 0.0f ? screenX + width_ - 4.0f : screenX + 2.0f, screenY + 2.0f, 1.0f, 1.0f};
        SDL_RenderFillRect(renderer, &eye);

        SDL_SetRenderDrawColor(renderer, 92, 82, 69, 255);
        SDL_FRect clawL{screenX + 1.0f, screenY + 11.0f + step, 2.0f, 3.0f};
        SDL_FRect clawR{screenX + width_ - 3.0f, screenY + 10.0f - step, 2.0f, 4.0f};
        SDL_RenderFillRect(renderer, &clawL);
        SDL_RenderFillRect(renderer, &clawR);
    } else {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 79, 42, 99, 150);
        SDL_FRect veil{screenX + 2.0f, screenY + 3.0f, width_ - 4.0f, 10.0f};
        SDL_RenderFillRect(renderer, &veil);

        SDL_SetRenderDrawColor(renderer, 132, 92, 182, 195);
        SDL_FRect core{screenX + 3.0f, screenY + 1.0f, width_ - 6.0f, 9.0f};
        SDL_RenderFillRect(renderer, &core);

        SDL_SetRenderDrawColor(renderer, 170, 238, 255, 255);
        SDL_FRect eyeA{screenX + 3.0f, screenY + 3.0f, 1.0f, 1.0f};
        SDL_FRect eyeB{screenX + width_ - 4.0f, screenY + 3.0f, 1.0f, 1.0f};
        SDL_RenderFillRect(renderer, &eyeA);
        SDL_RenderFillRect(renderer, &eyeB);

        SDL_SetRenderDrawColor(renderer, 100, 67, 138, 150);
        SDL_FRect tail{screenX + 4.0f, screenY + 10.0f, width_ - 8.0f, 4.0f};
        SDL_RenderFillRect(renderer, &tail);
    }

    if (kind_ != HostileKind::Wraith) {
        SDL_SetRenderDrawColor(renderer, 42, 30, 25, 255);
        SDL_FRect legL{screenX + 2.0f + step, screenY + height_ - 3.0f, 2.0f, 3.0f};
        SDL_FRect legR{screenX + width_ - 4.0f - step, screenY + height_ - 3.0f, 2.0f, 3.0f};
        SDL_RenderFillRect(renderer, &legL);
        SDL_RenderFillRect(renderer, &legR);
    }

    if (IsEmerging()) {
        const float groundY = screenY + height_ - 1.0f;
        const float coverHeight = std::max(1.0f, emergeBuried);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        if (kind_ == HostileKind::Wraith) {
            SDL_SetRenderDrawColor(renderer, 76, 52, 102, 155);
        } else {
            SDL_SetRenderDrawColor(renderer, 88, 64, 44, 228);
        }
        SDL_FRect cover{screenX - 1.0f, groundY - coverHeight, width_ + 2.0f, coverHeight + 1.0f};
        SDL_RenderFillRect(renderer, &cover);

        SDL_SetRenderDrawColor(renderer, 58, 40, 26, 240);
        SDL_FRect rim{screenX - 1.0f, groundY - 1.0f, width_ + 2.0f, 2.0f};
        SDL_RenderFillRect(renderer, &rim);
    }
}

bool HostileAI::IsReadyToAttack(float targetX, float targetY) const {
    return IsAlive() && !IsEmerging() && attackCooldown_ <= 0.0f && DistanceSq(CenterX(), FeetY(), targetX, targetY) <= (12.0f * 12.0f);
}

bool HostileAI::IsEmerging() const {
    return emergeTimer_ > 0.0f;
}

void HostileAI::ResetAttackCooldown() {
    attackCooldown_ =
        kind_ == HostileKind::Zombie ? 1.1f :
        kind_ == HostileKind::Marauder ? 0.8f :
        kind_ == HostileKind::Ghoul ? 1.35f : 0.65f;
}

void HostileAI::ApplyDamage(int amount) {
    health_ = std::max(0, health_ - std::max(amount, 0));
}

bool HostileAI::IsAlive() const {
    return health_ > 0;
}

HostileKind HostileAI::Kind() const {
    return kind_;
}

const std::string& HostileAI::Label() const {
    return label_;
}

float HostileAI::X() const {
    return x_;
}

float HostileAI::Y() const {
    return y_;
}

float HostileAI::CenterX() const {
    return x_ + (width_ * 0.5f);
}

float HostileAI::CenterY() const {
    return y_ + (height_ * 0.5f);
}

float HostileAI::FeetY() const {
    return y_ + height_;
}
