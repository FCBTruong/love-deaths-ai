#include "core/App.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
float ComputeRenderScale(SDL_Window* window, int virtualWidth, int virtualHeight) {
    int windowWidth = virtualWidth;
    int windowHeight = virtualHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    float scale = std::floor(std::min(
        static_cast<float>(windowWidth) / static_cast<float>(virtualWidth),
        static_cast<float>(windowHeight) / static_cast<float>(virtualHeight)
    ));
    return std::max(scale, 1.0f);
}

void DrawDigit(SDL_Renderer* renderer, int digit, float x, float y, float s, SDL_Color color) {
    static const int kMasks[10] = {
        0b111101101101111,
        0b010010010010010,
        0b111001111100111,
        0b111001111001111,
        0b101101111001001,
        0b111100111001111,
        0b111100111101111,
        0b111001001001001,
        0b111101111101111,
        0b111101111001111
    };

    const int mask = kMasks[std::clamp(digit, 0, 9)];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {
            const int bit = 14 - ((row * 3) + col);
            if (((mask >> bit) & 1) == 0) {
                continue;
            }
            SDL_FRect p{x + (static_cast<float>(col) * s), y + (static_cast<float>(row) * s), s, s};
            SDL_RenderFillRect(renderer, &p);
        }
    }
}

void DrawNumber(SDL_Renderer* renderer, int value, float x, float y, float s, SDL_Color color) {
    const int safeValue = std::max(0, value);
    if (safeValue < 10) {
        DrawDigit(renderer, safeValue, x, y, s, color);
        return;
    }

    const int tens = (safeValue / 10) % 10;
    const int ones = safeValue % 10;
    DrawDigit(renderer, tens, x, y, s, color);
    DrawDigit(renderer, ones, x + (4.0f * s), y, s, color);
}
}

App::App()
    : window_(nullptr),
      renderer_(nullptr),
      frameTexture_(nullptr),
    fogTexture_(nullptr),
    map_(8),
      player_(),
    camera_(),
    cameraOffsetX_(0.0f),
    cameraOffsetY_(0.0f),
    draggingCamera_(false),
    lastMouseX_(0),
    lastMouseY_(0),
    interactPressedLast_(false),
    jumpPressedLast_(false),
    attackPressedLast_(false),
    layerUpPressedLast_(false),
    layerDownPressedLast_(false),
    statusText_("WASD move | Space jump | J attack/chop | R up-layer | F down-layer"),
    statusTimer_(0.0f),
    currentLayer_(0),
    appleCount_(0),
    berryCount_(0),
    pearCount_(0),
    meatCount_(0),
    splashes_(),
    harvestFly_(),
    chopDebris_(),
    animals_(),
    splashSpawnTimer_(0.0f),
    shakeTimer_(0.0f),
    shakeDuration_(0.0f),
    shakeMagnitude_(0.0f),
    shakeOffsetX_(0.0f),
    shakeOffsetY_(0.0f) {}

App::~App() {
    Shutdown();
}

