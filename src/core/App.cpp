#include "core/App.h"

#include "ai/AiDebugLog.h"
#include "ai/NpcChatSystem.h"
#include "core/HudRenderer.h"
#include "core/PixelFont.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <vector>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using pixel_font::DrawNumber;
using pixel_font::DrawBitmapText;
using pixel_font::MeasureText;
using pixel_font::WrapText;

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

std::string FormatSignedInt(int value) {
    if (value >= 0) {
        return std::to_string(value);
    }
    return "-" + std::to_string(-value);
}

AiRuntimeManager::Config BuildDefaultAiConfig() {
    namespace fs = std::filesystem;

    std::vector<fs::path> roots;
    if (const char* basePathRaw = SDL_GetBasePath(); basePathRaw != nullptr) {
        const fs::path exeDir(basePathRaw);
        roots.push_back(exeDir);
        roots.push_back(exeDir.parent_path());
        roots.push_back(exeDir.parent_path().parent_path());
    }
    roots.push_back(fs::current_path());

    for (const fs::path& root : roots) {
        const fs::path llamaServerPath = root / "third_party" / "llama-bin" / "llama-server.exe";
        const fs::path modelPath = root / "models" / "npc-brain.gguf";
        if (!fs::exists(llamaServerPath) || !fs::exists(modelPath)) {
            continue;
        }

        return AiRuntimeManager::Config{
            AiRuntimeManager::Backend::LlamaServer,
            fs::absolute(llamaServerPath).string(),
            std::string(),
            fs::absolute(modelPath).string(),
            "http://127.0.0.1:8080",
            8080,
            true};
    }

    return AiRuntimeManager::Config{
        AiRuntimeManager::Backend::Ollama,
        std::string(),
        "gemma3:4b",
        std::string(),
        "http://127.0.0.1:11434",
        11434,
        true};
}
}

App::App()
    : window_(nullptr),
      renderer_(nullptr),
      frameTexture_(nullptr),
    fogTexture_(nullptr),
    audioStream_(nullptr),
    map_(8),
      player_(),
    camera_(),
    cameraOffsetX_(0.0f),
    cameraOffsetY_(0.0f),
    draggingCamera_(false),
    recenteringCamera_(false),
    lastMouseX_(0),
    lastMouseY_(0),
    interactPressedLast_(false),
    jumpPressedLast_(false),
    attackPressedLast_(false),
    talkPressedLast_(false),
    slot1PressedLast_(false),
    slot2PressedLast_(false),
    slot3PressedLast_(false),
    slot4PressedLast_(false),
    slot5PressedLast_(false),
    slot6PressedLast_(false),
    enterPressedLast_(false),
    suppressEnterOpen_(false),
    undoPressedLast_(false),
    fenceVerticalPlacement_(false),
    layerUpPressedLast_(false),
    layerDownPressedLast_(false),
    chatMode_(false),
    statusText_("WASD move | Shift slow walk | 1 weapon | 2 fence | 3 soil | 4 seeds | 5 rod | 6 bow | J use | E rotate/talk | Ctrl+Z undo"),
    chatInput_(),
    chatReply_(),
    playerChatText_(),
    playerChatTimer_(0.0f),
    statusTimer_(0.0f),
    currentLayer_(0),
    appleCount_(0),
    berryCount_(0),
    pearCount_(0),
    meatCount_(0),
    splashes_(),
    harvestFly_(),
    chopDebris_(),
    bloodParticles_(),
    arrows_(),
    fences_(),
    soilPlots_(),
    animals_(),
    miko_("MIKO", -10.0f, 10.0f),
    npcs_(),
    hostiles_(),
    heldItem_(HeldItem::Weapon),
    fishingCast_{false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    pendingAiFollowup_(),
    pendingAiRequest_(),
    nearbyNpcIndex_(-1),
    chatNpcIndex_(-1),
    splashSpawnTimer_(0.0f),
    hostileSpawnTimer_(3.2f),
    fogAnimTime_(0.0f),
    shakeTimer_(0.0f),
    shakeDuration_(0.0f),
    shakeMagnitude_(0.0f),
    shakeOffsetX_(0.0f),
    shakeOffsetY_(0.0f),
    mikoSoundCooldown_(0.0f),
    aiRuntime_(),
    aiBackendReady_(false),
    aiBackendStatus_("AI offline") {}

App::~App() {
    Shutdown();
}

bool App::InitializeAudio() {
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq = 22050;

    audioStream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!audioStream_) {
        std::cerr << "SDL_OpenAudioDeviceStream failed: " << SDL_GetError() << '\n';
        return false;
    }

    if (!SDL_ResumeAudioStreamDevice(audioStream_)) {
        std::cerr << "SDL_ResumeAudioStreamDevice failed: " << SDL_GetError() << '\n';
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
        return false;
    }

    return true;
}

void App::ShutdownAudio() {
    if (audioStream_) {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
    }
}

void App::PlayMikoSound(bool happy) {
    if (!audioStream_ || mikoSoundCooldown_ > 0.0f) {
        return;
    }

    constexpr int sampleRate = 22050;
    constexpr float barkDuration = 0.22f;
    const int sampleCount = static_cast<int>(sampleRate * barkDuration);
    std::vector<float> samples(static_cast<std::size_t>(sampleCount), 0.0f);

    for (int i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float phase = t / barkDuration;
        const float envelope = std::exp(-phase * 6.0f) * std::sin(std::clamp(phase * 3.14159265f, 0.0f, 3.14159265f));
        const float freqA = happy ? 680.0f : 520.0f;
        const float freqB = happy ? 920.0f : 760.0f;
        const float chirp = std::sin(t * freqA * 6.2831853f) * 0.55f;
        const float yip = std::sin(t * (freqB + (phase * 180.0f)) * 6.2831853f) * 0.35f;
        const float rough = std::sin(t * 180.0f * 6.2831853f) * 0.12f;
        samples[static_cast<std::size_t>(i)] = (chirp + yip + rough) * envelope * 0.22f;
    }

    SDL_PutAudioStreamData(audioStream_, samples.data(), static_cast<int>(samples.size() * sizeof(float)));
    mikoSoundCooldown_ = happy ? 0.34f : 0.45f;
}

