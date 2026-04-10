#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <random>
#include <string>

#include "core/Camera2D.h"
#include "game/Pawn.h"

class TileMap;

class NpcAI : public Pawn {
public:
    NpcAI(
        std::string name,
        std::string role,
        std::string trait,
        std::string focus,
        float x,
        float y,
        std::uint32_t seed
    );

    void Update(float dt, const TileMap& map, float playerX, float playerY);
    std::string TriggerConversation(int appleCount, int berryCount, int pearCount, int meatCount);
    std::string RespondToMessage(const std::string& message, int appleCount, int berryCount, int pearCount, int meatCount);
    std::string ApplyPlayerCommand(const std::string& message, float playerX, float playerY);
    std::string ApplyAiDirective(const std::string& intent, const std::string& speech, float playerX, float playerY);
    void SetAiSpeech(const std::string& speech);
    void ApplyDamage(int amount);
    void Restore();
    void DrawShadow(SDL_Renderer* renderer, const Camera2D& camera) const;
    void Draw(SDL_Renderer* renderer, const Camera2D& camera) const;

    bool IsPlayerNear(float playerX, float playerY) const;
    bool HasBubble() const;
    bool IsAlive() const;
    const std::string& BubbleText() const;
    const std::string& Name() const;
    const std::string& Role() const;
    const std::string& Trait() const;
    const std::string& Focus() const;
    int Health() const;
    const char* StateLabel() const;
    bool MatchesName(const std::string& messageUpper) const;

private:
    void OnBeginPlay() override;
    void OnEndPlay() override;

    enum class CommandMode {
        Wander,
        FollowPlayer,
        HoldPosition,
        ReturnHome
    };

    void PickNewBehavior();
    void SetBubble(std::string text, float duration);
    std::string BuildThoughtText() const;

    std::string name_;
    std::string role_;
    std::string trait_;
    std::string focus_;

    float homeX_;
    float homeY_;
    float dirX_;
    float dirY_;
    float behaviorTimer_;
    float animTime_;

    float thoughtTimer_;
    int thoughtIndex_;
    int talkIndex_;
    float bubbleTimer_;
    std::string bubbleText_;
    int health_;
    CommandMode commandMode_;
    float holdX_;
    float holdY_;

    std::mt19937 rng_;
};