bool App::Initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    window_ = SDL_CreateWindow("My2DGame - Pixel Grassland", kWindowWidth, kWindowHeight, SDL_WINDOW_RESIZABLE);
    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        Shutdown();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        Shutdown();
        return false;
    }

    frameTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, kVirtualWidth, kVirtualHeight);
    if (!frameTexture_) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << '\n';
        Shutdown();
        return false;
    }

    SDL_SetTextureScaleMode(frameTexture_, SDL_SCALEMODE_NEAREST);

    fogTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, kVirtualWidth, kVirtualHeight);
    if (!fogTexture_) {
        std::cerr << "SDL_CreateTexture fog failed: " << SDL_GetError() << '\n';
        Shutdown();
        return false;
    }
    SDL_SetTextureScaleMode(fogTexture_, SDL_SCALEMODE_LINEAR);

    player_.SetPosition(0.0f, 0.0f);

    {
        std::mt19937 rng(424242U);
        std::uniform_real_distribution<float> pos(-140.0f, 140.0f);
        std::uniform_int_distribution<int> kind(0, 2);
        std::vector<SDL_FPoint> nearbyWater;

        for (int y = -120; y <= 120; y += 4) {
            for (int x = -120; x <= 120; x += 4) {
                if (map_.IsWaterAt(static_cast<float>(x), static_cast<float>(y))) {
                    nearbyWater.push_back(SDL_FPoint{static_cast<float>(x), static_cast<float>(y)});
                }
            }
        }

        animals_.reserve(12);
        for (int i = 0; i < 12; ++i) {
            for (int attempt = 0; attempt < 24; ++attempt) {
                const float x = pos(rng);
                const float y = pos(rng);
                if (map_.IsWaterAt(x, y) || map_.IsBlockedAt(x, y, 0)) {
                    continue;
                }
                animals_.emplace_back(
                    x,
                    y,
                    static_cast<AnimalKind>(kind(rng)),
                    static_cast<std::uint32_t>(1337 + (i * 977) + attempt)
                );
                break;
            }
        }

        if (!nearbyWater.empty()) {
            std::uniform_int_distribution<std::size_t> waterIndex(0, nearbyWater.size() - 1);
            std::uniform_real_distribution<float> drift(-2.5f, 2.5f);

            for (int i = 0; i < 12; ++i) {
                const SDL_FPoint water = nearbyWater[waterIndex(rng)];
                animals_.emplace_back(
                    water.x + drift(rng),
                    water.y + drift(rng),
                    AnimalKind::Fish,
                    static_cast<std::uint32_t>(90000 + (i * 617))
                );
            }
        }
    }

    camera_.width = static_cast<float>(kVirtualWidth);
    camera_.height = static_cast<float>(kVirtualHeight);
    camera_.SnapToTarget(player_.CenterX(), player_.CenterY());
    SDL_SetWindowTitle(window_, statusText_.c_str());

    return true;
}

int App::Run() {
    bool running = true;
    Uint64 previousTick = SDL_GetTicks();

    while (running) {
        ProcessEvents(running);

        const Uint64 currentTick = SDL_GetTicks();
        float dt = static_cast<float>(currentTick - previousTick) / 1000.0f;
        previousTick = currentTick;
        dt = std::clamp(dt, 0.0f, 0.05f);

        Update(dt);
        Render();
    }

    return 0;
}

void App::Shutdown() {
    if (fogTexture_) {
        SDL_DestroyTexture(fogTexture_);
        fogTexture_ = nullptr;
    }
    if (frameTexture_) {
        SDL_DestroyTexture(frameTexture_);
        frameTexture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

void App::ProcessEvents(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            running = false;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            draggingCamera_ = true;
            lastMouseX_ = event.button.x;
            lastMouseY_ = event.button.y;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
            draggingCamera_ = false;
            cameraOffsetX_ = 0.0f;
            cameraOffsetY_ = 0.0f;
        } else if (event.type == SDL_EVENT_MOUSE_MOTION && draggingCamera_) {
            const int dx = event.motion.x - lastMouseX_;
            const int dy = event.motion.y - lastMouseY_;
            lastMouseX_ = event.motion.x;
            lastMouseY_ = event.motion.y;

            const float scale = ComputeRenderScale(window_, kVirtualWidth, kVirtualHeight);
            cameraOffsetX_ -= static_cast<float>(dx) / scale;
            cameraOffsetY_ -= static_cast<float>(dy) / scale;
        }
    }
}