bool App::Initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
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

    InitializeAudio();

    player_.SetPosition(0.0f, 0.0f);
    player_.BeginPlay();
    miko_.SetPosition(-12.0f, 10.0f);
    miko_.BeginPlay();

    {
        std::mt19937 rng(424242U);
        std::uniform_real_distribution<float> pos(-140.0f, 140.0f);
        std::uniform_int_distribution<int> kind(0, 5);
        std::vector<SDL_FPoint> nearbyWater;

        for (int y = -120; y <= 120; y += 4) {
            for (int x = -120; x <= 120; x += 4) {
                if (map_.IsWaterAt(static_cast<float>(x), static_cast<float>(y))) {
                    nearbyWater.push_back(SDL_FPoint{static_cast<float>(x), static_cast<float>(y)});
                }
            }
        }

        animals_.reserve(30);
        for (int i = 0; i < 18; ++i) {
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
    for (Animal& animal : animals_) {
        animal.BeginPlay();
    }

    npcs_.reserve(3);
    const auto placeNpc = [this](
                              const char* name,
                              const char* role,
                              const char* trait,
                              const char* focus,
                              float startX,
                              float startY,
                              std::uint32_t seed) {
        float x = startX;
        float y = startY;
        for (int attempt = 0; attempt < 18; ++attempt) {
            if (!map_.IsWaterAt(x, y) && !map_.IsBlockedAt(x, y, 0)) {
                npcs_.emplace_back(name, role, trait, focus, x, y, seed);
                return;
            }
            x += 10.0f;
            y += (attempt % 2 == 0) ? 6.0f : -6.0f;
        }
        npcs_.emplace_back(name, role, trait, focus, startX, startY, seed);
    };

    placeNpc("LENA", "SCOUT", "PATIENCE", "SAFE PATHS", 18.0f, 6.0f, 8801U);
    placeNpc("MARA", "HERBALIST", "CARE", "QUIET GARDENS", -18.0f, 8.0f, 8802U);
    placeNpc("ORIN", "TINKER", "CURIOSITY", "BETTER MACHINES", 10.0f, -14.0f, 8803U);
    for (NpcAI& npc : npcs_) {
        npc.BeginPlay();
    }

    hostiles_.reserve(7);
    hostiles_.emplace_back(92.0f, 44.0f, HostileKind::Zombie, 501U);
    hostiles_.emplace_back(-88.0f, -40.0f, HostileKind::Zombie, 502U);
    hostiles_.emplace_back(104.0f, -28.0f, HostileKind::Marauder, 503U);
    hostiles_.emplace_back(-96.0f, 52.0f, HostileKind::Marauder, 504U);
    hostiles_.emplace_back(118.0f, 8.0f, HostileKind::Ghoul, 505U);
    hostiles_.emplace_back(-118.0f, 14.0f, HostileKind::Ghoul, 506U);
    hostiles_.emplace_back(0.0f, 92.0f, HostileKind::Wraith, 507U);
    for (HostileAI& hostile : hostiles_) {
        hostile.BeginPlay();
    }

    camera_.width = static_cast<float>(kVirtualWidth);
    camera_.height = static_cast<float>(kVirtualHeight);
    camera_.SnapToTarget(player_.CenterX(), player_.CenterY());

    aiRuntime_.SetConfig(BuildDefaultAiConfig());
    aiBackendReady_ = aiRuntime_.Start();
    if (aiBackendReady_) {
        aiBackendStatus_ = aiRuntime_.GetConfig().backend == AiRuntimeManager::Backend::LlamaServer ? "LOCAL AI READY" : "OLLAMA AI READY";
        statusText_ = aiBackendStatus_;
        statusTimer_ = 2.6f;
    } else if (aiRuntime_.GetConfig().backend == AiRuntimeManager::Backend::LlamaServer) {
        aiBackendStatus_ = "AI missing: third_party/llama-server.exe or models/npc-brain.gguf";
    } else {
        aiBackendStatus_ = "AI missing: install Ollama or bundle llama-server";
    }

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
    ShutdownAudio();
    player_.EndPlay();
    miko_.EndPlay();
    for (Animal& animal : animals_) {
        animal.EndPlay();
    }
    for (NpcAI& npc : npcs_) {
        npc.EndPlay();
    }
    for (HostileAI& hostile : hostiles_) {
        hostile.EndPlay();
    }

    aiRuntime_.Stop();

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
        } else if (chatMode_ && event.type == SDL_EVENT_TEXT_INPUT) {
            if (chatInput_.size() < 42U) {
                chatInput_ += event.text.text;
            }
        } else if (chatMode_ && event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_BACKSPACE && !chatInput_.empty()) {
                chatInput_.pop_back();
            } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                if (chatNpcIndex_ >= 0 && !chatInput_.empty()) {
                    chatReply_ = npcs_[static_cast<std::size_t>(chatNpcIndex_)].RespondToMessage(
                        chatInput_, appleCount_, berryCount_, pearCount_, meatCount_);
                    statusText_ = chatReply_;
                    statusTimer_ = 3.5f;
                    chatMode_ = false;
                    chatNpcIndex_ = -1;
                    chatInput_.clear();
                    chatReply_.clear();
                    suppressEnterOpen_ = true;
                    SDL_StopTextInput(window_);
                } else if (chatNpcIndex_ < 0 && !chatInput_.empty()) {
                    playerChatText_ = chatInput_;
                    playerChatTimer_ = 5.0f;
                    std::optional<npc_chat_system::AiFollowupRequest> aiFollowup;
                    const npc_chat_system::CommandResult commandResult = npc_chat_system::HandlePlayerChatCommand(
                        chatInput_, nearbyNpcIndex_, npcs_, miko_, player_, hostiles_, aiBackendReady_, aiRuntime_.GetConfig(), aiFollowup);
                    if (aiFollowup.has_value() && !pendingAiRequest_.has_value()) {
                        pendingAiRequest_ = aiFollowup;
                        pendingAiFollowup_ = std::async(std::launch::async, npc_chat_system::ExecuteAiFollowup, *pendingAiRequest_);
                    } else if (aiFollowup.has_value()) {
                        statusText_ = "AI is still thinking";
                        statusTimer_ = 1.0f;
                    }
                    if (commandResult.playMikoSound) {
                        PlayMikoSound(true);
                    }
                    chatReply_ = commandResult.chatReply;
                    statusText_ = commandResult.statusText;
                    statusTimer_ = commandResult.statusTimer;
                    chatInput_.clear();
                    chatMode_ = false;
                    chatNpcIndex_ = -1;
                    chatReply_.clear();
                    suppressEnterOpen_ = true;
                    SDL_StopTextInput(window_);
                } else if (chatNpcIndex_ < 0 && chatInput_.empty()) {
                    chatMode_ = false;
                    chatNpcIndex_ = -1;
                    chatReply_.clear();
                    suppressEnterOpen_ = true;
                    SDL_StopTextInput(window_);
                }
            } else if (event.key.key == SDLK_ESCAPE) {
                chatMode_ = false;
                chatNpcIndex_ = -1;
                chatInput_.clear();
                chatReply_.clear();
                suppressEnterOpen_ = true;
                SDL_StopTextInput(window_);
            }
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            if (chatMode_) {
                continue;
            }
            draggingCamera_ = true;
            recenteringCamera_ = false;
            lastMouseX_ = event.button.x;
            lastMouseY_ = event.button.y;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
            draggingCamera_ = false;
            cameraOffsetX_ = 0.0f;
            cameraOffsetY_ = 0.0f;
            recenteringCamera_ = true;
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
    mikoSoundCooldown_ = std::max(0.0f, mikoSoundCooldown_ - dt);

    const bool* keys = SDL_GetKeyboardState(nullptr);

    const bool up = !chatMode_ && (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]);
    const bool down = !chatMode_ && (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]);
    const bool left = !chatMode_ && (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]);
    const bool right = !chatMode_ && (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]);
    const bool slowWalk = !chatMode_ && (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]);
    const bool jumpPressed = !chatMode_ && keys[SDL_SCANCODE_SPACE];
    const bool attackPressed = !chatMode_ && keys[SDL_SCANCODE_J];
    const bool talkPressed = !chatMode_ && keys[SDL_SCANCODE_E];
    const bool slot1Pressed = !chatMode_ && keys[SDL_SCANCODE_1];
    const bool slot2Pressed = !chatMode_ && keys[SDL_SCANCODE_2];
    const bool slot3Pressed = !chatMode_ && keys[SDL_SCANCODE_3];
    const bool slot4Pressed = !chatMode_ && keys[SDL_SCANCODE_4];
    const bool slot5Pressed = !chatMode_ && keys[SDL_SCANCODE_5];
    const bool slot6Pressed = !chatMode_ && keys[SDL_SCANCODE_6];
    const bool enterPressed = !chatMode_ && (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]);
    const bool undoPressed =
        !chatMode_ &&
        keys[SDL_SCANCODE_Z] &&
        (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]);
    const bool layerUpPressed = !chatMode_ && keys[SDL_SCANCODE_R];
    const bool layerDownPressed = !chatMode_ && keys[SDL_SCANCODE_F];
    const bool movingInput = up || down || left || right;

    const float prevX = player_.X();
    const float prevY = player_.Y();

    const float terrainMoveMultiplier = map_.MovementMultiplierAt(player_.CenterX(), player_.FeetY(), currentLayer_);
    const float moveMultiplier = terrainMoveMultiplier * (slowWalk ? 0.42f : 1.0f);
    player_.Update(dt, up, down, left, right, moveMultiplier, slowWalk);
    player_.SetSwimming(currentLayer_ == 0 && map_.IsWaterAt(player_.CenterX(), player_.FeetY()));

    UpdateEffects(dt, movingInput);

    auto pointHitsFence = [&](float x, float y) {
        for (const Fence& fence : fences_) {
            if (fence.health <= 0) {
                continue;
            }
            if (x >= fence.x && x <= (fence.x + fence.width) && y >= fence.y && y <= (fence.y + fence.height)) {
                return true;
            }
        }
        return false;
    };

    auto pointHitsSoil = [&](float x, float y) {
        for (const SoilPlot& plot : soilPlots_) {
            if (x >= plot.x && x <= (plot.x + plot.width) && y >= plot.y && y <= (plot.y + plot.height)) {
                return true;
            }
        }
        return false;
    };

    auto computeFencePlacement = [&]() {
        Fence preview{};
        preview.width = fenceVerticalPlacement_ ? 8.0f : 16.0f;
        preview.height = fenceVerticalPlacement_ ? 16.0f : 8.0f;
        const float facingY = player_.FacingY();
        if (std::fabs(facingY) > 0.01f) {
            preview.x = player_.CenterX() - (preview.width * 0.5f);
            preview.y = player_.FeetY() + (facingY * 12.0f) - preview.height;
        } else {
            preview.x = player_.CenterX() + (player_.FacingX() * 12.0f) - (preview.width * 0.5f);
            preview.y = player_.FeetY() - preview.height;
        }
        preview.health = 8;
        return preview;
    };

    auto canPlaceFence = [&](const Fence& preview) {
        const float probeX = preview.x + (preview.width * 0.5f);
        const float probeY = preview.y + preview.height;
        return !map_.IsWaterAt(probeX, probeY) &&
               !map_.HasBlockingObstacleInRect(preview.x, preview.y, preview.width, preview.height, 0) &&
               !pointHitsFence(probeX, probeY) &&
               !pointHitsSoil(probeX, probeY);
    };

    auto computeSoilPlacement = [&]() {
        SoilPlot preview{};
        preview.width = 16.0f;
        preview.height = 16.0f;
        const float facingY = player_.FacingY();
        if (std::fabs(facingY) > 0.01f) {
            preview.x = player_.CenterX() - (preview.width * 0.5f);
            preview.y = player_.FeetY() + (facingY * 14.0f) - preview.height;
        } else {
            preview.x = player_.CenterX() + (player_.FacingX() * 14.0f) - (preview.width * 0.5f);
            preview.y = player_.FeetY() - preview.height;
        }
        preview.x = std::floor(preview.x / 8.0f) * 8.0f;
        preview.y = std::floor(preview.y / 8.0f) * 8.0f;
        return preview;
    };

    auto canPlaceSoil = [&](const SoilPlot& preview) {
        const float probeX = preview.x + (preview.width * 0.5f);
        const float probeY = preview.y + preview.height;
        if (map_.IsWaterAt(probeX, probeY) ||
            map_.HasBlockingObstacleInRect(preview.x, preview.y, preview.width, preview.height, 0) ||
            pointHitsFence(probeX, probeY)) {
            return false;
        }
        for (const SoilPlot& plot : soilPlots_) {
            const bool overlap =
                preview.x < (plot.x + plot.width) &&
                (preview.x + preview.width) > plot.x &&
                preview.y < (plot.y + plot.height) &&
                (preview.y + preview.height) > plot.y;
            if (overlap) {
                return false;
            }
        }
        return true;
    };

    if (map_.IsBlockedAt(player_.CenterX(), player_.FeetY(), currentLayer_) || pointHitsFence(player_.CenterX(), player_.FeetY())) {
        player_.SetPosition(prevX, prevY);
    }

    if (slot1Pressed && !slot1PressedLast_) {
        heldItem_ = HeldItem::Weapon;
        statusText_ = "Weapon equipped";
        statusTimer_ = 0.8f;
    }
    if (slot2Pressed && !slot2PressedLast_) {
        heldItem_ = HeldItem::Fence;
        statusText_ = "Fence equipped";
        statusTimer_ = 0.8f;
    }
    if (slot3Pressed && !slot3PressedLast_) {
        heldItem_ = HeldItem::Soil;
        statusText_ = "Soil plot equipped";
        statusTimer_ = 0.8f;
    }
    if (slot4Pressed && !slot4PressedLast_) {
        heldItem_ = HeldItem::Seed;
        statusText_ = "Seeds equipped";
        statusTimer_ = 0.8f;
    }
    if (slot5Pressed && !slot5PressedLast_) {
        heldItem_ = HeldItem::Rod;
        statusText_ = "Fishing rod equipped";
        statusTimer_ = 0.8f;
    }
    if (slot6Pressed && !slot6PressedLast_) {
        heldItem_ = HeldItem::Bow;
        statusText_ = "Bow equipped";
        statusTimer_ = 0.8f;
    }
    if (!enterPressed) {
        suppressEnterOpen_ = false;
    }

    if (enterPressed && !enterPressedLast_ && !chatMode_ && !suppressEnterOpen_) {
        chatMode_ = true;
        chatNpcIndex_ = -1;
        chatInput_.clear();
        chatReply_.clear();
        statusText_ = "Type a message";
        statusTimer_ = 0.8f;
        SDL_StartTextInput(window_);
    }
    if (talkPressed && !talkPressedLast_ && heldItem_ == HeldItem::Fence && !chatMode_) {
        fenceVerticalPlacement_ = !fenceVerticalPlacement_;
        statusText_ = fenceVerticalPlacement_ ? "Fence rotated vertical" : "Fence rotated horizontal";
        statusTimer_ = 0.8f;
    }
    if (undoPressed && !undoPressedLast_) {
        if (heldItem_ == HeldItem::Soil && !soilPlots_.empty()) {
            soilPlots_.pop_back();
            statusText_ = "Soil plot removed";
        } else if (!fences_.empty()) {
            fences_.pop_back();
            statusText_ = "Fence removed";
        } else {
            statusText_ = heldItem_ == HeldItem::Soil ? "No soil plot to undo" : "No fence to undo";
        }
        statusTimer_ = 0.8f;
    }

    for (Animal& animal : animals_) {
        animal.Update(dt, map_);
    }
    miko_.Update(dt, map_, player_.CenterX(), player_.FeetY(), movingInput);

    auto spawnArrowBlood = [&](float worldX, float worldY) {
        static std::mt19937 rng(6401U);
        std::uniform_real_distribution<float> dir(-1.0f, 1.0f);
        std::uniform_real_distribution<float> life(0.16f, 0.28f);
        for (int i = 0; i < 3; ++i) {
            BloodParticle p{};
            p.worldX = worldX + (dir(rng) * 0.8f);
            p.worldY = worldY - 2.0f + (dir(rng) * 0.8f);
            p.velX = dir(rng) * 5.0f;
            p.velY = -1.0f - (std::abs(dir(rng)) * 1.5f);
            p.life = 0.0f;
            p.maxLife = life(rng);
            p.red = 78;
            p.green = 18;
            p.blue = 20;
            p.size = 0.8f;
            bloodParticles_.push_back(p);
        }
    };

    for (ArrowShot& arrow : arrows_) {
        arrow.life += dt;
        arrow.worldX += arrow.velX * dt;
        arrow.worldY += arrow.velY * dt;
    }

    for (ArrowShot& arrow : arrows_) {
        if (arrow.life >= arrow.maxLife) {
            continue;
        }

        if (map_.IsWaterAt(arrow.worldX, arrow.worldY) || map_.IsBlockedAt(arrow.worldX, arrow.worldY, currentLayer_)) {
            arrow.life = arrow.maxLife;
            continue;
        }

        bool consumed = false;
        for (HostileAI& hostile : hostiles_) {
            if (!hostile.IsAlive()) {
                continue;
            }
            const float dx = hostile.CenterX() - arrow.worldX;
            const float dy = hostile.CenterY() - arrow.worldY;
            if ((dx * dx) + (dy * dy) > (7.0f * 7.0f)) {
                continue;
            }
            hostile.ApplyDamage(3);
            spawnArrowBlood(hostile.CenterX(), hostile.CenterY());
            statusText_ = hostile.IsAlive() ? "Arrow hit a hostile" : "Arrow felled a hostile";
            statusTimer_ = 1.2f;
            arrow.life = arrow.maxLife;
            consumed = true;
            break;
        }
        if (consumed) {
            continue;
        }

        for (std::size_t index = 0; index < animals_.size(); ++index) {
            Animal& animal = animals_[index];
            const float dx = animal.CenterX() - arrow.worldX;
            const float dy = animal.CenterY() - arrow.worldY;
            if ((dx * dx) + (dy * dy) > (7.0f * 7.0f)) {
                continue;
            }
            harvestFly_.push_back(HarvestFly{HarvestResult::Meat, animal.CenterX(), animal.CenterY() - 4.0f, 0.0f, 0.6f});
            statusText_ = animal.Kind() == AnimalKind::Fish ? "Arrow caught a fish" : "Arrow brought down prey";
            statusTimer_ = 1.3f;
            arrow.life = arrow.maxLife;
            animals_.erase(animals_.begin() + static_cast<std::ptrdiff_t>(index));
            consumed = true;
            break;
        }
        if (consumed) {
            continue;
        }

        for (Fence& fence : fences_) {
            if (fence.health <= 0) {
                continue;
            }
            if (arrow.worldX >= fence.x && arrow.worldX <= (fence.x + fence.width) &&
                arrow.worldY >= fence.y && arrow.worldY <= (fence.y + fence.height)) {
                fence.health -= 1;
                statusText_ = "Arrow struck a fence";
                statusTimer_ = 0.8f;
                arrow.life = arrow.maxLife;
                break;
            }
        }
    }

    arrows_.erase(
        std::remove_if(arrows_.begin(), arrows_.end(), [](const ArrowShot& arrow) { return arrow.life >= arrow.maxLife; }),
        arrows_.end());

    playerChatTimer_ = std::max(0.0f, playerChatTimer_ - dt);
    if (playerChatTimer_ == 0.0f) {
        playerChatText_.clear();
    }

    if (pendingAiRequest_.has_value() && pendingAiFollowup_.valid() &&
        pendingAiFollowup_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const npc_chat_system::AiFollowupResult result = pendingAiFollowup_.get();
        if (result.ok) {
            if (result.targetMiko) {
                if (result.intent == "follow_player" || result.intent == "sit_down" || result.intent == "hold_position" ||
                    result.intent == "come_to_player" || result.intent == "return_home" || result.intent == "idle") {
                    chatReply_ = miko_.ApplyAiDirective(result.intent, result.speech, player_.CenterX(), player_.FeetY());
                    ai_debug_log::Write("GAME_APPLY_AI target=MIKO intent=\"" + result.intent + "\" speech=\"" + result.speech + "\"");
                } else {
                    chatReply_ = result.speech;
                    ai_debug_log::Write("AI_IGNORED target=MIKO reason=invalid_intent intent=\"" + result.intent + "\"");
                }
            } else if (result.targetNpcIndex >= 0 && result.targetNpcIndex < static_cast<int>(npcs_.size())) {
                NpcAI& npc = npcs_[static_cast<std::size_t>(result.targetNpcIndex)];
                if (result.intent == "follow_player" || result.intent == "hold_position" || result.intent == "come_to_player" ||
                    result.intent == "return_home" || result.intent == "wander" || result.intent == "talk") {
                    chatReply_ = npc.ApplyAiDirective(result.intent, result.speech, player_.CenterX(), player_.FeetY());
                    ai_debug_log::Write("GAME_APPLY_AI target_npc=\"" + npc.Name() + "\" intent=\"" + result.intent + "\" speech=\"" + result.speech + "\"");
                } else if (!result.speech.empty()) {
                    npc.SetAiSpeech(result.speech);
                    chatReply_ = result.speech;
                    ai_debug_log::Write("AI_IGNORED target_npc=\"" + npc.Name() + "\" reason=invalid_intent intent=\"" + result.intent + "\"");
                }
            }

            if (!chatReply_.empty()) {
                statusText_ = "CMD OK | " + chatReply_;
                statusTimer_ = 3.0f;
            }
        } else {
            statusText_ = "AI request failed";
            statusTimer_ = 1.6f;
        }
        pendingAiRequest_.reset();
    }

    for (SoilPlot& plot : soilPlots_) {
        if (!plot.planted) {
            continue;
        }
        plot.growTimer = std::min(plot.growDuration, plot.growTimer + dt);
    }

    if (fishingCast_.active) {
        fishingCast_.timer += dt;
        if (fishingCast_.timer >= fishingCast_.totalTime) {
            fishingCast_.active = false;
            harvestFly_.push_back(HarvestFly{HarvestResult::Meat, fishingCast_.worldX, fishingCast_.worldY - 4.0f, 0.0f, 0.6f});
            statusText_ = "You reeled in a fish";
            statusTimer_ = 1.4f;
        } else if (fishingCast_.timer >= fishingCast_.biteTime && statusTimer_ <= 0.0f) {
            statusText_ = "A fish is biting";
            statusTimer_ = 0.35f;
        }
    }

    nearbyNpcIndex_ = -1;
    float nearestNpcDistSq = 999999.0f;
    for (std::size_t index = 0; index < npcs_.size(); ++index) {
        NpcAI& npc = npcs_[index];
        npc.Update(dt, map_, player_.CenterX(), player_.FeetY());
        if (!npc.IsPlayerNear(player_.CenterX(), player_.FeetY())) {
            continue;
        }

        const float dx = npc.CenterX() - player_.CenterX();
        const float dy = npc.FeetY() - player_.FeetY();
        const float distSq = (dx * dx) + (dy * dy);
        if (distSq < nearestNpcDistSq) {
            nearestNpcDistSq = distSq;
            nearbyNpcIndex_ = static_cast<int>(index);
        }
    }

    auto spawnBloodBurst = [&](float worldX, float worldY, bool heavy) {
        static std::mt19937 rng(8401U);
        std::uniform_real_distribution<float> dir(-1.0f, 1.0f);
        std::uniform_real_distribution<float> life(0.18f, 0.36f);
        const int count = heavy ? 6 : 3;

        for (int i = 0; i < count; ++i) {
            BloodParticle p{};
            p.worldX = worldX + (dir(rng) * 1.2f);
            p.worldY = worldY - 4.0f + (dir(rng) * 1.0f);
            p.velX = dir(rng) * (heavy ? 8.0f : 4.0f);
            p.velY = -1.5f - (std::abs(dir(rng)) * (heavy ? 4.0f : 2.0f));
            p.life = 0.0f;
            p.maxLife = life(rng);
            p.red = static_cast<std::uint8_t>(78 + (i % 2) * 7);
            p.green = 18;
            p.blue = 20;
            p.size = heavy ? 1.0f : 0.8f;
            bloodParticles_.push_back(p);
        }
    };

    {
        int liveHostiles = 0;
        for (const HostileAI& hostile : hostiles_) {
            if (hostile.IsAlive()) {
                ++liveHostiles;
            }
        }

        if (hostileSpawnTimer_ <= 0.0f && liveHostiles < 12) {
            static std::mt19937 rng(44021U);
            std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
            std::uniform_real_distribution<float> radius(76.0f, 118.0f);
            std::uniform_int_distribution<int> kindRoll(0, 9);

            for (int attempt = 0; attempt < 18; ++attempt) {
                const float a = angle(rng);
                const float r = radius(rng);
                const float spawnX = player_.CenterX() + (std::cos(a) * r);
                const float spawnY = player_.CenterY() + (std::sin(a) * r);
                if (map_.IsWaterAt(spawnX, spawnY) || map_.IsBlockedAt(spawnX, spawnY, 0)) {
                    continue;
                }

                const int roll = kindRoll(rng);
                const HostileKind kind =
                    roll < 4 ? HostileKind::Zombie :
                    roll < 7 ? HostileKind::Marauder :
                    roll < 9 ? HostileKind::Ghoul : HostileKind::Wraith;
                hostiles_.emplace_back(spawnX, spawnY, kind, static_cast<std::uint32_t>(9000U + SDL_GetTicks() + attempt));
                hostiles_.back().BeginPlay();
                statusText_ = kind == HostileKind::Zombie ? "The dead rose again"
                             : kind == HostileKind::Marauder ? "Dark raiders entered the valley"
                             : kind == HostileKind::Ghoul ? "A ghoul crawled into the light"
                                                          : "A wraith slipped from the dark";
                statusTimer_ = 1.2f;
                hostileSpawnTimer_ = 2.8f;
                break;
            }
        }
    }

    for (HostileAI& hostile : hostiles_) {
        const float hostilePrevX = hostile.X();
        const float hostilePrevY = hostile.Y();
        float bestX = player_.CenterX();
        float bestY = player_.FeetY();
        bool hasTarget = player_.IsAlive();
        float bestDistSq = hasTarget ? ((hostile.CenterX() - bestX) * (hostile.CenterX() - bestX)) +
                                           ((hostile.FeetY() - bestY) * (hostile.FeetY() - bestY))
                                     : 999999.0f;

        for (NpcAI& npc : npcs_) {
            if (!npc.IsAlive()) {
                continue;
            }
            const float dx = hostile.CenterX() - npc.CenterX();
            const float dy = hostile.FeetY() - npc.FeetY();
            const float distSq = (dx * dx) + (dy * dy);
            if (!hasTarget || distSq < bestDistSq) {
                hasTarget = true;
                bestDistSq = distSq;
                bestX = npc.CenterX();
                bestY = npc.FeetY();
            }
        }

        hostile.Update(dt, map_, bestX, bestY, hasTarget);

        if (pointHitsFence(hostile.CenterX(), hostile.FeetY())) {
            hostile.SetPosition(hostilePrevX, hostilePrevY);
        }

        int nearestFenceIndex = -1;
        float nearestFenceDistSq = 999999.0f;
        for (std::size_t index = 0; index < fences_.size(); ++index) {
            const Fence& fence = fences_[index];
            if (fence.health <= 0) {
                continue;
            }
            const float fenceCX = fence.x + (fence.width * 0.5f);
            const float fenceCY = fence.y + fence.height;
            const float dx = hostile.CenterX() - fenceCX;
            const float dy = hostile.FeetY() - fenceCY;
            const float distSq = (dx * dx) + (dy * dy);
            if (distSq < nearestFenceDistSq) {
                nearestFenceDistSq = distSq;
                nearestFenceIndex = static_cast<int>(index);
            }
        }

        if (nearestFenceIndex >= 0 && hostile.IsReadyToAttack(fences_[static_cast<std::size_t>(nearestFenceIndex)].x + 5.0f,
                fences_[static_cast<std::size_t>(nearestFenceIndex)].y + 8.0f)) {
            Fence& fence = fences_[static_cast<std::size_t>(nearestFenceIndex)];
            fence.health -= hostile.Kind() == HostileKind::Ghoul ? 2 : 1;
            hostile.ResetAttackCooldown();
            statusText_ = fence.health > 0 ? "A fence is under attack" : "A fence was destroyed";
            statusTimer_ = 1.0f;
            continue;
        }

        if (!hasTarget) {
            continue;
        }

        if (player_.IsAlive() && hostile.IsReadyToAttack(player_.CenterX(), player_.FeetY())) {
            const int hostileDamage =
                hostile.Kind() == HostileKind::Zombie ? 1 :
                hostile.Kind() == HostileKind::Marauder ? 2 :
                hostile.Kind() == HostileKind::Ghoul ? 3 : 1;
            player_.ApplyDamage(hostileDamage);
            spawnBloodBurst(player_.CenterX(), player_.CenterY(), hostile.Kind() == HostileKind::Ghoul);
            hostile.ResetAttackCooldown();

            if (hostile.Kind() == HostileKind::Marauder) {
                if (meatCount_ > 0) {
                    --meatCount_;
                    statusText_ = "Marauder stole your meat";
                } else if (pearCount_ > 0) {
                    --pearCount_;
                    statusText_ = "Marauder stole your pears";
                } else if (berryCount_ > 0) {
                    --berryCount_;
                    statusText_ = "Marauder stole your berries";
                } else if (appleCount_ > 0) {
                    --appleCount_;
                    statusText_ = "Marauder stole your apples";
                } else {
                    statusText_ = "Marauder cut you";
                }
            } else if (hostile.Kind() == HostileKind::Ghoul) {
                statusText_ = "Ghoul tore into you";
            } else if (hostile.Kind() == HostileKind::Wraith) {
                statusText_ = "Wraith drained your warmth";
            } else {
                statusText_ = "Zombie bit you";
            }
            statusTimer_ = 1.8f;
        }

        for (NpcAI& npc : npcs_) {
            if (!npc.IsAlive()) {
                continue;
            }
            if (!hostile.IsReadyToAttack(npc.CenterX(), npc.FeetY())) {
                continue;
            }

            const int hostileDamage =
                hostile.Kind() == HostileKind::Zombie ? 1 :
                hostile.Kind() == HostileKind::Marauder ? 2 :
                hostile.Kind() == HostileKind::Ghoul ? 3 : 1;
            npc.ApplyDamage(hostileDamage);
            spawnBloodBurst(npc.CenterX(), npc.CenterY(), hostile.Kind() == HostileKind::Ghoul);
            hostile.ResetAttackCooldown();
            statusText_ =
                hostile.Kind() == HostileKind::Zombie ? "Zombie attacked a villager" :
                hostile.Kind() == HostileKind::Marauder ? "Marauder raided a villager" :
                hostile.Kind() == HostileKind::Ghoul ? "Ghoul butchered a villager" :
                                                        "Wraith haunted a villager";
            statusTimer_ = 1.6f;
            break;
        }
    }
    fences_.erase(std::remove_if(fences_.begin(), fences_.end(), [](const Fence& fence) { return fence.health <= 0; }), fences_.end());

    if (chatMode_ && chatNpcIndex_ >= 0 &&
        !npcs_[static_cast<std::size_t>(chatNpcIndex_)].IsPlayerNear(player_.CenterX(), player_.FeetY())) {
        chatMode_ = false;
        chatNpcIndex_ = -1;
        chatInput_.clear();
        chatReply_.clear();
        SDL_StopTextInput(window_);
    }

    if (heldItem_ == HeldItem::Weapon && nearbyNpcIndex_ >= 0 && !talkPressedLast_ && statusTimer_ <= 0.0f) {
        statusText_ = "Press E to talk";
        statusTimer_ = 0.2f;
    }

    if (talkPressed && !talkPressedLast_ && heldItem_ == HeldItem::Weapon && nearbyNpcIndex_ >= 0) {
        chatMode_ = true;
        chatNpcIndex_ = nearbyNpcIndex_;
        chatInput_.clear();
        const std::string spoken = npcs_[static_cast<std::size_t>(nearbyNpcIndex_)].TriggerConversation(
            appleCount_, berryCount_, pearCount_, meatCount_);
        chatReply_ = spoken;
        statusText_ = spoken;
        statusTimer_ = 3.5f;
        SDL_StartTextInput(window_);
    } else if (talkPressed && !talkPressedLast_ && chatMode_) {
        chatMode_ = false;
        chatNpcIndex_ = -1;
        chatInput_.clear();
        chatReply_.clear();
        SDL_StopTextInput(window_);
    }

    if (jumpPressed && !jumpPressedLast_) {
        player_.TriggerJump();
    }
    if (attackPressed && !attackPressedLast_) {
        if (heldItem_ == HeldItem::Fence) {
            if (currentLayer_ != 0) {
                statusText_ = "Build fences on the ground";
                statusTimer_ = 1.0f;
            } else {
                const Fence preview = computeFencePlacement();
                if (canPlaceFence(preview)) {
                    fences_.push_back(preview);
                    statusText_ = "Fence placed";
                } else {
                    statusText_ = "Cannot place fence here";
                }
                statusTimer_ = 1.0f;
            }
        } else if (heldItem_ == HeldItem::Soil) {
            if (currentLayer_ != 0) {
                statusText_ = "Place soil on the ground";
            } else {
                const SoilPlot preview = computeSoilPlacement();
                if (canPlaceSoil(preview)) {
                    soilPlots_.push_back(SoilPlot{preview.x, preview.y, preview.width, preview.height, false, HarvestResult::None, 0.0f, 0.0f});
                    statusText_ = "Soil plot placed";
                } else {
                    statusText_ = "Cannot place soil here";
                }
            }
            statusTimer_ = 1.0f;
        } else if (heldItem_ == HeldItem::Seed) {
            const SoilPlot target = computeSoilPlacement();
            SoilPlot* targetPlot = nullptr;
            for (SoilPlot& plot : soilPlots_) {
                const bool overlap =
                    target.x < (plot.x + plot.width) &&
                    (target.x + target.width) > plot.x &&
                    target.y < (plot.y + plot.height) &&
                    (target.y + target.height) > plot.y;
                if (overlap) {
                    targetPlot = &plot;
                    break;
                }
            }

            if (targetPlot == nullptr) {
                statusText_ = "No soil plot here";
            } else if (targetPlot->planted) {
                statusText_ = "This soil is already planted";
            } else {
                HarvestResult seedType = HarvestResult::None;
                if (appleCount_ > 0) {
                    --appleCount_;
                    seedType = HarvestResult::Apple;
                } else if (berryCount_ > 0) {
                    --berryCount_;
                    seedType = HarvestResult::Berry;
                } else if (pearCount_ > 0) {
                    --pearCount_;
                    seedType = HarvestResult::Pear;
                }

                if (seedType == HarvestResult::None) {
                    statusText_ = "You need fruit to plant";
                } else {
                    targetPlot->planted = true;
                    targetPlot->seedType = seedType;
                    targetPlot->growTimer = 0.0f;
                    targetPlot->growDuration =
                        seedType == HarvestResult::Apple ? 18.0f :
                        seedType == HarvestResult::Berry ? 14.0f : 16.0f;
                    statusText_ =
                        seedType == HarvestResult::Apple ? "Apple seed planted" :
                        seedType == HarvestResult::Berry ? "Berry seed planted" :
                                                           "Pear seed planted";
                }
            }
            statusTimer_ = 1.0f;
        } else if (heldItem_ == HeldItem::Rod) {
            if (fishingCast_.active) {
                statusText_ = "You are already fishing";
            } else {
                const float castX = player_.CenterX() + (player_.FacingX() * 18.0f);
                const float castY = player_.FeetY() - 3.0f;
                bool foundWater = false;
                float waterX = castX;
                float waterY = castY;

                for (int step = 1; step <= 4; ++step) {
                    const float sampleX = player_.CenterX() + (player_.FacingX() * (10.0f + (step * 6.0f)));
                    const float sampleY = player_.FeetY() - 2.0f;
                    if (map_.IsWaterAt(sampleX, sampleY)) {
                        foundWater = true;
                        waterX = sampleX;
                        waterY = sampleY;
                        break;
                    }
                }

                if (!foundWater && std::fabs(player_.FacingY()) > 0.01f) {
                    for (int step = 1; step <= 4; ++step) {
                        const float sampleX = player_.CenterX();
                        const float sampleY = player_.FeetY() + (player_.FacingY() * (6.0f + (step * 6.0f)));
                        if (map_.IsWaterAt(sampleX, sampleY)) {
                            foundWater = true;
                            waterX = sampleX;
                            waterY = sampleY;
                            break;
                        }
                    }
                }

                if (!foundWater) {
                    statusText_ = "Cast toward the water";
                } else {
                    fishingCast_.active = true;
                    fishingCast_.worldX = waterX;
                    fishingCast_.worldY = waterY;
                    fishingCast_.timer = 0.0f;
                    fishingCast_.biteTime = 1.0f;
                    fishingCast_.totalTime = 1.8f;
                    statusText_ = "Line cast";
                }
            }
            statusTimer_ = 1.0f;
        } else if (heldItem_ == HeldItem::Bow) {
            ArrowShot arrow{};
            const float facingY = player_.FacingY();
            arrow.worldX = player_.CenterX() + (player_.FacingX() * 7.0f);
            arrow.worldY = player_.CenterY() + (facingY * 7.0f);
            arrow.velX = player_.FacingX() * 118.0f;
            arrow.velY = facingY * 118.0f;
            if (std::fabs(facingY) < 0.01f) {
                arrow.velY = 0.0f;
            }
            arrow.life = 0.0f;
            arrow.maxLife = 0.55f;
            arrows_.push_back(arrow);
            statusText_ = "Arrow fired";
            statusTimer_ = 0.5f;
            player_.TriggerAttack();
        } else {
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
            int nearestHostileIndex = -1;
            float nearestHostileDistSq = 999999.0f;
            int nearestPlayerFenceIndex = -1;
            float nearestPlayerFenceDistSq = 999999.0f;
            if (currentLayer_ == 0) {
                for (std::size_t index = 0; index < hostiles_.size(); ++index) {
                    HostileAI& hostile = hostiles_[index];
                    if (!hostile.IsAlive()) {
                        continue;
                    }
                    const float dx = hostile.CenterX() - hitX;
                    const float dy = hostile.CenterY() - hitY;
                    const float distSq = (dx * dx) + (dy * dy);
                    if (distSq > 14.0f * 14.0f) {
                        continue;
                    }
                    if (distSq < nearestHostileDistSq) {
                        nearestHostileDistSq = distSq;
                        nearestHostileIndex = static_cast<int>(index);
                    }
                }

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

                for (std::size_t index = 0; index < fences_.size(); ++index) {
                    Fence& fence = fences_[index];
                    if (fence.health <= 0) {
                        continue;
                    }
                    const float fenceCX = fence.x + (fence.width * 0.5f);
                    const float fenceCY = fence.y + (fence.height * 0.5f);
                    const float dx = fenceCX - hitX;
                    const float dy = fenceCY - hitY;
                    const float distSq = (dx * dx) + (dy * dy);
                    if (distSq > 14.0f * 14.0f) {
                        continue;
                    }
                    if (distSq < nearestPlayerFenceDistSq) {
                        nearestPlayerFenceDistSq = distSq;
                        nearestPlayerFenceIndex = static_cast<int>(index);
                    }
                }
            }

            if (nearestHostileIndex >= 0) {
                HostileAI& hostile = hostiles_[static_cast<std::size_t>(nearestHostileIndex)];
                hitX = hostile.CenterX();
                hitY = hostile.CenterY();
                const HostileKind hostileKind = hostile.Kind();
                hostile.ApplyDamage(2);
                spawnBurst(
                    hitX,
                    hitY,
                    hostileKind == HostileKind::Zombie ? SDL_Color{118, 168, 104, 255}
                                                       : (hostileKind == HostileKind::Marauder ? SDL_Color{205, 132, 72, 255}
                                                                                               : (hostileKind == HostileKind::Ghoul
                                                                                                       ? SDL_Color{128, 111, 92, 255}
                                                                                                       : SDL_Color{142, 96, 206, 255})),
                    hostileKind == HostileKind::Zombie ? SDL_Color{196, 228, 186, 255}
                                                       : (hostileKind == HostileKind::Marauder ? SDL_Color{245, 210, 164, 255}
                                                                                               : (hostileKind == HostileKind::Ghoul
                                                                                                       ? SDL_Color{201, 185, 161, 255}
                                                                                                       : SDL_Color{204, 188, 255, 255})),
                    2.3f
                );
                triggerImpactFeedback(
                    0.12f,
                    hostileKind == HostileKind::Ghoul ? 4.4f : 3.7f,
                    hostileKind == HostileKind::Zombie ? 420U :
                    hostileKind == HostileKind::Marauder ? 740U :
                    hostileKind == HostileKind::Ghoul ? 360U : 880U,
                    24U
                );
                statusText_ = hostile.IsAlive()
                                  ? (hostileKind == HostileKind::Zombie ? "You struck a zombie"
                                     : hostileKind == HostileKind::Marauder ? "You struck a marauder"
                                     : hostileKind == HostileKind::Ghoul ? "You struck a ghoul"
                                                                         : "You struck a wraith")
                                  : (hostileKind == HostileKind::Zombie ? "Zombie dropped"
                                     : hostileKind == HostileKind::Marauder ? "Marauder dropped"
                                     : hostileKind == HostileKind::Ghoul ? "Ghoul collapsed"
                                                                         : "Wraith dispersed");
                statusTimer_ = 1.5f;
            } else if (nearestAnimalIndex >= 0) {
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
            } else if (nearestPlayerFenceIndex >= 0) {
                Fence& fence = fences_[static_cast<std::size_t>(nearestPlayerFenceIndex)];
                hitX = fence.x + (fence.width * 0.5f);
                hitY = fence.y + (fence.height * 0.5f);

                fence.health -= 2;
                spawnBurst(hitX, hitY, SDL_Color{129, 92, 58, 255}, SDL_Color{193, 156, 112, 255}, 2.0f);
                triggerImpactFeedback(0.10f, 2.7f, 690U, 18U);
                statusText_ = fence.health > 0 ? "You damaged a fence" : "You destroyed a fence";
                statusTimer_ = 1.1f;
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
    }
    jumpPressedLast_ = jumpPressed;
    attackPressedLast_ = attackPressed;
    talkPressedLast_ = talkPressed;
    slot1PressedLast_ = slot1Pressed;
    slot2PressedLast_ = slot2Pressed;
    slot3PressedLast_ = slot3Pressed;
    slot4PressedLast_ = slot4Pressed;
    slot5PressedLast_ = slot5Pressed;
    slot6PressedLast_ = slot6Pressed;
    enterPressedLast_ = enterPressed;
    undoPressedLast_ = undoPressed;

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
            statusText_ = "WASD move | Shift slow walk | 1 weapon | 2 fence | 3 soil | 4 seeds | 5 rod | 6 bow | J use | E rotate/talk | Ctrl+Z undo";
        }
    }
    SDL_SetWindowTitle(window_, statusText_.c_str());

    player_.SetToolVisual(
        heldItem_ == HeldItem::Weapon ? Player::ToolVisual::Blade :
        heldItem_ == HeldItem::Fence ? Player::ToolVisual::Fence :
        heldItem_ == HeldItem::Soil ? Player::ToolVisual::Soil :
        heldItem_ == HeldItem::Seed ? Player::ToolVisual::Seed :
        heldItem_ == HeldItem::Rod ? Player::ToolVisual::Rod :
                                     Player::ToolVisual::Bow
    );

    const float cameraTargetX = player_.CenterX() + cameraOffsetX_;
    const float cameraTargetY = player_.CenterY() + cameraOffsetY_;
    if (draggingCamera_) {
        camera_.SnapToTarget(cameraTargetX, cameraTargetY);
    } else if (recenteringCamera_) {
        camera_.LerpToTarget(cameraTargetX, cameraTargetY, 14.0f, dt);
        const float goalX = cameraTargetX - (camera_.width * 0.5f);
        const float goalY = cameraTargetY - (camera_.height * 0.5f);
        if (std::fabs(camera_.x - goalX) < 0.1f && std::fabs(camera_.y - goalY) < 0.1f) {
            camera_.SnapToTarget(cameraTargetX, cameraTargetY);
            recenteringCamera_ = false;
        }
    } else {
        camera_.SnapToTarget(cameraTargetX, cameraTargetY);
    }
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

    std::vector<SDL_FRect> clearedGround;
    clearedGround.reserve(soilPlots_.size());
    for (const SoilPlot& plot : soilPlots_) {
        clearedGround.push_back(SDL_FRect{plot.x, plot.y, plot.width, plot.height});
    }

    map_.DrawGround(renderer_, renderCamera, currentLayer_);
    DrawSoilPlots(renderCamera, true, 1000000.0f);
    map_.DrawShadows(renderer_, renderCamera, currentLayer_, &clearedGround);
    DrawFences(renderCamera, true, player_.FeetY());
    map_.DrawProps(renderer_, renderCamera, player_.FeetY(), true, currentLayer_, &clearedGround);
    DrawWorldEffects(renderCamera);

    if (currentLayer_ == 0) {
        for (const Animal& animal : animals_) {
            if (animal.FeetY() <= player_.FeetY()) {
                animal.DrawShadow(renderer_, renderCamera);
                animal.Draw(renderer_, renderCamera);
            }
        }

        if (miko_.FeetY() <= player_.FeetY()) {
            miko_.DrawShadow(renderer_, renderCamera);
            miko_.Draw(renderer_, renderCamera);
        }

        for (const NpcAI& npc : npcs_) {
            if (npc.FeetY() <= player_.FeetY()) {
                npc.DrawShadow(renderer_, renderCamera);
                npc.Draw(renderer_, renderCamera);
            }
        }

        for (const HostileAI& hostile : hostiles_) {
            if (hostile.IsAlive() && hostile.FeetY() <= player_.FeetY()) {
                hostile.DrawShadow(renderer_, renderCamera);
                hostile.Draw(renderer_, renderCamera);
            }
        }
    }

    player_.DrawShadow(renderer_, renderCamera);
    DrawPlacementPreview(renderCamera);
    player_.Draw(renderer_, renderCamera);

    if (currentLayer_ == 0) {
        if (miko_.FeetY() > player_.FeetY()) {
            miko_.DrawShadow(renderer_, renderCamera);
            miko_.Draw(renderer_, renderCamera);
        }

        for (const Animal& animal : animals_) {
            if (animal.FeetY() > player_.FeetY()) {
                animal.DrawShadow(renderer_, renderCamera);
                animal.Draw(renderer_, renderCamera);
            }
        }

        for (const NpcAI& npc : npcs_) {
            if (npc.FeetY() > player_.FeetY()) {
                npc.DrawShadow(renderer_, renderCamera);
                npc.Draw(renderer_, renderCamera);
            }
        }

        for (const HostileAI& hostile : hostiles_) {
            if (hostile.IsAlive() && hostile.FeetY() > player_.FeetY()) {
                hostile.DrawShadow(renderer_, renderCamera);
                hostile.Draw(renderer_, renderCamera);
            }
        }
    }

    DrawFences(renderCamera, false, player_.FeetY());
    map_.DrawProps(renderer_, renderCamera, player_.FeetY(), false, currentLayer_, &clearedGround);

    SDL_SetRenderTarget(renderer_, nullptr);

    SDL_SetRenderDrawColor(renderer_, 14, 14, 18, 255);
    SDL_RenderClear(renderer_);

    int windowWidth = kWindowWidth;
    int windowHeight = kWindowHeight;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);

    const float scale = std::max(
        static_cast<float>(windowWidth) / static_cast<float>(kVirtualWidth),
        static_cast<float>(windowHeight) / static_cast<float>(kVirtualHeight)
    );

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
    hud_renderer::DrawNpcLocators(renderer_, renderCamera, destination, npcs_, nearbyNpcIndex_, kVirtualWidth);
    hud_renderer::DrawNpcBubbles(renderer_, renderCamera, destination, npcs_, kVirtualWidth);
    hud_renderer::DrawPlayerChatBubble(renderer_, renderCamera, destination, player_, playerChatText_, playerChatTimer_,
                                       kVirtualWidth);
    hud_renderer::DrawUI(renderer_, destination, kVirtualWidth, player_, appleCount_, berryCount_, pearCount_, meatCount_,
                         hostiles_, npcs_, nearbyNpcIndex_, aiBackendReady_, aiBackendStatus_, chatMode_, chatNpcIndex_,
                         chatReply_, chatInput_,
                         heldItem_ == HeldItem::Weapon ? "BLADE" :
                         heldItem_ == HeldItem::Fence ? "FENCE" :
                         heldItem_ == HeldItem::Soil ? "SOIL" :
                         heldItem_ == HeldItem::Seed ? "SEED" :
                         heldItem_ == HeldItem::Rod ? "ROD" : "BOW");
    SDL_RenderPresent(renderer_);
}

