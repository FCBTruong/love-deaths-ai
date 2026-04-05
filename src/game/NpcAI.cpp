#include "game/NpcAI.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <utility>

#include "game/TileMap.h"

namespace {
float DistanceSq(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return (dx * dx) + (dy * dy);
}

std::string ToUpperCopy(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

bool ContainsWord(const std::string& haystackUpper, const char* needleUpper) {
    return haystackUpper.find(needleUpper) != std::string::npos;
}
}

NpcAI::NpcAI(
    std::string name,
    std::string role,
    std::string trait,
    std::string focus,
    float x,
    float y,
    std::uint32_t seed
)
    : name_(std::move(name)),
      role_(std::move(role)),
      trait_(std::move(trait)),
      focus_(std::move(focus)),
      x_(x),
      y_(y),
      homeX_(x),
      homeY_(y),
      width_(12.0f),
      height_(14.0f),
      speed_(28.0f),
      dirX_(1.0f),
      dirY_(0.0f),
      facingX_(1.0f),
      moving_(false),
      behaviorTimer_(0.0f),
      animTime_(0.0f),
      thoughtTimer_(1.0f),
      thoughtIndex_(0),
      talkIndex_(0),
      bubbleTimer_(0.0f),
      bubbleText_(),
      health_(5),
      commandMode_(CommandMode::Wander),
      holdX_(x),
      holdY_(y),
      rng_(seed) {
    PickNewBehavior();
}

void NpcAI::PickNewBehavior() {
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    std::uniform_real_distribution<float> moveTime(0.8f, 2.4f);
    std::uniform_real_distribution<float> idleTime(0.6f, 1.6f);
    std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);

    moving_ = chance(rng_) > 0.38f;
    behaviorTimer_ = moving_ ? moveTime(rng_) : idleTime(rng_);
    if (moving_) {
        const float a = angle(rng_);
        dirX_ = std::cos(a);
        dirY_ = std::sin(a);
        if (std::fabs(dirX_) > 0.08f) {
            facingX_ = dirX_;
        }
    }
}

void NpcAI::SetBubble(std::string text, float duration) {
    bubbleText_ = std::move(text);
    bubbleTimer_ = duration;
}

std::string NpcAI::BuildThoughtText() const {
    static const std::array<const char*, 5> kPrefixes{
        "THINKING ABOUT",
        "KEEPING WATCH ON",
        "PLANNING FOR",
        "WORRYING ABOUT",
        "HOPING FOR"
    };

    const std::string prefix = kPrefixes[static_cast<std::size_t>(thoughtIndex_) % kPrefixes.size()];
    return prefix + " " + focus_;
}

void NpcAI::Update(float dt, const TileMap& map, float playerX, float playerY) {
    if (!IsAlive()) {
        bubbleTimer_ = std::max(0.0f, bubbleTimer_ - dt);
        return;
    }

    behaviorTimer_ -= dt;
    if (behaviorTimer_ <= 0.0f) {
        PickNewBehavior();
    }

    thoughtTimer_ -= dt;
    if (bubbleTimer_ > 0.0f) {
        bubbleTimer_ = std::max(0.0f, bubbleTimer_ - dt);
        if (bubbleTimer_ == 0.0f) {
            bubbleText_.clear();
        }
    }

    if (thoughtTimer_ <= 0.0f) {
        thoughtTimer_ = 4.0f + (static_cast<float>((thoughtIndex_ % 3)) * 1.4f);
        ++thoughtIndex_;
        if (IsPlayerNear(playerX, playerY)) {
            SetBubble(BuildThoughtText(), 2.4f);
        }
    }

    if (commandMode_ != CommandMode::Wander) {
        float targetX = x_;
        float targetY = y_;
        float stopDistance = 8.0f;

        if (commandMode_ == CommandMode::FollowPlayer) {
            targetX = playerX - (facingX_ >= 0.0f ? 10.0f : -10.0f);
            targetY = playerY - 6.0f;
            stopDistance = 16.0f;
        } else if (commandMode_ == CommandMode::HoldPosition) {
            targetX = holdX_;
            targetY = holdY_;
            stopDistance = 4.0f;
        } else if (commandMode_ == CommandMode::ReturnHome) {
            targetX = homeX_;
            targetY = homeY_;
            stopDistance = 6.0f;
        }

        const float toTargetX = targetX - CenterX();
        const float toTargetY = targetY - FeetY();
        const float distanceSq = (toTargetX * toTargetX) + (toTargetY * toTargetY);
        if (distanceSq <= (stopDistance * stopDistance)) {
            moving_ = false;
            animTime_ = 0.0f;
            if (commandMode_ == CommandMode::ReturnHome) {
                commandMode_ = CommandMode::Wander;
                behaviorTimer_ = 0.0f;
            }
            return;
        }

        const float distance = std::sqrt(distanceSq);
        dirX_ = toTargetX / std::max(distance, 0.001f);
        dirY_ = toTargetY / std::max(distance, 0.001f);
        moving_ = true;
        behaviorTimer_ = 0.15f;
        if (std::fabs(dirX_) > 0.08f) {
            facingX_ = dirX_;
        }
    }

    if (!moving_) {
        animTime_ = 0.0f;
        return;
    }

    const float prevX = x_;
    const float prevY = y_;
    const float moveMultiplier = map.MovementMultiplierAt(x_ + (width_ * 0.5f), y_ + height_, 0);
    const float moveSpeed = speed_ * std::max(moveMultiplier, 0.55f);

    x_ += dirX_ * moveSpeed * dt;
    y_ += dirY_ * moveSpeed * dt;

    const float centerX = x_ + (width_ * 0.5f);
    const float feetY = y_ + height_;
    if (map.IsBlockedAt(centerX, feetY, 0) || map.IsWaterAt(centerX, feetY)) {
        x_ = prevX;
        y_ = prevY;
        moving_ = false;
        behaviorTimer_ = 0.0f;
        return;
    }

    if (DistanceSq(centerX, feetY, playerX, playerY) < (20.0f * 20.0f)) {
        x_ = prevX;
        y_ = prevY;
        moving_ = false;
        behaviorTimer_ = 0.3f;
        return;
    }

    animTime_ += dt * 9.0f;
}