void App::Update(float dt) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    const bool up = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
    const bool down = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
    const bool left = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    const bool right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
    const bool jumpPressed = keys[SDL_SCANCODE_SPACE];
    const bool attackPressed = keys[SDL_SCANCODE_J];
    const bool layerUpPressed = keys[SDL_SCANCODE_R];
    const bool layerDownPressed = keys[SDL_SCANCODE_F];
    const bool movingInput = up || down || left || right;

    const float prevX = player_.X();
    const float prevY = player_.Y();

    const float moveMultiplier = map_.MovementMultiplierAt(player_.CenterX(), player_.FeetY(), currentLayer_);
    player_.Update(dt, up, down, left, right, moveMultiplier);

    UpdateEffects(dt, movingInput);

    if (map_.IsBlockedAt(player_.CenterX(), player_.FeetY(), currentLayer_)) {
        player_.SetPosition(prevX, prevY);
    }

    for (Animal& animal : animals_) {
        animal.Update(dt, map_);
    }

    if (jumpPressed && !jumpPressedLast_) {
        player_.TriggerJump();
    }
    if (attackPressed && !attackPressedLast_) {
        player_.TriggerAttack();

        auto spawnBurst = [&](float worldX, float worldY, SDL_Color primary, SDL_Color secondary, float baseSize) {
            static std::mt19937 rng(9917U);
            std::uniform_real_distribution<float> dir(-1.0f, 1.0f);
            std::uniform_real_distribution<float> up(11.0f, 19.0f);
            std::uniform_real_distribution<float> life(0.45f, 0.80f);

            for (int i = 0; i < 26; ++i) {
                ChopDebris d{};
                d.worldX = worldX + (dir(rng) * 4.5f);
                d.worldY = worldY - 7.0f + (dir(rng) * 3.5f);
                d.velX = dir(rng) * 24.0f;
                d.velY = -up(rng);
                d.life = 0.0f;
                d.maxLife = life(rng);
                d.red = (i % 3) != 0 ? primary.r : secondary.r;
                d.green = (i % 3) != 0 ? primary.g : secondary.g;
                d.blue = (i % 3) != 0 ? primary.b : secondary.b;
                d.size = baseSize + (std::abs(dir(rng)) * 0.9f);
                chopDebris_.push_back(d);
            }

            for (int i = 0; i < 10; ++i) {
                ChopDebris dust{};
                dust.worldX = worldX + (dir(rng) * 7.0f);
                dust.worldY = worldY - 1.0f + (dir(rng) * 1.2f);
                dust.velX = dir(rng) * 14.0f;
                dust.velY = -std::abs(dir(rng) * 5.0f);
                dust.life = 0.0f;
                dust.maxLife = life(rng) * 0.7f;
                dust.red = secondary.r;
                dust.green = secondary.g;
                dust.blue = secondary.b;
                dust.size = std::max(1.4f, baseSize - 0.4f);
                chopDebris_.push_back(dust);
            }
        };

        auto triggerImpactFeedback = [&](float duration, float magnitude, unsigned int toneHz, unsigned int toneMs) {
            shakeDuration_ = std::max(shakeDuration_, duration);
            shakeTimer_ = std::max(shakeTimer_, duration);
            shakeMagnitude_ = std::max(shakeMagnitude_, magnitude);
#if defined(_WIN32)
            ::Beep(toneHz, toneMs);
#endif
        };

        float hitX = player_.CenterX() + (player_.FacingX() * 8.0f);
        float hitY = player_.FeetY() - 2.0f;

        int nearestAnimalIndex = -1;
        float nearestAnimalDistSq = 999999.0f;
        if (currentLayer_ == 0) {
            for (std::size_t index = 0; index < animals_.size(); ++index) {
                Animal& animal = animals_[index];
            const float dx = animal.CenterX() - hitX;
            const float dy = animal.CenterY() - hitY;
            const float distSq = (dx * dx) + (dy * dy);
            if (distSq > 12.0f * 12.0f) {
                continue;
            }
            if (distSq < nearestAnimalDistSq) {
                nearestAnimalDistSq = distSq;
                    nearestAnimalIndex = static_cast<int>(index);
                }
            }
        }

        if (nearestAnimalIndex >= 0) {
            const Animal hitAnimal = animals_[static_cast<std::size_t>(nearestAnimalIndex)];
            hitX = hitAnimal.CenterX();
            hitY = hitAnimal.CenterY();

            const bool fishKill = hitAnimal.Kind() == AnimalKind::Fish;
            spawnBurst(
                hitX,
                hitY,
                fishKill ? SDL_Color{115, 205, 246, 255} : SDL_Color{214, 96, 92, 255},
                fishKill ? SDL_Color{190, 242, 255, 255} : SDL_Color{255, 215, 188, 255},
                fishKill ? 2.0f : 2.5f
            );
            triggerImpactFeedback(0.14f, fishKill ? 2.8f : 3.9f, fishKill ? 840U : 680U, 26U);
            harvestFly_.push_back(HarvestFly{HarvestResult::Meat, hitX, hitY - 4.0f, 0.0f, 0.6f});
            statusText_ = fishKill ? "You caught a fish" : "You got a drumstick";
            statusTimer_ = 1.6f;

            animals_.erase(animals_.begin() + nearestAnimalIndex);
        } else {

            const InteractionResult result = map_.InteractAt(player_.CenterX(), player_.FeetY(), player_.FacingX(), &hitX, &hitY);
            if (result == InteractionResult::ChoppedTree) {
                statusText_ = "You chopped a tree";
                statusTimer_ = 2.0f;
                spawnBurst(hitX, hitY, SDL_Color{90, 210, 110, 255}, SDL_Color{199, 146, 92, 255}, 2.2f);
                triggerImpactFeedback(0.13f, 3.8f, 930, 22);
            } else if (result == InteractionResult::BrokeRock) {
                statusText_ = "You broke a rock";
                statusTimer_ = 2.0f;
                spawnBurst(hitX, hitY, SDL_Color{154, 155, 163, 255}, SDL_Color{207, 194, 177, 255}, 2.1f);
                triggerImpactFeedback(0.10f, 2.8f, 760, 18);
            } else if (result == InteractionResult::EnteredHouse) {
                statusText_ = "You entered a small house";
                statusTimer_ = 2.0f;
            }

            float harvestX = player_.CenterX() + (player_.FacingX() * 8.0f);
            float harvestY = player_.FeetY() - 2.0f;
            const HarvestResult harvest =
                map_.AttackAt(player_.CenterX(), player_.FeetY(), currentLayer_, player_.FacingX(), &harvestX, &harvestY);
            if (harvest == HarvestResult::Apple) {
                harvestFly_.push_back(HarvestFly{harvest, harvestX, harvestY - 6.0f, 0.0f, 0.55f});
                statusText_ = "Apple dropped";
                statusTimer_ = 1.5f;
                spawnBurst(harvestX, harvestY, SDL_Color{90, 210, 110, 255}, SDL_Color{199, 146, 92, 255}, 2.2f);
                triggerImpactFeedback(0.12f, 3.2f, 980, 20);
            } else if (harvest == HarvestResult::Berry) {
                harvestFly_.push_back(HarvestFly{harvest, harvestX, harvestY - 6.0f, 0.0f, 0.55f});
                statusText_ = "Berries dropped";
                statusTimer_ = 1.5f;
                spawnBurst(harvestX, harvestY, SDL_Color{90, 210, 110, 255}, SDL_Color{199, 146, 92, 255}, 2.2f);
                triggerImpactFeedback(0.12f, 3.2f, 980, 20);
            } else if (harvest == HarvestResult::Pear) {
                harvestFly_.push_back(HarvestFly{harvest, harvestX, harvestY - 6.0f, 0.0f, 0.55f});
                statusText_ = "Pear dropped";
                statusTimer_ = 1.5f;
                spawnBurst(harvestX, harvestY, SDL_Color{90, 210, 110, 255}, SDL_Color{199, 146, 92, 255}, 2.2f);
                triggerImpactFeedback(0.12f, 3.2f, 980, 20);
            }
        }
    }
    jumpPressedLast_ = jumpPressed;
    attackPressedLast_ = attackPressed;

    int newLayer = currentLayer_;
    if (layerUpPressed && !layerUpPressedLast_ && map_.CanMoveLayer(player_.CenterX(), player_.FeetY(), currentLayer_, true, newLayer)) {
        currentLayer_ = newLayer;
        statusText_ = "Moved up to mountain layer";
        statusTimer_ = 1.5f;
    }
    if (layerDownPressed && !layerDownPressedLast_ &&
        map_.CanMoveLayer(player_.CenterX(), player_.FeetY(), currentLayer_, false, newLayer)) {
        currentLayer_ = newLayer;
        statusText_ = "Moved down to ground layer";
        statusTimer_ = 1.5f;
    }
    layerUpPressedLast_ = layerUpPressed;
    layerDownPressedLast_ = layerDownPressed;

    if (statusTimer_ > 0.0f) {
        statusTimer_ = std::max(0.0f, statusTimer_ - dt);
        if (statusTimer_ == 0.0f) {
            statusText_ = "WASD move | Space jump | J attack/chop | R up-layer | F down-layer";
        }
    }
    SDL_SetWindowTitle(window_, statusText_.c_str());

    camera_.SnapToTarget(player_.CenterX() + cameraOffsetX_, player_.CenterY() + cameraOffsetY_);
}