void App::UpdateEffects(float dt, bool moving) {
    fogAnimTime_ += dt;
    splashSpawnTimer_ = std::max(0.0f, splashSpawnTimer_ - dt);
    hostileSpawnTimer_ = std::max(0.0f, hostileSpawnTimer_ - dt);

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

    for (BloodParticle& b : bloodParticles_) {
        b.life += dt;
        b.worldX += b.velX * dt;
        b.worldY += b.velY * dt;
        b.velY += 16.0f * dt;
    }

    bloodParticles_.erase(
        std::remove_if(bloodParticles_.begin(), bloodParticles_.end(), [](const BloodParticle& b) { return b.life >= b.maxLife; }),
        bloodParticles_.end());

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

    for (const BloodParticle& b : bloodParticles_) {
        const float t = std::clamp(b.life / b.maxLife, 0.0f, 1.0f);
        const std::uint8_t a = static_cast<std::uint8_t>((1.0f - t) * 110.0f);
        SDL_SetRenderDrawColor(renderer_, b.red, b.green, b.blue, a);

        const float screenX = std::floor(b.worldX - camera.x);
        const float screenY = std::floor(b.worldY - camera.y);
        SDL_FRect bit{screenX, screenY, b.size, b.size};
        SDL_RenderFillRect(renderer_, &bit);
    }

    for (const ArrowShot& arrow : arrows_) {
        const float screenX = std::floor(arrow.worldX - camera.x);
        const float screenY = std::floor(arrow.worldY - camera.y);
        const bool horizontal = std::fabs(arrow.velX) >= std::fabs(arrow.velY);

        SDL_SetRenderDrawColor(renderer_, 184, 146, 92, 255);
        SDL_FRect shaft = horizontal
                              ? SDL_FRect{screenX - (arrow.velX >= 0.0f ? 4.0f : 1.0f), screenY, 5.0f, 1.0f}
                              : SDL_FRect{screenX, screenY - (arrow.velY >= 0.0f ? 4.0f : 1.0f), 1.0f, 5.0f};
        SDL_RenderFillRect(renderer_, &shaft);

        SDL_SetRenderDrawColor(renderer_, 215, 220, 226, 255);
        SDL_FRect tip = horizontal
                            ? SDL_FRect{screenX + (arrow.velX >= 0.0f ? 1.0f : -1.0f), screenY, 1.0f, 1.0f}
                            : SDL_FRect{screenX, screenY + (arrow.velY >= 0.0f ? 1.0f : -1.0f), 1.0f, 1.0f};
        SDL_RenderFillRect(renderer_, &tip);
    }

    if (fishingCast_.active) {
        const float bob = std::sin(fishingCast_.timer * 8.0f) * 0.8f;
        const float screenX = std::floor(fishingCast_.worldX - camera.x);
        const float screenY = std::floor((fishingCast_.worldY - camera.y) + bob);

        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, 223, 74, 66, 255);
        SDL_FRect bobber{screenX - 1.0f, screenY - 1.0f, 3.0f, 3.0f};
        SDL_RenderFillRect(renderer_, &bobber);

        SDL_SetRenderDrawColor(renderer_, 244, 245, 247, 255);
        SDL_FRect bobberTop{screenX, screenY - 2.0f, 1.0f, 1.0f};
        SDL_RenderFillRect(renderer_, &bobberTop);
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

void App::DrawFences(const Camera2D& camera, bool drawBeforePlayer, float splitWorldY) {
    for (const Fence& fence : fences_) {
        const float baseY = fence.y + fence.height;
        const bool behind = baseY <= splitWorldY;
        if (behind != drawBeforePlayer) {
            continue;
        }

        const float screenX = std::floor(fence.x - camera.x);
        const float screenY = std::floor(fence.y - camera.y);

        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 56);
        SDL_FRect shadow{screenX + 1.0f, screenY + fence.height - 1.0f, std::max(2.0f, fence.width - 2.0f), 2.0f};
        SDL_RenderFillRect(renderer_, &shadow);

        const bool vertical = fence.height > fence.width;
        if (vertical) {
            SDL_SetRenderDrawColor(renderer_, 171, 129, 82, 255);
            SDL_FRect postTop{screenX + 1.0f, screenY + 1.0f, fence.width - 2.0f, 2.0f};
            SDL_FRect postMid{screenX + 1.0f, screenY + (fence.height * 0.5f) - 1.0f, fence.width - 2.0f, 2.0f};
            SDL_FRect postBot{screenX + 1.0f, screenY + fence.height - 3.0f, fence.width - 2.0f, 2.0f};
            SDL_RenderFillRect(renderer_, &postTop);
            SDL_RenderFillRect(renderer_, &postMid);
            SDL_RenderFillRect(renderer_, &postBot);

            SDL_SetRenderDrawColor(renderer_, 129, 92, 58, 255);
            SDL_FRect railL{screenX + 1.0f, screenY + 1.0f, 2.0f, fence.height - 2.0f};
            SDL_FRect railR{screenX + fence.width - 3.0f, screenY + 1.0f, 2.0f, fence.height - 2.0f};
            SDL_RenderFillRect(renderer_, &railL);
            SDL_RenderFillRect(renderer_, &railR);
        } else {
            SDL_SetRenderDrawColor(renderer_, 129, 92, 58, 255);
            SDL_FRect postL{screenX + 1.0f, screenY + 1.0f, 2.0f, fence.height - 1.0f};
            SDL_FRect postM{screenX + (fence.width * 0.5f) - 1.0f, screenY + 1.0f, 2.0f, fence.height - 1.0f};
            SDL_FRect postR{screenX + fence.width - 3.0f, screenY + 1.0f, 2.0f, fence.height - 1.0f};
            SDL_RenderFillRect(renderer_, &postL);
            SDL_RenderFillRect(renderer_, &postM);
            SDL_RenderFillRect(renderer_, &postR);

            SDL_SetRenderDrawColor(renderer_, 171, 129, 82, 255);
            SDL_FRect railTop{screenX + 1.0f, screenY + 2.0f, fence.width - 2.0f, 2.0f};
            SDL_FRect railBot{screenX + 1.0f, screenY + fence.height - 4.0f, fence.width - 2.0f, 2.0f};
            SDL_RenderFillRect(renderer_, &railTop);
            SDL_RenderFillRect(renderer_, &railBot);
        }
    }
}