std::string NpcAI::TriggerConversation(int appleCount, int berryCount, int pearCount, int meatCount) {
    static const std::array<const char*, 4> kOpeners{
        "HELLO TRAVELER.",
        "I HAVE BEEN WAITING.",
        "YOU LOOK BUSY.",
        "THE LAND IS LISTENING."
    };
    static const std::array<const char*, 4> kClosers{
        "STAY CURIOUS.",
        "KEEP YOUR STEPS LIGHT.",
        "DO NOT WASTE THE DAY.",
        "COME BACK SOON."
    };

    const int resourceTotal = appleCount + berryCount + pearCount + meatCount;
    std::string middle = "I AM " + name_ + " THE " + role_ + ".";
    if (resourceTotal > 6) {
        middle += " YOU CARRY A GOOD HARVEST.";
    } else if (meatCount > appleCount + berryCount + pearCount) {
        middle += " YOU HUNT MORE THAN YOU GATHER.";
    } else if (appleCount + berryCount + pearCount > 0) {
        middle += " THE ORCHARD HAS NOTICED YOU.";
    } else {
        middle += " THE WORLD STILL NEEDS YOUR STORY.";
    }

    middle += " I VALUE " + trait_ + ".";
    middle += " MY MIND STAYS ON " + focus_ + ".";

    const std::string line =
        std::string(kOpeners[static_cast<std::size_t>(talkIndex_) % kOpeners.size()]) + " " + middle + " " +
        kClosers[static_cast<std::size_t>(talkIndex_) % kClosers.size()];
    ++talkIndex_;
    SetBubble(line, 5.2f);
    return line;
}

std::string NpcAI::RespondToMessage(const std::string& message, int appleCount, int berryCount, int pearCount, int meatCount) {
    if (!IsAlive()) {
        return name_ + " CAN NO LONGER ANSWER.";
    }

    const std::string upper = ToUpperCopy(message);
    std::string reply;

    if (ContainsWord(upper, "HELLO") || ContainsWord(upper, "HI")) {
        reply = "HELLO. I AM " + name_ + ". I WALK AS THE " + role_ + ".";
    } else if (ContainsWord(upper, "WHO") || ContainsWord(upper, "NAME")) {
        reply = "MY NAME IS " + name_ + ". PEOPLE KNOW ME FOR " + trait_ + ".";
    } else if (ContainsWord(upper, "THINK") || ContainsWord(upper, "MIND") || ContainsWord(upper, "WHY")) {
        reply = "I KEEP THINKING ABOUT " + focus_ + ". THAT IS MY SHAPE INSIDE.";
    } else if (ContainsWord(upper, "APPLE") || ContainsWord(upper, "BERRY") || ContainsWord(upper, "PEAR") ||
               ContainsWord(upper, "FOOD")) {
        const int fruits = appleCount + berryCount + pearCount;
        if (fruits > 0) {
            reply = "YOU ALREADY CARRY FRUIT. SHARE IT WISELY.";
        } else {
            reply = "THE TREES STILL OWE YOU A HARVEST.";
        }
    } else if (ContainsWord(upper, "MEAT") || ContainsWord(upper, "HUNT")) {
        reply = meatCount > 0 ? "YOU HAVE ENOUGH TO SURVIVE FOR NOW." : "HUNT ONLY WHEN THE LAND DEMANDS IT.";
    } else if (ContainsWord(upper, "WORK") || ContainsWord(upper, "JOB") || ContainsWord(upper, "ROLE")) {
        reply = "I SERVE AS A " + role_ + ". I TRUST " + trait_ + " MORE THAN SPEED.";
    } else if (ContainsWord(upper, "BYE") || ContainsWord(upper, "LEAVE")) {
        reply = "GO WELL. RETURN WHEN YOUR THOUGHTS GROW HEAVY.";
    } else {
        reply = "I HEAR YOU. THROUGH ALL OF IT, I STAY WITH " + focus_ + ".";
    }

    ++talkIndex_;
    SetBubble(reply, 5.0f);
    return reply;
}