void App::Render() {
    SDL_SetRenderTarget(renderer_, frameTexture_);

    SDL_SetRenderDrawColor(renderer_, 148, 207, 255, 255);
    SDL_RenderClear(renderer_);

    Camera2D renderCamera = camera_;
    renderCamera.x += shakeOffsetX_;
    renderCamera.y += shakeOffsetY_;
    renderCamera.x = std::floor(renderCamera.x);
    renderCamera.y = std::floor(renderCamera.y);

    map_.DrawGround(renderer_, renderCamera, currentLayer_);
    map_.DrawShadows(renderer_, renderCamera, currentLayer_);
    map_.DrawProps(renderer_, renderCamera, player_.FeetY(), true, currentLayer_);
    DrawWorldEffects(renderCamera);

    if (currentLayer_ == 0) {
        for (const Animal& animal : animals_) {
            if (animal.FeetY() <= player_.FeetY()) {
                animal.DrawShadow(renderer_, renderCamera);
                animal.Draw(renderer_, renderCamera);
            }
        }
    }

    player_.DrawShadow(renderer_, renderCamera);
    player_.Draw(renderer_, renderCamera);

    if (currentLayer_ == 0) {
        for (const Animal& animal : animals_) {
            if (animal.FeetY() > player_.FeetY()) {
                animal.DrawShadow(renderer_, renderCamera);
                animal.Draw(renderer_, renderCamera);
            }
        }
    }

    map_.DrawProps(renderer_, renderCamera, player_.FeetY(), false, currentLayer_);

    SDL_SetRenderTarget(renderer_, nullptr);

    SDL_SetRenderDrawColor(renderer_, 14, 14, 18, 255);
    SDL_RenderClear(renderer_);

    int windowWidth = kWindowWidth;
    int windowHeight = kWindowHeight;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);

    const float scale = ComputeRenderScale(window_, kVirtualWidth, kVirtualHeight);

    const float drawWidth = static_cast<float>(kVirtualWidth) * scale;
    const float drawHeight = static_cast<float>(kVirtualHeight) * scale;

    SDL_FRect destination{
        (static_cast<float>(windowWidth) - drawWidth) * 0.5f,
        (static_cast<float>(windowHeight) - drawHeight) * 0.5f,
        drawWidth,
        drawHeight
    };

    SDL_RenderTexture(renderer_, frameTexture_, nullptr, &destination);
    DrawFogOfWar(renderCamera, destination);
    DrawHarvestFlyEffects(renderCamera, destination);
    DrawUI(destination);
    SDL_RenderPresent(renderer_);
}