void App::DrawSoilPlots(const Camera2D& camera, bool drawBeforePlayer, float splitWorldY) {
    for (const SoilPlot& plot : soilPlots_) {
        const float baseY = plot.y + plot.height;
        const bool behind = baseY <= splitWorldY;
        if (behind != drawBeforePlayer) {
            continue;
        }

        const float screenX = std::floor(plot.x - camera.x);
        const float screenY = std::floor(plot.y - camera.y);

        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer_, 112, 78, 46, 255);
        SDL_FRect body{screenX, screenY, plot.width, plot.height};
        SDL_RenderFillRect(renderer_, &body);

        SDL_SetRenderDrawColor(renderer_, 132, 94, 58, 255);
        SDL_FRect top{screenX, screenY, plot.width, 1.0f};
        SDL_FRect left{screenX, screenY, 1.0f, plot.height};
        SDL_RenderFillRect(renderer_, &top);
        SDL_RenderFillRect(renderer_, &left);

        SDL_SetRenderDrawColor(renderer_, 79, 52, 31, 255);
        for (int furrow = 0; furrow < 3; ++furrow) {
            SDL_FRect line{screenX + 3.0f + (furrow * 4.0f), screenY + 2.0f, 1.5f, plot.height - 4.0f};
            SDL_RenderFillRect(renderer_, &line);
        }

        SDL_SetRenderDrawColor(renderer_, 121, 86, 52, 255);
        SDL_FRect mound0{screenX + 1.0f, screenY + 3.0f, plot.width - 2.0f, 1.0f};
        SDL_FRect mound1{screenX + 1.0f, screenY + 8.0f, plot.width - 2.0f, 1.0f};
        SDL_FRect mound2{screenX + 1.0f, screenY + 13.0f, plot.width - 2.0f, 1.0f};
        SDL_RenderFillRect(renderer_, &mound0);
        SDL_RenderFillRect(renderer_, &mound1);
        SDL_RenderFillRect(renderer_, &mound2);

        if (!plot.planted) {
            continue;
        }

        const float growth = plot.growDuration > 0.0f ? std::clamp(plot.growTimer / plot.growDuration, 0.0f, 1.0f) : 1.0f;
        const float centerX = screenX + (plot.width * 0.5f);
        const float rootY = screenY + plot.height - 2.0f;

        if (growth < 0.33f) {
            SDL_SetRenderDrawColor(renderer_, 62, 143, 71, 255);
            SDL_FRect sproutL{centerX - 2.0f, rootY - 4.0f, 1.5f, 4.0f};
            SDL_FRect sproutR{centerX + 0.5f, rootY - 4.0f, 1.5f, 4.0f};
            SDL_RenderFillRect(renderer_, &sproutL);
            SDL_RenderFillRect(renderer_, &sproutR);
        } else if (growth < 0.8f) {
            SDL_SetRenderDrawColor(renderer_, 83, 58, 37, 255);
            SDL_FRect stem{centerX - 1.0f, rootY - 8.0f, 2.0f, 8.0f};
            SDL_RenderFillRect(renderer_, &stem);

            SDL_SetRenderDrawColor(renderer_, 54, 136, 63, 255);
            SDL_FRect leaf0{centerX - 5.0f, rootY - 10.0f, 5.0f, 4.0f};
            SDL_FRect leaf1{centerX, rootY - 11.0f, 5.0f, 4.0f};
            SDL_FRect leaf2{centerX - 3.0f, rootY - 14.0f, 6.0f, 4.0f};
            SDL_RenderFillRect(renderer_, &leaf0);
            SDL_RenderFillRect(renderer_, &leaf1);
            SDL_RenderFillRect(renderer_, &leaf2);
        } else {
            SDL_SetRenderDrawColor(renderer_, 86, 58, 39, 255);
            SDL_FRect stem{centerX - 1.0f, rootY - 10.0f, 2.0f, 10.0f};
            SDL_RenderFillRect(renderer_, &stem);

            SDL_SetRenderDrawColor(renderer_, 42, 128, 56, 255);
            SDL_FRect crown0{centerX - 6.0f, rootY - 14.0f, 12.0f, 4.0f};
            SDL_FRect crown1{centerX - 5.0f, rootY - 18.0f, 10.0f, 4.0f};
            SDL_FRect crown2{centerX - 3.0f, rootY - 22.0f, 6.0f, 4.0f};
            SDL_RenderFillRect(renderer_, &crown0);
            SDL_RenderFillRect(renderer_, &crown1);
            SDL_RenderFillRect(renderer_, &crown2);

            if (plot.seedType == HarvestResult::Apple) {
                SDL_SetRenderDrawColor(renderer_, 219, 69, 68, 255);
            } else if (plot.seedType == HarvestResult::Berry) {
                SDL_SetRenderDrawColor(renderer_, 161, 87, 226, 255);
            } else {
                SDL_SetRenderDrawColor(renderer_, 240, 208, 81, 255);
            }
            SDL_FRect fruit0{centerX - 4.0f, rootY - 16.0f, 2.0f, 2.0f};
            SDL_FRect fruit1{centerX + 2.0f, rootY - 14.0f, 2.0f, 2.0f};
            SDL_RenderFillRect(renderer_, &fruit0);
            SDL_RenderFillRect(renderer_, &fruit1);
        }
    }
}