std::string NpcAI::ApplyPlayerCommand(const std::string& message, float playerX, float playerY) {
    if (!IsAlive()) {
        return name_ + " CANNOT FOLLOW ANY ORDER NOW.";
    }

    const std::string upper = ToUpperCopy(message);

    if (ContainsWord(upper, "FOLLOW") || ContainsWord(upper, "THEO")) {
        commandMode_ = CommandMode::FollowPlayer;
        SetBubble("I WILL FOLLOW YOU.", 3.0f);
        return name_ + " IS FOLLOWING YOU.";
    }

    if (ContainsWord(upper, "STOP") || ContainsWord(upper, "STAY") || ContainsWord(upper, "DUNG") ||
        ContainsWord(upper, "GIU") || ContainsWord(upper, "HERE") || ContainsWord(upper, "DAY")) {
        commandMode_ = CommandMode::HoldPosition;
        holdX_ = CenterX();
        holdY_ = FeetY();
        SetBubble("I WILL HOLD THIS GROUND.", 3.0f);
        return name_ + " IS HOLDING POSITION.";
    }

    if (ContainsWord(upper, "COME") || ContainsWord(upper, "LAI") || ContainsWord(upper, "NEAR") ||
        ContainsWord(upper, "GAN")) {
        commandMode_ = CommandMode::HoldPosition;
        holdX_ = playerX;
        holdY_ = playerY;
        SetBubble("MOVING TO YOU.", 3.0f);
        return name_ + " IS MOVING TO YOUR POSITION.";
    }

    if (ContainsWord(upper, "HOME") || ContainsWord(upper, "RETURN") || ContainsWord(upper, "BACK") ||
        ContainsWord(upper, "VE")) {
        commandMode_ = CommandMode::ReturnHome;
        SetBubble("I AM RETURNING HOME.", 3.0f);
        return name_ + " IS RETURNING HOME.";
    }

    return name_ + " HEARD YOU, BUT THE ORDER IS UNCLEAR.";
}

std::string NpcAI::ApplyAiDirective(const std::string& intent, const std::string& speech, float playerX, float playerY) {
    if (!IsAlive()) {
        return name_ + " CANNOT FOLLOW ANY ORDER NOW.";
    }

    const std::string upperIntent = ToUpperCopy(intent);
    const std::string spoken = speech.empty() ? (name_ + " IS THINKING.") : speech;

    if (ContainsWord(upperIntent, "FOLLOW")) {
        commandMode_ = CommandMode::FollowPlayer;
        SetBubble(spoken, 4.2f);
        return spoken;
    }

    if (ContainsWord(upperIntent, "HOLD") || ContainsWord(upperIntent, "STAY")) {
        commandMode_ = CommandMode::HoldPosition;
        holdX_ = CenterX();
        holdY_ = FeetY();
        SetBubble(spoken, 4.2f);
        return spoken;
    }

    if (ContainsWord(upperIntent, "COME")) {
        commandMode_ = CommandMode::HoldPosition;
        holdX_ = playerX;
        holdY_ = playerY;
        SetBubble(spoken, 4.2f);
        return spoken;
    }

    if (ContainsWord(upperIntent, "RETURN") || ContainsWord(upperIntent, "HOME")) {
        commandMode_ = CommandMode::ReturnHome;
        SetBubble(spoken, 4.2f);
        return spoken;
    }

    if (ContainsWord(upperIntent, "WANDER")) {
        commandMode_ = CommandMode::Wander;
        behaviorTimer_ = 0.0f;
        SetBubble(spoken, 4.2f);
        return spoken;
    }

    SetBubble(spoken, 4.2f);
    return spoken;
}