void App::UpdateEffects(float dt, bool moving) {
    splashSpawnTimer_ = std::max(0.0f, splashSpawnTimer_ - dt);

    if (moving && map_.IsWaterAt(player_.CenterX(), player_.FeetY())) {
        if (splashSpawnTimer_ <= 0.0f) {
            static std::mt19937 rng(1337U);
            std::uniform_real_distribution<float> dir(-1.0f, 1.0f);
            std::uniform_real_distribution<float> size(1.0f, 2.2f);

            WaterSplash s{};
            s.worldX = player_.CenterX() + (dir(rng) * 2.0f);
            s.worldY = player_.FeetY() - 1.5f;
            s.velX = dir(rng) * 9.0f;
            s.velY = -8.5f - (std::abs(dir(rng)) * 4.0f);
            s.life = 0.0f;
            s.maxLife = 0.35f;
            s.size = size(rng);
            splashes_.push_back(s);

            splashSpawnTimer_ = 0.045f;
        }
    }

    for (WaterSplash& s : splashes_) {
        s.life += dt;
        s.worldX += s.velX * dt;
        s.worldY += s.velY * dt;
        s.velY += 18.0f * dt;
    }

    splashes_.erase(std::remove_if(splashes_.begin(), splashes_.end(), [](const WaterSplash& s) { return s.life >= s.maxLife; }),
        splashes_.end());

    for (HarvestFly& h : harvestFly_) {
        h.elapsed += dt;
        if (h.elapsed >= h.duration) {
            if (h.type == HarvestResult::Apple) {
                ++appleCount_;
            } else if (h.type == HarvestResult::Berry) {
                ++berryCount_;
            } else if (h.type == HarvestResult::Pear) {
                ++pearCount_;
            } else if (h.type == HarvestResult::Meat) {
                ++meatCount_;
            }
        }
    }

    harvestFly_.erase(
        std::remove_if(harvestFly_.begin(), harvestFly_.end(), [](const HarvestFly& h) { return h.elapsed >= h.duration; }),
        harvestFly_.end());

    for (ChopDebris& d : chopDebris_) {
        d.life += dt;
        d.worldX += d.velX * dt;
        d.worldY += d.velY * dt;
        d.velY += 28.0f * dt;
    }

    chopDebris_.erase(
        std::remove_if(chopDebris_.begin(), chopDebris_.end(), [](const ChopDebris& d) { return d.life >= d.maxLife; }),
        chopDebris_.end());

    if (shakeTimer_ > 0.0f) {
        shakeTimer_ = std::max(0.0f, shakeTimer_ - dt);
        const float t = (shakeDuration_ > 0.0f) ? (shakeTimer_ / shakeDuration_) : 0.0f;
        const float strength = shakeMagnitude_ * t;

        static std::mt19937 rng(5150U);
        std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);
        shakeOffsetX_ = jitter(rng) * strength;
        shakeOffsetY_ = jitter(rng) * strength;
    } else {
        shakeDuration_ = 0.0f;
        shakeMagnitude_ = 0.0f;
        shakeOffsetX_ = 0.0f;
        shakeOffsetY_ = 0.0f;
    }
}