void App::DrawPlacementPreview(const Camera2D& camera) {
    if ((heldItem_ != HeldItem::Fence && heldItem_ != HeldItem::Soil) || currentLayer_ != 0) {
        return;
    }

    const bool soilPreview = heldItem_ == HeldItem::Soil;
    const float previewWidth = soilPreview ? 16.0f : (fenceVerticalPlacement_ ? 8.0f : 16.0f);
    const float previewHeight = soilPreview ? 16.0f : (fenceVerticalPlacement_ ? 16.0f : 8.0f);
    const float facingY = player_.FacingY();
    const float previewX =
        soilPreview
            ? (std::fabs(facingY) > 0.01f
                   ? std::floor((player_.CenterX() - (previewWidth * 0.5f)) / 8.0f) * 8.0f
                   : std::floor((player_.CenterX() + (player_.FacingX() * 14.0f) - (previewWidth * 0.5f)) / 8.0f) * 8.0f)
            : (std::fabs(facingY) > 0.01f
                   ? (player_.CenterX() - (previewWidth * 0.5f))
                   : (player_.CenterX() + (player_.FacingX() * 12.0f) - (previewWidth * 0.5f)));
    const float previewY =
        soilPreview
            ? (std::fabs(facingY) > 0.01f
                   ? std::floor((player_.FeetY() + (facingY * 14.0f) - previewHeight) / 8.0f) * 8.0f
                   : std::floor((player_.FeetY() - previewHeight) / 8.0f) * 8.0f)
            : (std::fabs(facingY) > 0.01f
                   ? (player_.FeetY() + (facingY * 12.0f) - previewHeight)
                   : (player_.FeetY() - previewHeight));

    const float probeX = previewX + (previewWidth * 0.5f);
    const float probeY = previewY + previewHeight;
    const bool validSpot =
        !map_.IsWaterAt(probeX, probeY) &&
        !map_.IsBlockedAt(probeX, probeY, 0) &&
        !std::any_of(fences_.begin(), fences_.end(), [&](const Fence& fence) {
            return fence.health > 0 &&
                   probeX >= fence.x &&
                   probeX <= (fence.x + fence.width) &&
                   probeY >= fence.y &&
                   probeY <= (fence.y + fence.height);
        }) &&
        !std::any_of(soilPlots_.begin(), soilPlots_.end(), [&](const SoilPlot& plot) {
            return previewX < (plot.x + plot.width) &&
                   (previewX + previewWidth) > plot.x &&
                   previewY < (plot.y + plot.height) &&
                   (previewY + previewHeight) > plot.y;
        });

    const float blink = 0.45f + (0.25f * (0.5f + (0.5f * std::sin(fogAnimTime_ * 7.0f))));
    const Uint8 alpha = static_cast<Uint8>((validSpot ? 120.0f : 90.0f) + (blink * 60.0f));
    const float screenX = std::floor(previewX - camera.x);
    const float screenY = std::floor(previewY - camera.y);

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, alpha / 3);
    SDL_FRect shadow{screenX + 1.0f, screenY + previewHeight - 1.0f, std::max(2.0f, previewWidth - 2.0f), 2.0f};
    SDL_RenderFillRect(renderer_, &shadow);

    if (soilPreview) {
        SDL_SetRenderDrawColor(renderer_, validSpot ? 132 : 158, validSpot ? 98 : 84, validSpot ? 64 : 84, alpha);
        SDL_FRect body{screenX, screenY, previewWidth, previewHeight};
        SDL_RenderFillRect(renderer_, &body);

        SDL_SetRenderDrawColor(renderer_, validSpot ? 166 : 212, validSpot ? 127 : 118, validSpot ? 84 : 118, alpha);
        SDL_FRect top{screenX, screenY, previewWidth, 2.0f};
        SDL_FRect left{screenX, screenY, 2.0f, previewHeight};
        SDL_RenderFillRect(renderer_, &top);
        SDL_RenderFillRect(renderer_, &left);

        SDL_SetRenderDrawColor(renderer_, validSpot ? 92 : 168, validSpot ? 66 : 84, validSpot ? 40 : 84, alpha);
        for (int furrow = 0; furrow < 3; ++furrow) {
            SDL_FRect line{screenX + 3.0f + (furrow * 4.0f), screenY + 2.0f, 1.5f, previewHeight - 4.0f};
            SDL_RenderFillRect(renderer_, &line);
        }
        return;
    }

    const bool vertical = previewHeight > previewWidth;
    if (vertical) {
        SDL_SetRenderDrawColor(renderer_, validSpot ? 214 : 218, validSpot ? 177 : 118, validSpot ? 124 : 118, alpha);
        SDL_FRect postTop{screenX + 1.0f, screenY + 1.0f, previewWidth - 2.0f, 2.0f};
        SDL_FRect postMid{screenX + 1.0f, screenY + (previewHeight * 0.5f) - 1.0f, previewWidth - 2.0f, 2.0f};
        SDL_FRect postBot{screenX + 1.0f, screenY + previewHeight - 3.0f, previewWidth - 2.0f, 2.0f};
        SDL_RenderFillRect(renderer_, &postTop);
        SDL_RenderFillRect(renderer_, &postMid);
        SDL_RenderFillRect(renderer_, &postBot);

        SDL_SetRenderDrawColor(renderer_, validSpot ? 176 : 168, validSpot ? 133 : 84, validSpot ? 88 : 84, alpha);
        SDL_FRect railL{screenX + 1.0f, screenY + 1.0f, 2.0f, previewHeight - 2.0f};
        SDL_FRect railR{screenX + previewWidth - 3.0f, screenY + 1.0f, 2.0f, previewHeight - 2.0f};
        SDL_RenderFillRect(renderer_, &railL);
        SDL_RenderFillRect(renderer_, &railR);
    } else {
        SDL_SetRenderDrawColor(renderer_, validSpot ? 176 : 168, validSpot ? 133 : 84, validSpot ? 88 : 84, alpha);
        SDL_FRect postL{screenX + 1.0f, screenY + 1.0f, 2.0f, previewHeight - 1.0f};
        SDL_FRect postM{screenX + (previewWidth * 0.5f) - 1.0f, screenY + 1.0f, 2.0f, previewHeight - 1.0f};
        SDL_FRect postR{screenX + previewWidth - 3.0f, screenY + 1.0f, 2.0f, previewHeight - 1.0f};
        SDL_RenderFillRect(renderer_, &postL);
        SDL_RenderFillRect(renderer_, &postM);
        SDL_RenderFillRect(renderer_, &postR);

        SDL_SetRenderDrawColor(renderer_, validSpot ? 214 : 218, validSpot ? 177 : 118, validSpot ? 124 : 118, alpha);
        SDL_FRect railTop{screenX + 1.0f, screenY + 2.0f, previewWidth - 2.0f, 2.0f};
        SDL_FRect railBot{screenX + 1.0f, screenY + previewHeight - 4.0f, previewWidth - 2.0f, 2.0f};
        SDL_RenderFillRect(renderer_, &railTop);
        SDL_RenderFillRect(renderer_, &railBot);
    }
}