void NpcAI::SetAiSpeech(const std::string& speech) {
    if (speech.empty()) {
        return;
    }
    SetBubble(speech, 4.2f);
}

void NpcAI::ApplyDamage(int amount) {
    if (!IsAlive()) {
        return;
    }

    health_ = std::max(0, health_ - std::max(amount, 0));
    if (health_ == 0) {
        SetBubble("FALLEN TO THE NIGHT.", 4.0f);
    } else {
        SetBubble("UNDER ATTACK!", 1.4f);
    }
}

void NpcAI::Restore() {
    health_ = 5;
}

void NpcAI::DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const {
    const float screenX = std::floor(x_ - camera.x);
    const float screenY = std::floor(y_ - camera.y);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
    SDL_FRect shadow{screenX + 1.0f, screenY + 12.0f, 10.0f, 2.5f};
    SDL_RenderFillRect(renderer, &shadow);
}

void NpcAI::Draw(SDL_Renderer* renderer, const Camera2D& camera) const {
    const float screenX = std::floor(x_ - camera.x);
    const float screenY = std::floor(y_ - camera.y);
    const float bobY = moving_ ? std::floor(std::sin(animTime_ * 2.1f) * 1.0f) : 0.0f;
    const float renderY = screenY + bobY;
    const float legShift = moving_ ? ((static_cast<int>(animTime_) % 2 == 0) ? 1.0f : -1.0f) : 0.0f;

    SDL_SetRenderDrawColor(renderer, 52, 57, 68, 255);
    SDL_FRect legL{screenX + 2.0f + legShift, renderY + 11.0f, 2.0f, 3.0f};
    SDL_FRect legR{screenX + 8.0f - legShift, renderY + 11.0f, 2.0f, 3.0f};
    SDL_RenderFillRect(renderer, &legL);
    SDL_RenderFillRect(renderer, &legR);

    SDL_SetRenderDrawColor(renderer, 76, 112, 172, 255);
    SDL_FRect body{screenX + 1.0f, renderY + 5.0f, 10.0f, 7.0f};
    SDL_RenderFillRect(renderer, &body);

    SDL_SetRenderDrawColor(renderer, 220, 191, 155, 255);
    SDL_FRect head{screenX + 2.0f, renderY, 8.0f, 6.0f};
    SDL_RenderFillRect(renderer, &head);

    SDL_SetRenderDrawColor(renderer, 140, 87, 60, 255);
    SDL_FRect hair{screenX + 2.0f, renderY, 8.0f, 2.0f};
    SDL_RenderFillRect(renderer, &hair);

    SDL_SetRenderDrawColor(renderer, 20, 20, 22, 255);
    const float eyeX = facingX_ >= 0.0f ? screenX + 7.0f : screenX + 4.0f;
    SDL_FRect eye{eyeX, renderY + 2.0f, 1.0f, 1.0f};
    SDL_RenderFillRect(renderer, &eye);
}

bool NpcAI::IsPlayerNear(float playerX, float playerY) const {
    return DistanceSq(CenterX(), FeetY(), playerX, playerY) <= (34.0f * 34.0f);
}

bool NpcAI::HasBubble() const {
    return !bubbleText_.empty() && bubbleTimer_ > 0.0f;
}

bool NpcAI::IsAlive() const {
    return health_ > 0;
}

const std::string& NpcAI::BubbleText() const {
    return bubbleText_;
}

const std::string& NpcAI::Name() const {
    return name_;
}

const std::string& NpcAI::Role() const {
    return role_;
}

const std::string& NpcAI::Trait() const {
    return trait_;
}

const std::string& NpcAI::Focus() const {
    return focus_;
}

float NpcAI::X() const {
    return x_;
}

float NpcAI::Y() const {
    return y_;
}

float NpcAI::CenterX() const {
    return x_ + (width_ * 0.5f);
}

float NpcAI::CenterY() const {
    return y_ + (height_ * 0.5f);
}

float NpcAI::FeetY() const {
    return y_ + height_;
}

int NpcAI::Health() const {
    return health_;
}

bool NpcAI::MatchesName(const std::string& messageUpper) const {
    const std::string upperName = ToUpperCopy(name_);
    if (messageUpper.find(upperName) != std::string::npos) {
        return true;
    }

    if (name_ == "MARA" && messageUpper.find("CLARE") != std::string::npos) {
        return true;
    }

    return false;
}