void App::DrawWorldEffects(const Camera2D& camera) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    for (const WaterSplash& s : splashes_) {
        const float t = std::clamp(s.life / s.maxLife, 0.0f, 1.0f);
        const std::uint8_t a = static_cast<std::uint8_t>((1.0f - t) * 200.0f);
        SDL_SetRenderDrawColor(renderer_, 204, 234, 255, a);

        const float screenX = std::floor(s.worldX - camera.x);
        const float screenY = std::floor(s.worldY - camera.y);
        SDL_FRect p{screenX, screenY, s.size, s.size};
        SDL_RenderFillRect(renderer_, &p);
    }

    for (const ChopDebris& d : chopDebris_) {
        const float t = std::clamp(d.life / d.maxLife, 0.0f, 1.0f);
        const std::uint8_t a = static_cast<std::uint8_t>((1.0f - t) * 240.0f);

        SDL_SetRenderDrawColor(renderer_, d.red, d.green, d.blue, a);

        const float screenX = std::floor(d.worldX - camera.x);
        const float screenY = std::floor(d.worldY - camera.y);
        SDL_FRect bit{screenX, screenY, d.size, d.size};
        SDL_RenderFillRect(renderer_, &bit);
    }
}

void App::DrawHarvestFlyEffects(const Camera2D& camera, const SDL_FRect& destination) {
    if (harvestFly_.empty()) {
        return;
    }

    const float scale = std::max(1.0f, std::floor(destination.w / static_cast<float>(kVirtualWidth)));
    const float panelX = destination.x + (6.0f * scale);
    const float panelY = destination.y + (6.0f * scale);
    const float slotY = panelY + (3.0f * scale);

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    for (const HarvestFly& h : harvestFly_) {
        const float t = std::clamp(h.elapsed / h.duration, 0.0f, 1.0f);
        const float e = t * t * (3.0f - (2.0f * t));

        float targetX = panelX + (5.0f * scale);
        if (h.type == HarvestResult::Berry) {
            targetX = panelX + (36.0f * scale);
        } else if (h.type == HarvestResult::Pear) {
            targetX = panelX + (67.0f * scale);
        } else if (h.type == HarvestResult::Meat) {
            targetX = panelX + (98.0f * scale);
        }
        const float targetY = slotY + (7.0f * scale);

        const float worldSX = destination.x + ((h.worldX - camera.x) * scale);
        const float worldSY = destination.y + ((h.worldY - camera.y) * scale) - (std::sin(t * 3.14159265f) * 8.0f * scale);

        const float drawX = worldSX + ((targetX - worldSX) * e);
        const float drawY = worldSY + ((targetY - worldSY) * e);

        if (h.type == HarvestResult::Apple) {
            SDL_SetRenderDrawColor(renderer_, 222, 66, 65, 235);
        } else if (h.type == HarvestResult::Berry) {
            SDL_SetRenderDrawColor(renderer_, 166, 89, 227, 235);
        } else if (h.type == HarvestResult::Meat) {
            SDL_SetRenderDrawColor(renderer_, 219, 121, 93, 235);
        } else {
            SDL_SetRenderDrawColor(renderer_, 238, 209, 80, 235);
        }

        SDL_FRect orb{drawX, drawY, 3.0f * scale, 3.0f * scale};
        SDL_RenderFillRect(renderer_, &orb);
    }
}