void App::DrawFogOfWar(const Camera2D& camera, const SDL_FRect& destination) {
    void* pixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(fogTexture_, nullptr, &pixels, &pitch)) {
        return;
    }

    struct LightSource {
        float x;
        float y;
        float innerRadius;
        float outerRadius;
        float intensity;
    };

    std::vector<LightSource> lights;
    lights.reserve(2 + npcs_.size());
    lights.push_back(LightSource{
        player_.CenterX() - camera.x,
        player_.CenterY() - camera.y,
        42.0f,
        108.0f,
        1.0f
    });
    lights.push_back(LightSource{
        miko_.CenterX() - camera.x,
        miko_.CenterY() - camera.y,
        22.0f,
        58.0f,
        0.58f
    });

    for (const NpcAI& npc : npcs_) {
        lights.push_back(LightSource{
            npc.CenterX() - camera.x,
            npc.CenterY() - camera.y,
            22.0f,
            58.0f,
            0.58f
        });
    }

    const float maxAlpha = 220.0f;

    std::uint8_t* row = static_cast<std::uint8_t*>(pixels);
    const float cx = static_cast<float>(kVirtualWidth) * 0.5f;
    const float cy = static_cast<float>(kVirtualHeight) * 0.5f;
    const float vignetteMax = std::sqrt((cx * cx) + (cy * cy));

    for (int y = 0; y < kVirtualHeight; ++y) {
        std::uint32_t* out = reinterpret_cast<std::uint32_t*>(row + (y * pitch));
        for (int x = 0; x < kVirtualWidth; ++x) {
            float lightAlpha = maxAlpha;
            for (const LightSource& light : lights) {
                const float dx = static_cast<float>(x) - light.x;
                const float dy = static_cast<float>(y) - light.y;
                const float dist = std::sqrt((dx * dx) + (dy * dy));

                const float coreAlpha = maxAlpha * (1.0f - light.intensity);
                float candidateAlpha = maxAlpha;
                if (dist <= light.innerRadius) {
                    candidateAlpha = coreAlpha;
                } else if (dist < light.outerRadius) {
                    const float t = (dist - light.innerRadius) / (light.outerRadius - light.innerRadius);
                    const float eased = t * t * (3.0f - (2.0f * t));
                    candidateAlpha = coreAlpha + (eased * (maxAlpha - coreAlpha));
                }

                lightAlpha = std::min(lightAlpha, candidateAlpha);
            }

            const float driftX = (static_cast<float>(x) * 0.052f) + (fogAnimTime_ * 0.53f);
            const float driftY = (static_cast<float>(y) * 0.046f) - (fogAnimTime_ * 0.37f);
            const float swirl =
                std::sin(driftX) * 11.0f +
                std::cos(driftY) * 9.0f +
                std::sin((static_cast<float>(x + y) * 0.028f) + (fogAnimTime_ * 0.82f)) * 6.0f;

            float alphaF = lightAlpha;
            const float vx = static_cast<float>(x) - cx;
            const float vy = static_cast<float>(y) - cy;
            const float vignette = std::sqrt((vx * vx) + (vy * vy)) / vignetteMax;
            alphaF += std::clamp(vignette - 0.35f, 0.0f, 0.65f) * 80.0f;
            alphaF += swirl;

            const float lightPulse = 6.0f + (4.0f * std::sin(fogAnimTime_ * 1.8f));
            alphaF -= lightPulse * std::exp(-((vx * vx) + (vy * vy)) / 14000.0f);

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