void App::DrawFogOfWar(const Camera2D& camera, const SDL_FRect& destination) {
    void* pixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(fogTexture_, nullptr, &pixels, &pitch)) {
        return;
    }

    const float px = player_.CenterX() - camera.x;
    const float py = player_.CenterY() - camera.y;

    const float innerRadius = 42.0f;
    const float outerRadius = 108.0f;
    const float invFadeRange = 1.0f / (outerRadius - innerRadius);
    const float maxAlpha = 220.0f;

    std::uint8_t* row = static_cast<std::uint8_t*>(pixels);
    const float cx = static_cast<float>(kVirtualWidth) * 0.5f;
    const float cy = static_cast<float>(kVirtualHeight) * 0.5f;
    const float vignetteMax = std::sqrt((cx * cx) + (cy * cy));

    for (int y = 0; y < kVirtualHeight; ++y) {
        std::uint32_t* out = reinterpret_cast<std::uint32_t*>(row + (y * pitch));
        for (int x = 0; x < kVirtualWidth; ++x) {
            const float dx = static_cast<float>(x) - px;
            const float dy = static_cast<float>(y) - py;
            const float dist = std::sqrt((dx * dx) + (dy * dy));

            float alphaF = 0.0f;
            if (dist >= outerRadius) {
                alphaF = maxAlpha;
            } else if (dist > innerRadius) {
                const float t = (dist - innerRadius) * invFadeRange;
                const float eased = t * t * (3.0f - (2.0f * t));
                alphaF = eased * maxAlpha;
            }

            const float vx = static_cast<float>(x) - cx;
            const float vy = static_cast<float>(y) - cy;
            const float vignette = std::sqrt((vx * vx) + (vy * vy)) / vignetteMax;
            alphaF += std::clamp(vignette - 0.35f, 0.0f, 0.65f) * 80.0f;

            const std::uint8_t alpha = static_cast<std::uint8_t>(std::clamp(alphaF, 0.0f, 255.0f));
            const std::uint32_t rgba = (static_cast<std::uint32_t>(12) << 24U) |
                (static_cast<std::uint32_t>(16) << 16U) |
                (static_cast<std::uint32_t>(24) << 8U) |
                static_cast<std::uint32_t>(alpha);
            out[x] = rgba;
        }
    }

    SDL_UnlockTexture(fogTexture_);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(renderer_, fogTexture_, nullptr, &destination);
}

void App::DrawUI(const SDL_FRect& destination) {
    const float scale = std::max(1.0f, std::floor(destination.w / static_cast<float>(kVirtualWidth)));

    const float panelX = destination.x + (6.0f * scale);
    const float panelY = destination.y + (6.0f * scale);
    const float panelW = 127.0f * scale;
    const float panelH = 24.0f * scale;

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 8, 13, 18, 210);
    SDL_FRect panel{panelX, panelY, panelW, panelH};
    SDL_RenderFillRect(renderer_, &panel);

    SDL_SetRenderDrawColor(renderer_, 164, 178, 190, 230);
    SDL_FRect borderTop{panelX, panelY, panelW, 1.0f * scale};
    SDL_FRect borderLeft{panelX, panelY, 1.0f * scale, panelH};
    SDL_RenderFillRect(renderer_, &borderTop);
    SDL_RenderFillRect(renderer_, &borderLeft);

    const float slotW = 29.0f * scale;
    const float slotH = 18.0f * scale;
    const float slotY = panelY + (3.0f * scale);

    const auto drawResourceSlot = [&](int index, SDL_Color iconColor, int count, bool meatIcon) {
        const float slotX = panelX + (3.0f * scale) + (index * 31.0f * scale);
        SDL_SetRenderDrawColor(renderer_, 23, 31, 42, 235);
        SDL_FRect slot{slotX, slotY, slotW, slotH};
        SDL_RenderFillRect(renderer_, &slot);

        SDL_SetRenderDrawColor(renderer_, iconColor.r, iconColor.g, iconColor.b, 255);
        if (meatIcon) {
            SDL_FRect meatCore{slotX + (3.0f * scale), slotY + (5.0f * scale), 5.0f * scale, 4.0f * scale};
            SDL_RenderFillRect(renderer_, &meatCore);
            SDL_SetRenderDrawColor(renderer_, 243, 224, 205, 255);
            SDL_FRect bone{slotX + (8.0f * scale), slotY + (6.0f * scale), 2.0f * scale, 2.0f * scale};
            SDL_RenderFillRect(renderer_, &bone);
        } else {
            SDL_FRect fruit{slotX + (3.0f * scale), slotY + (5.0f * scale), 4.0f * scale, 4.0f * scale};
            SDL_RenderFillRect(renderer_, &fruit);
        }

        DrawNumber(renderer_, count, slotX + (10.0f * scale), slotY + (4.0f * scale), 1.0f * scale, SDL_Color{242, 245, 248, 255});
    };

    drawResourceSlot(0, SDL_Color{222, 66, 65, 255}, appleCount_, false);
    drawResourceSlot(1, SDL_Color{166, 89, 227, 255}, berryCount_, false);
    drawResourceSlot(2, SDL_Color{238, 209, 80, 255}, pearCount_, false);
    drawResourceSlot(3, SDL_Color{219, 121, 93, 255}, meatCount_, true);
}
