#include "game/TileMap.h"

#include <algorithm>
#include <cmath>

namespace {
std::uint8_t HashTile(int x, int y) {
    const std::uint32_t ux = static_cast<std::uint32_t>(x);
    const std::uint32_t uy = static_cast<std::uint32_t>(y);
    std::uint32_t n = (ux * 374761393U) ^ (uy * 668265263U);
    n = (n ^ (n >> 13)) * 1274126177U;
    return static_cast<std::uint8_t>((n ^ (n >> 16)) % 3U);
}

std::uint32_t Hash32(int x, int y, std::uint32_t seed) {
    const std::uint32_t ux = static_cast<std::uint32_t>(x);
    const std::uint32_t uy = static_cast<std::uint32_t>(y);
    std::uint32_t h = seed;
    h ^= ux + 0x9e3779b9U + (h << 6U) + (h >> 2U);
    h ^= uy + 0x85ebca6bU + (h << 6U) + (h >> 2U);
    h ^= (h >> 16U);
    h *= 0x7feb352dU;
    h ^= (h >> 15U);
    h *= 0x846ca68bU;
    h ^= (h >> 16U);
    return h;
}

float Hash01(int x, int y, std::uint32_t seed) {
    const std::uint32_t h = Hash32(x, y, seed);
    return static_cast<float>(h & 0x00ffffffU) / static_cast<float>(0x01000000U);
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float Smooth(float t) {
    return t * t * (3.0f - (2.0f * t));
}

float ValueNoise(float x, float y, std::uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = Smooth(x - static_cast<float>(x0));
    const float ty = Smooth(y - static_cast<float>(y0));

    const float n00 = Hash01(x0, y0, seed);
    const float n10 = Hash01(x1, y0, seed);
    const float n01 = Hash01(x0, y1, seed);
    const float n11 = Hash01(x1, y1, seed);

    const float nx0 = Lerp(n00, n10, tx);
    const float nx1 = Lerp(n01, n11, tx);
    return Lerp(nx0, nx1, ty);
}

float FractalNoise(float x, float y, std::uint32_t seed) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int i = 0; i < 4; ++i) {
        value += ValueNoise(x * frequency, y * frequency, seed + static_cast<std::uint32_t>(i * 1013)) * amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return std::clamp(value, 0.0f, 1.0f);
}

enum class Terrain : std::uint8_t {
    Water,
    Sand,
    Grass,
    DarkGrass,
    Dirt
};

Terrain GetTerrainAt(int tx, int ty) {
    const float elevation = FractalNoise(static_cast<float>(tx) * 0.03f, static_cast<float>(ty) * 0.03f, 1337U);
    const float moisture = FractalNoise(static_cast<float>(tx) * 0.035f, static_cast<float>(ty) * 0.035f, 9001U);

    if (elevation < 0.35f) {
        return Terrain::Water;
    }
    if (elevation < 0.40f) {
        return Terrain::Sand;
    }
    if (elevation > 0.78f) {
        return Terrain::Dirt;
    }
    if (moisture > 0.62f) {
        return Terrain::DarkGrass;
    }
    return Terrain::Grass;
}

bool IsMountainTile(int tx, int ty) {
    const float ridge = FractalNoise(static_cast<float>(tx) * 0.021f, static_cast<float>(ty) * 0.021f, 7823U);
    return ridge > 0.72f;
}

bool IsCliffEdgeTile(int tx, int ty) {
    if (!IsMountainTile(tx, ty)) {
        return false;
    }

    return !IsMountainTile(tx + 1, ty) || !IsMountainTile(tx - 1, ty) || !IsMountainTile(tx, ty + 1) ||
        !IsMountainTile(tx, ty - 1);
}

bool IsPassCorridor(int tx, int ty) {
    // Low frequency mask creates sparse mountain passes.
    const float passNoise = FractalNoise(static_cast<float>(tx) * 0.012f, static_cast<float>(ty) * 0.012f, 5549U);
    return passNoise > 0.79f;
}

bool IsMountainRampTile(int tx, int ty) {
    if (!IsMountainTile(tx, ty)) {
        return false;
    }

    if (!IsCliffEdgeTile(tx, ty)) {
        return false;
    }

    // Ramps only appear on designated pass corridors and only at sparse checkpoints.
    if (!IsPassCorridor(tx, ty)) {
        return false;
    }

    const bool sparseGate = ((std::abs(tx) + (std::abs(ty) * 3)) % 9) == 0;
    if (!sparseGate) {
        return false;
    }

    return Hash01(tx, ty, 4411U) > 0.55f;
}

void SetTerrainColor(SDL_Renderer* renderer, Terrain terrain, std::uint8_t variant) {
    switch (terrain) {
        case Terrain::Water:
            if (variant == 0) {
                SDL_SetRenderDrawColor(renderer, 54, 120, 192, 255);
            } else if (variant == 1) {
                SDL_SetRenderDrawColor(renderer, 43, 108, 181, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 62, 130, 201, 255);
            }
            break;
        case Terrain::Sand:
            SDL_SetRenderDrawColor(renderer, 207, 191, 124, 255);
            break;
        case Terrain::Grass:
            SDL_SetRenderDrawColor(renderer, 94, 165, 75, 255);
            break;
        case Terrain::DarkGrass:
            SDL_SetRenderDrawColor(renderer, 76, 146, 63, 255);
            break;
        case Terrain::Dirt:
            SDL_SetRenderDrawColor(renderer, 136, 101, 68, 255);
            break;
    }
}

void DrawGroundTexture(SDL_Renderer* renderer, Terrain terrain, float x, float y, float tile, std::uint32_t tileHash, float waterTime) {
    if (terrain == Terrain::Sand) {
        SDL_SetRenderDrawColor(renderer, 191, 172, 110, 255);
        const float d0x = x + static_cast<float>((tileHash >> 1U) & 5U);
        const float d0y = y + static_cast<float>((tileHash >> 4U) & 5U);
        const float d1x = x + static_cast<float>((tileHash >> 7U) & 5U) + (tile * 0.28f);
        const float d1y = y + static_cast<float>((tileHash >> 10U) & 5U) + (tile * 0.18f);
        SDL_FRect detailA{d0x, d0y, 1.0f, 1.0f};
        SDL_FRect detailB{d1x, d1y, 1.0f, 1.0f};
        SDL_RenderFillRect(renderer, &detailA);
        SDL_RenderFillRect(renderer, &detailB);

        SDL_SetRenderDrawColor(renderer, 226, 210, 148, 110);
        SDL_FRect warmBand{x + tile * 0.10f, y + tile * 0.12f, tile * 0.56f, 1.0f};
        SDL_RenderFillRect(renderer, &warmBand);
        return;
    }

    if (terrain == Terrain::Water) {
        const float phase = waterTime + (static_cast<float>(static_cast<int>(x / tile)) * 0.73f) + (static_cast<float>(static_cast<int>(y / tile)) * 0.41f);
        const float rippleA = (std::sin(phase) + 1.0f) * 0.5f;
        const float rippleB = (std::sin((phase * 1.37f) + 1.2f) + 1.0f) * 0.5f;

        SDL_SetRenderDrawColor(renderer, 91, 170, 233, 120);
        SDL_FRect waveA{x + (tile * 0.14f), y + (tile * (0.22f + (rippleA * 0.18f))), tile * 0.40f, 1.0f};
        SDL_FRect waveB{x + (tile * 0.52f), y + (tile * (0.56f + (rippleB * 0.16f))), tile * 0.28f, 1.0f};
        SDL_RenderFillRect(renderer, &waveA);
        SDL_RenderFillRect(renderer, &waveB);

        SDL_SetRenderDrawColor(renderer, 126, 199, 247, 86);
        SDL_FRect shimmer{x + (tile * (0.18f + (rippleB * 0.16f))), y + (tile * (0.72f - (rippleA * 0.10f))), tile * 0.18f, 1.0f};
        SDL_RenderFillRect(renderer, &shimmer);
        return;
    }

    if (terrain == Terrain::Grass || terrain == Terrain::DarkGrass) {
        const std::uint8_t bladeTint = terrain == Terrain::Grass ? 82 : 68;
        SDL_SetRenderDrawColor(renderer, bladeTint, static_cast<std::uint8_t>(150 + ((tileHash >> 3U) & 7U)),
            static_cast<std::uint8_t>(56 + ((tileHash >> 6U) & 7U)), 255);
        SDL_FRect bladeA{x + tile * 0.12f, y + tile * 0.24f, 1.0f, tile * 0.22f};
        SDL_FRect bladeB{x + tile * 0.54f, y + tile * 0.44f, 1.0f, tile * 0.20f};
        SDL_FRect bladeC{x + tile * 0.78f, y + tile * 0.18f, 1.0f, tile * 0.18f};
        SDL_RenderFillRect(renderer, &bladeA);
        SDL_RenderFillRect(renderer, &bladeB);
        SDL_RenderFillRect(renderer, &bladeC);

        SDL_SetRenderDrawColor(renderer, terrain == Terrain::Grass ? 126 : 102, terrain == Terrain::Grass ? 191 : 168,
            terrain == Terrain::Grass ? 94 : 82, 108);
        SDL_FRect lightPatch{x + tile * 0.18f, y + tile * 0.10f, tile * 0.24f, 1.0f};
        SDL_RenderFillRect(renderer, &lightPatch);
        return;
    }

    if (terrain == Terrain::Dirt) {
        SDL_SetRenderDrawColor(renderer, 118, 84, 58, 255);
        SDL_FRect crackA{x + tile * 0.20f, y + tile * 0.24f, tile * 0.28f, 1.0f};
        SDL_FRect crackB{x + tile * 0.56f, y + tile * 0.56f, tile * 0.22f, 1.0f};
        SDL_RenderFillRect(renderer, &crackA);
        SDL_RenderFillRect(renderer, &crackB);
        SDL_SetRenderDrawColor(renderer, 154, 118, 82, 96);
        SDL_FRect dust{x + tile * 0.12f, y + tile * 0.12f, 1.0f, 1.0f};
        SDL_FRect dust2{x + tile * 0.82f, y + tile * 0.32f, 1.0f, 1.0f};
        SDL_RenderFillRect(renderer, &dust);
        SDL_RenderFillRect(renderer, &dust2);
    }
}


void DrawRampMark(SDL_Renderer* renderer, float x, float y, float tile) {
    SDL_SetRenderDrawColor(renderer, 182, 160, 112, 255);
    SDL_FRect r0{x + tile * 0.20f, y + tile * 0.75f, tile * 0.70f, tile * 0.18f};
    SDL_FRect r1{x + tile * 0.35f, y + tile * 0.50f, tile * 0.70f, tile * 0.16f};
    SDL_FRect r2{x + tile * 0.50f, y + tile * 0.26f, tile * 0.70f, tile * 0.14f};
    SDL_RenderFillRect(renderer, &r0);
    SDL_RenderFillRect(renderer, &r1);
    SDL_RenderFillRect(renderer, &r2);
}

void DrawCliffFace(SDL_Renderer* renderer, float x, float y, float tile) {
    SDL_SetRenderDrawColor(renderer, 72, 68, 78, 255);
    SDL_FRect face{x, y + tile * 0.60f, tile, tile * 0.40f};
    SDL_RenderFillRect(renderer, &face);

    SDL_SetRenderDrawColor(renderer, 92, 90, 100, 255);
    SDL_FRect lip{x, y + tile * 0.56f, tile, tile * 0.08f};
    SDL_RenderFillRect(renderer, &lip);
}

void DrawTree(SDL_Renderer* renderer, float x, float y, float tile) {
    SDL_SetRenderDrawColor(renderer, 90, 58, 37, 255);
    SDL_FRect trunkCore{x + tile * 0.98f, y - tile * 1.20f, tile * 0.34f, tile * 1.02f};
    SDL_FRect trunkMid{x + tile * 0.90f, y - tile * 0.78f, tile * 0.50f, tile * 0.56f};
    SDL_FRect rootL{x + tile * 0.76f, y - tile * 0.18f, tile * 0.26f, tile * 0.18f};
    SDL_FRect rootR{x + tile * 1.28f, y - tile * 0.18f, tile * 0.26f, tile * 0.18f};
    SDL_RenderFillRect(renderer, &trunkCore);
    SDL_RenderFillRect(renderer, &trunkMid);
    SDL_RenderFillRect(renderer, &rootL);
    SDL_RenderFillRect(renderer, &rootR);

    SDL_SetRenderDrawColor(renderer, 118, 78, 50, 255);
    SDL_FRect barkLight{x + tile * 0.96f, y - tile * 1.10f, tile * 0.10f, tile * 0.90f};
    SDL_FRect barkLightLow{x + tile * 0.92f, y - tile * 0.56f, tile * 0.08f, tile * 0.26f};
    SDL_RenderFillRect(renderer, &barkLight);
    SDL_RenderFillRect(renderer, &barkLightLow);

    SDL_SetRenderDrawColor(renderer, 68, 43, 28, 255);
    SDL_FRect barkShadow{x + tile * 1.24f, y - tile * 1.10f, tile * 0.10f, tile * 0.92f};
    SDL_FRect barkShadowLow{x + tile * 1.22f, y - tile * 0.54f, tile * 0.12f, tile * 0.28f};
    SDL_RenderFillRect(renderer, &barkShadow);
    SDL_RenderFillRect(renderer, &barkShadowLow);

    SDL_SetRenderDrawColor(renderer, 30, 110, 46, 255);
    SDL_FRect crownTop{x + tile * 0.70f, y - tile * 2.76f, tile * 0.92f, tile * 0.30f};
    SDL_FRect crownUpper{x + tile * 0.34f, y - tile * 2.46f, tile * 1.66f, tile * 0.34f};
    SDL_FRect crownLeft{x + tile * 0.10f, y - tile * 2.10f, tile * 0.76f, tile * 0.58f};
    SDL_FRect crownCore{x + tile * 0.34f, y - tile * 2.06f, tile * 1.52f, tile * 0.86f};
    SDL_FRect crownRight{x + tile * 1.38f, y - tile * 2.00f, tile * 0.62f, tile * 0.56f};
    SDL_FRect crownBottom{x + tile * 0.26f, y - tile * 1.34f, tile * 1.62f, tile * 0.30f};
    SDL_RenderFillRect(renderer, &crownTop);
    SDL_RenderFillRect(renderer, &crownUpper);
    SDL_RenderFillRect(renderer, &crownLeft);
    SDL_RenderFillRect(renderer, &crownCore);
    SDL_RenderFillRect(renderer, &crownRight);
    SDL_RenderFillRect(renderer, &crownBottom);

    SDL_SetRenderDrawColor(renderer, 25, 92, 40, 255);
    SDL_FRect shadeRight{x + tile * 1.20f, y - tile * 2.12f, tile * 0.62f, tile * 0.72f};
    SDL_FRect shadeLow{x + tile * 0.94f, y - tile * 1.54f, tile * 0.70f, tile * 0.28f};
    SDL_FRect shadeInner{x + tile * 0.48f, y - tile * 1.94f, tile * 0.48f, tile * 0.32f};
    SDL_RenderFillRect(renderer, &shadeRight);
    SDL_RenderFillRect(renderer, &shadeLow);
    SDL_RenderFillRect(renderer, &shadeInner);

    SDL_SetRenderDrawColor(renderer, 62, 154, 82, 255);
    SDL_FRect lightTop{x + tile * 0.74f, y - tile * 2.58f, tile * 0.34f, tile * 0.18f};
    SDL_FRect lightLeft{x + tile * 0.40f, y - tile * 2.24f, tile * 0.28f, tile * 0.18f};
    SDL_FRect lightMid{x + tile * 0.62f, y - tile * 1.82f, tile * 0.30f, tile * 0.16f};
    SDL_RenderFillRect(renderer, &lightTop);
    SDL_RenderFillRect(renderer, &lightLeft);
    SDL_RenderFillRect(renderer, &lightMid);
}

void DrawRock(SDL_Renderer* renderer, float x, float y, float tile) {
    // Stepped irregular body for a rough rock shape.
    SDL_SetRenderDrawColor(renderer, 121, 129, 137, 255);
    SDL_FRect r0{x + tile * 0.26f, y + tile * 0.26f, tile * 0.92f, tile * 0.20f};
    SDL_FRect r1{x + tile * 0.14f, y + tile * 0.46f, tile * 1.18f, tile * 0.22f};
    SDL_FRect r2{x + tile * 0.08f, y + tile * 0.68f, tile * 1.34f, tile * 0.22f};
    SDL_FRect r3{x + tile * 0.16f, y + tile * 0.90f, tile * 1.12f, tile * 0.20f};
    SDL_RenderFillRect(renderer, &r0);
    SDL_RenderFillRect(renderer, &r1);
    SDL_RenderFillRect(renderer, &r2);
    SDL_RenderFillRect(renderer, &r3);

    SDL_SetRenderDrawColor(renderer, 99, 107, 116, 255);
    SDL_FRect shade0{x + tile * 0.90f, y + tile * 0.50f, tile * 0.36f, tile * 0.46f};
    SDL_FRect shade1{x + tile * 0.74f, y + tile * 0.76f, tile * 0.46f, tile * 0.26f};
    SDL_RenderFillRect(renderer, &shade0);
    SDL_RenderFillRect(renderer, &shade1);

    SDL_SetRenderDrawColor(renderer, 86, 92, 101, 255);
    SDL_FRect crackA{x + tile * 0.52f, y + tile * 0.48f, tile * 0.10f, tile * 0.34f};
    SDL_FRect crackB{x + tile * 0.46f, y + tile * 0.62f, tile * 0.24f, tile * 0.10f};
    SDL_RenderFillRect(renderer, &crackA);
    SDL_RenderFillRect(renderer, &crackB);

    SDL_SetRenderDrawColor(renderer, 168, 176, 184, 255);
    SDL_FRect shine0{x + tile * 0.34f, y + tile * 0.40f, tile * 0.22f, tile * 0.12f};
    SDL_FRect shine1{x + tile * 0.60f, y + tile * 0.34f, tile * 0.16f, tile * 0.10f};
    SDL_RenderFillRect(renderer, &shine0);
    SDL_RenderFillRect(renderer, &shine1);
}

void DrawHouse(SDL_Renderer* renderer, float x, float y, float tile) {
    SDL_SetRenderDrawColor(renderer, 206, 170, 126, 255);
    SDL_FRect body{x - tile * 0.25f, y - tile * 2.2f, tile * 3.0f, tile * 2.2f};
    SDL_RenderFillRect(renderer, &body);

    SDL_SetRenderDrawColor(renderer, 179, 145, 108, 255);
    SDL_FRect wallShade{x + tile * 1.75f, y - tile * 2.2f, tile * 0.90f, tile * 2.2f};
    SDL_RenderFillRect(renderer, &wallShade);

    SDL_SetRenderDrawColor(renderer, 222, 190, 146, 255);
    SDL_FRect wallLight{x - tile * 0.10f, y - tile * 2.0f, tile * 0.20f, tile * 1.85f};
    SDL_RenderFillRect(renderer, &wallLight);

    SDL_SetRenderDrawColor(renderer, 156, 72, 58, 255);
    SDL_FRect roof{x - tile * 0.45f, y - tile * 2.65f, tile * 3.4f, tile * 0.65f};
    SDL_RenderFillRect(renderer, &roof);

    SDL_SetRenderDrawColor(renderer, 128, 56, 46, 255);
    SDL_FRect roofShade{x + tile * 1.45f, y - tile * 2.65f, tile * 1.35f, tile * 0.65f};
    SDL_RenderFillRect(renderer, &roofShade);

    SDL_SetRenderDrawColor(renderer, 188, 93, 76, 255);
    SDL_FRect roofLight{x - tile * 0.28f, y - tile * 2.56f, tile * 1.10f, tile * 0.18f};
    SDL_RenderFillRect(renderer, &roofLight);

    SDL_SetRenderDrawColor(renderer, 88, 51, 35, 255);
    SDL_FRect door{x + tile * 1.05f, y - tile * 1.1f, tile * 0.6f, tile * 1.1f};
    SDL_RenderFillRect(renderer, &door);

    SDL_SetRenderDrawColor(renderer, 110, 69, 49, 255);
    SDL_FRect doorLight{x + tile * 1.10f, y - tile * 1.02f, tile * 0.10f, tile * 0.84f};
    SDL_RenderFillRect(renderer, &doorLight);

    SDL_SetRenderDrawColor(renderer, 134, 202, 226, 255);
    SDL_FRect winL{x + tile * 0.28f, y - tile * 1.72f, tile * 0.40f, tile * 0.34f};
    SDL_FRect winR{x + tile * 1.88f, y - tile * 1.72f, tile * 0.40f, tile * 0.34f};
    SDL_RenderFillRect(renderer, &winL);
    SDL_RenderFillRect(renderer, &winR);

    SDL_SetRenderDrawColor(renderer, 82, 117, 132, 255);
    SDL_FRect winFrameL{x + tile * 0.28f, y - tile * 1.55f, tile * 0.40f, tile * 0.06f};
    SDL_FRect winFrameR{x + tile * 1.88f, y - tile * 1.55f, tile * 0.40f, tile * 0.06f};
    SDL_RenderFillRect(renderer, &winFrameL);
    SDL_RenderFillRect(renderer, &winFrameR);
}

void DrawFruitTree(SDL_Renderer* renderer, float x, float y, float tile, int fruitTypeCode) {
    SDL_SetRenderDrawColor(renderer, 90, 58, 37, 255);
    SDL_FRect trunkCore{x + tile * 0.99f, y - tile * 1.08f, tile * 0.30f, tile * 0.92f};
    SDL_FRect trunkBase{x + tile * 0.90f, y - tile * 0.54f, tile * 0.48f, tile * 0.36f};
    SDL_FRect rootL{x + tile * 0.78f, y - tile * 0.16f, tile * 0.22f, tile * 0.14f};
    SDL_FRect rootR{x + tile * 1.28f, y - tile * 0.16f, tile * 0.22f, tile * 0.14f};
    SDL_RenderFillRect(renderer, &trunkCore);
    SDL_RenderFillRect(renderer, &trunkBase);
    SDL_RenderFillRect(renderer, &rootL);
    SDL_RenderFillRect(renderer, &rootR);

    SDL_SetRenderDrawColor(renderer, 37, 124, 54, 255);
    SDL_FRect crownTop{x + tile * 0.60f, y - tile * 2.56f, tile * 0.96f, tile * 0.28f};
    SDL_FRect crownUpper{x + tile * 0.28f, y - tile * 2.28f, tile * 1.56f, tile * 0.34f};
    SDL_FRect crownLeft{x + tile * 0.08f, y - tile * 1.98f, tile * 0.62f, tile * 0.54f};
    SDL_FRect crownCore{x + tile * 0.30f, y - tile * 1.96f, tile * 1.46f, tile * 0.82f};
    SDL_FRect crownRight{x + tile * 1.26f, y - tile * 1.92f, tile * 0.60f, tile * 0.50f};
    SDL_FRect crownBottom{x + tile * 0.34f, y - tile * 1.24f, tile * 1.34f, tile * 0.24f};
    SDL_RenderFillRect(renderer, &crownTop);
    SDL_RenderFillRect(renderer, &crownUpper);
    SDL_RenderFillRect(renderer, &crownLeft);
    SDL_RenderFillRect(renderer, &crownCore);
    SDL_RenderFillRect(renderer, &crownRight);
    SDL_RenderFillRect(renderer, &crownBottom);

    SDL_SetRenderDrawColor(renderer, 63, 154, 82, 255);
    SDL_FRect lightTop{x + tile * 0.64f, y - tile * 2.38f, tile * 0.28f, tile * 0.16f};
    SDL_FRect lightLeft{x + tile * 0.38f, y - tile * 2.04f, tile * 0.24f, tile * 0.16f};
    SDL_FRect lightLow{x + tile * 0.56f, y - tile * 1.58f, tile * 0.24f, tile * 0.16f};
    SDL_RenderFillRect(renderer, &lightTop);
    SDL_RenderFillRect(renderer, &lightLeft);
    SDL_RenderFillRect(renderer, &lightLow);

    SDL_SetRenderDrawColor(renderer, 28, 98, 44, 255);
    SDL_FRect shadeRight{x + tile * 1.12f, y - tile * 2.02f, tile * 0.52f, tile * 0.62f};
    SDL_FRect shadeLow{x + tile * 0.90f, y - tile * 1.48f, tile * 0.54f, tile * 0.22f};
    SDL_RenderFillRect(renderer, &shadeRight);
    SDL_RenderFillRect(renderer, &shadeLow);

    if (fruitTypeCode == 0) {
        SDL_SetRenderDrawColor(renderer, 223, 64, 66, 255);
    } else if (fruitTypeCode == 1) {
        SDL_SetRenderDrawColor(renderer, 166, 86, 228, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 241, 210, 76, 255);
    }

    SDL_FRect fruitA{x + tile * 0.54f, y - tile * 1.98f, tile * 0.22f, tile * 0.22f};
    SDL_FRect fruitB{x + tile * 1.18f, y - tile * 1.74f, tile * 0.22f, tile * 0.22f};
    SDL_FRect fruitC{x + tile * 0.88f, y - tile * 1.42f, tile * 0.22f, tile * 0.22f};
    SDL_FRect fruitD{x + tile * 1.06f, y - tile * 2.12f, tile * 0.18f, tile * 0.18f};
    SDL_RenderFillRect(renderer, &fruitA);
    SDL_RenderFillRect(renderer, &fruitB);
    SDL_RenderFillRect(renderer, &fruitC);
    SDL_RenderFillRect(renderer, &fruitD);
}

enum class PropType : std::uint8_t {
    None,
    Tree,
    Rock,
    House,
    Bush,
    TallGrass,
    FlowerPatch,
    AppleTree,
    BerryTree,
    PearTree
};

void DrawBush(SDL_Renderer* renderer, float x, float y, float tile, std::uint32_t hash) {
    const std::uint8_t tint = static_cast<std::uint8_t>(hash & 0x1fU);
    SDL_SetRenderDrawColor(renderer, static_cast<std::uint8_t>(42 + tint), static_cast<std::uint8_t>(128 + (tint / 2U)),
        static_cast<std::uint8_t>(54 + (tint / 3U)), 255);
    SDL_FRect core{x + tile * 0.20f, y + tile * 0.35f, tile * 1.6f, tile * 1.0f};
    SDL_RenderFillRect(renderer, &core);

    SDL_SetRenderDrawColor(renderer, static_cast<std::uint8_t>(58 + tint), static_cast<std::uint8_t>(156 + (tint / 2U)),
        static_cast<std::uint8_t>(71 + (tint / 3U)), 255);
    SDL_FRect top{x + tile * 0.44f, y + tile * 0.08f, tile * 1.1f, tile * 0.6f};
    SDL_RenderFillRect(renderer, &top);
    SDL_SetRenderDrawColor(renderer, static_cast<std::uint8_t>(78 + tint), static_cast<std::uint8_t>(178 + (tint / 2U)),
        static_cast<std::uint8_t>(86 + (tint / 3U)), 255);
    SDL_FRect shine{x + tile * 0.58f, y + tile * 0.18f, tile * 0.42f, tile * 0.18f};
    SDL_RenderFillRect(renderer, &shine);
}

void DrawTallGrass(SDL_Renderer* renderer, float x, float y, float tile, std::uint32_t hash) {
    SDL_SetRenderDrawColor(renderer, 50, 134, 61, 255);
    const float b0 = static_cast<float>((hash >> 3U) & 3U);
    const float b1 = static_cast<float>((hash >> 6U) & 3U);
    const float b2 = static_cast<float>((hash >> 9U) & 3U);
    SDL_FRect blade0{x + tile * 0.30f, y + tile * 0.28f - b0 * 0.12f, tile * 0.20f, tile * 0.78f};
    SDL_FRect blade1{x + tile * 0.62f, y + tile * 0.22f - b1 * 0.10f, tile * 0.22f, tile * 0.86f};
    SDL_FRect blade2{x + tile * 1.00f, y + tile * 0.30f - b2 * 0.08f, tile * 0.18f, tile * 0.72f};
    SDL_RenderFillRect(renderer, &blade0);
    SDL_RenderFillRect(renderer, &blade1);
    SDL_RenderFillRect(renderer, &blade2);
    SDL_SetRenderDrawColor(renderer, 90, 178, 88, 220);
    SDL_FRect tip0{x + tile * 0.32f, blade0.y, 1.0f, 1.0f};
    SDL_FRect tip1{x + tile * 0.66f, blade1.y, 1.0f, 1.0f};
    SDL_RenderFillRect(renderer, &tip0);
    SDL_RenderFillRect(renderer, &tip1);
}

void DrawFlowerPatch(SDL_Renderer* renderer, float x, float y, float tile, std::uint32_t hash) {
    SDL_SetRenderDrawColor(renderer, 62, 132, 64, 255);
    SDL_FRect stem{x + tile * 0.72f, y + tile * 0.62f, tile * 0.18f, tile * 0.46f};
    SDL_RenderFillRect(renderer, &stem);

    const std::uint8_t variant = static_cast<std::uint8_t>((hash >> 4U) % 3U);
    if (variant == 0) {
        SDL_SetRenderDrawColor(renderer, 239, 84, 124, 255);
    } else if (variant == 1) {
        SDL_SetRenderDrawColor(renderer, 255, 211, 83, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 133, 168, 255, 255);
    }

    SDL_FRect petalA{x + tile * 0.56f, y + tile * 0.46f, tile * 0.18f, tile * 0.18f};
    SDL_FRect petalB{x + tile * 0.92f, y + tile * 0.46f, tile * 0.18f, tile * 0.18f};
    SDL_FRect petalC{x + tile * 0.74f, y + tile * 0.32f, tile * 0.18f, tile * 0.18f};
    SDL_RenderFillRect(renderer, &petalA);
    SDL_RenderFillRect(renderer, &petalB);
    SDL_RenderFillRect(renderer, &petalC);
}

void DrawPropShadow(SDL_Renderer* renderer, float x, float y, float tile, PropType prop) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (prop == PropType::Tree) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 58);
        SDL_FRect trunkShadow{x + tile * 0.8f, y + tile * 0.12f, tile * 0.95f, tile * 0.42f};
        SDL_RenderFillRect(renderer, &trunkShadow);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 36);
        SDL_FRect crownShadow{x + tile * 1.4f, y - tile * 0.45f, tile * 2.2f, tile * 0.9f};
        SDL_RenderFillRect(renderer, &crownShadow);
    } else if (prop == PropType::Rock) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 46);
        SDL_FRect baseShadow{x + tile * 0.2f, y + tile * 0.95f, tile * 1.25f, tile * 0.26f};
        SDL_RenderFillRect(renderer, &baseShadow);
    } else if (prop == PropType::House) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
        SDL_FRect bodyShadow{x + tile * 1.2f, y - tile * 0.05f, tile * 2.4f, tile * 0.55f};
        SDL_RenderFillRect(renderer, &bodyShadow);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 34);
        SDL_FRect roofShadow{x + tile * 1.8f, y - tile * 0.55f, tile * 2.6f, tile * 0.45f};
        SDL_RenderFillRect(renderer, &roofShadow);
    } else if (prop == PropType::Bush || prop == PropType::TallGrass || prop == PropType::FlowerPatch) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 30);
        SDL_FRect lowShadow{x + tile * 0.40f, y + tile * 0.90f, tile * 1.1f, tile * 0.22f};
        SDL_RenderFillRect(renderer, &lowShadow);
    } else if (prop == PropType::AppleTree || prop == PropType::BerryTree || prop == PropType::PearTree) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 56);
        SDL_FRect trunkShadow{x + tile * 0.82f, y + tile * 0.12f, tile * 0.9f, tile * 0.42f};
        SDL_RenderFillRect(renderer, &trunkShadow);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 34);
        SDL_FRect crownShadow{x + tile * 1.25f, y - tile * 0.42f, tile * 2.1f, tile * 0.86f};
        SDL_RenderFillRect(renderer, &crownShadow);
    }
}

bool CanPlaceHouse(int x, int y) {
    const Terrain c0 = GetTerrainAt(x, y);
    const Terrain c1 = GetTerrainAt(x + 1, y);
    const Terrain c2 = GetTerrainAt(x, y + 1);
    const Terrain c3 = GetTerrainAt(x + 1, y + 1);

    return c0 != Terrain::Water && c1 != Terrain::Water && c2 != Terrain::Water && c3 != Terrain::Water &&
        c0 != Terrain::Sand && c1 != Terrain::Sand && c2 != Terrain::Sand && c3 != Terrain::Sand;
}

PropType GetPropAt(int x, int y, Terrain terrain) {
    const float decorRoll = Hash01(x, y, 777U);
    const float foliageRoll = Hash01(x, y, 1207U);
    const float flowerRoll = Hash01(x, y, 3331U);
    const float fruitRoll = Hash01(x, y, 6601U);

    if ((x % 23) == 0 && (y % 19) == 0 && CanPlaceHouse(x, y)) {
        return PropType::House;
    }
    if ((terrain == Terrain::Grass || terrain == Terrain::DarkGrass) && fruitRoll > 0.972f) {
        const std::uint8_t kind = static_cast<std::uint8_t>(Hash32(x, y, 9811U) % 3U);
        if (kind == 0U) {
            return PropType::AppleTree;
        }
        if (kind == 1U) {
            return PropType::BerryTree;
        }
        return PropType::PearTree;
    }
    if (terrain == Terrain::DarkGrass && decorRoll > 0.985f) {
        return PropType::Tree;
    }
    if ((terrain == Terrain::Grass || terrain == Terrain::Dirt) && decorRoll > 0.993f) {
        return PropType::Rock;
    }
    if (terrain == Terrain::DarkGrass && foliageRoll > 0.90f) {
        return PropType::Bush;
    }
    if (terrain == Terrain::Grass && foliageRoll > 0.92f) {
        return PropType::TallGrass;
    }
    if ((terrain == Terrain::Grass || terrain == Terrain::DarkGrass || terrain == Terrain::Sand) && flowerRoll > 0.965f) {
        return PropType::FlowerPatch;
    }
    return PropType::None;
}

float PropBaseY(int y, int tileSize, PropType propType) {
    const float tile = static_cast<float>(tileSize);
    (void)propType;
    return (static_cast<float>(y) * tile) + tile;
}

bool IsSoftGroundProp(PropType prop) {
    return prop == PropType::Bush || prop == PropType::TallGrass || prop == PropType::FlowerPatch;
}

bool OverlapsClearedGround(float x, float y, float w, float h, const std::vector<SDL_FRect>* clearedGround) {
    if (clearedGround == nullptr) {
        return false;
    }

    for (const SDL_FRect& rect : *clearedGround) {
        const bool overlap =
            x < (rect.x + rect.w) &&
            (x + w) > rect.x &&
            y < (rect.y + rect.h) &&
            (y + h) > rect.y;
        if (overlap) {
            return true;
        }
    }
    return false;
}
}

TileMap::TileMap(int tileSize) : tileSize_(tileSize) {}

long long TileMap::PropKey(int tx, int ty) {
    return (static_cast<long long>(tx) << 32) ^ static_cast<unsigned int>(ty);
}

int TileMap::WorldToTile(float worldPos, int tileSize) {
    return static_cast<int>(std::floor(worldPos / static_cast<float>(tileSize)));
}

float TileMap::MovementMultiplierAt(float worldX, float worldY, int currentLayer) const {
    const int tx = WorldToTile(worldX, tileSize_);
    const int ty = WorldToTile(worldY, tileSize_);

    const bool mountain = IsMountainTile(tx, ty);
    if (currentLayer == 1 && !mountain) {
        return 0.0f;
    }

    const Terrain terrain = GetTerrainAt(tx, ty);
    if (terrain == Terrain::Water) {
        return 0.55f;
    }
    if (terrain == Terrain::Sand) {
        return 0.78f;
    }
    return 1.0f;
}

bool TileMap::IsWaterAt(float worldX, float worldY) const {
    const int tx = WorldToTile(worldX, tileSize_);
    const int ty = WorldToTile(worldY, tileSize_);
    return GetTerrainAt(tx, ty) == Terrain::Water;
}

bool TileMap::IsBlockedAt(float worldX, float worldY, int currentLayer) const {
    const int tx = WorldToTile(worldX, tileSize_);
    const int ty = WorldToTile(worldY, tileSize_);

    const bool mountain = IsMountainTile(tx, ty);
    const bool ramp = IsMountainRampTile(tx, ty);
    if (currentLayer == 0) {
        if (mountain && !ramp) {
            return true;
        }
    } else {
        if (!mountain) {
            return true;
        }
    }

    const Terrain terrain = GetTerrainAt(tx, ty);
    const long long key = PropKey(tx, ty);
    const bool removed = removedProps_.find(key) != removedProps_.end();

    PropType prop = GetPropAt(tx, ty, terrain);
    if (removed && prop != PropType::House) {
        prop = PropType::None;
    }

    return prop == PropType::Tree || prop == PropType::Rock || prop == PropType::House || prop == PropType::AppleTree ||
        prop == PropType::BerryTree || prop == PropType::PearTree;
}

bool TileMap::HasBlockingObstacleInRect(float worldX, float worldY, float width, float height, int currentLayer) const {
    const int startX = WorldToTile(worldX, tileSize_);
    const int startY = WorldToTile(worldY, tileSize_);
    const int endX = WorldToTile(worldX + width - 0.01f, tileSize_);
    const int endY = WorldToTile(worldY + height - 0.01f, tileSize_);

    for (int ty = startY; ty <= endY; ++ty) {
        for (int tx = startX; tx <= endX; ++tx) {
            const float sampleX = (static_cast<float>(tx) + 0.5f) * static_cast<float>(tileSize_);
            const float sampleY = (static_cast<float>(ty) + 0.5f) * static_cast<float>(tileSize_);
            if (IsBlockedAt(sampleX, sampleY, currentLayer)) {
                return true;
            }
        }
    }
    return false;
}

bool TileMap::CanMoveLayer(float worldX, float worldY, int currentLayer, bool moveUp, int& outLayer) const {
    const int tx = WorldToTile(worldX, tileSize_);
    const int ty = WorldToTile(worldY, tileSize_);
    if (!IsMountainRampTile(tx, ty)) {
        return false;
    }

    if (moveUp && currentLayer == 0) {
        outLayer = 1;
        return true;
    }
    if (!moveUp && currentLayer == 1) {
        outLayer = 0;
        return true;
    }
    return false;
}

InteractionResult TileMap::InteractAt(float worldX, float worldY, float facingX, float* outHitX, float* outHitY) {
    const float attackX = worldX + ((facingX >= 0.0f ? 1.0f : -1.0f) * static_cast<float>(tileSize_) * 1.5f);
    const float attackY = worldY - static_cast<float>(tileSize_) * 0.3f;

    const int centerX = WorldToTile(attackX, tileSize_);
    const int centerY = WorldToTile(attackY, tileSize_);

    float nearestDistSq = 999999.0f;
    int nearestTx = 0;
    int nearestTy = 0;
    PropType nearestProp = PropType::None;

    for (int y = centerY - 2; y <= centerY + 2; ++y) {
        for (int x = centerX - 2; x <= centerX + 2; ++x) {
            const Terrain terrain = GetTerrainAt(x, y);
            if (terrain == Terrain::Water) {
                continue;
            }

            const long long key = PropKey(x, y);
            const bool removed = removedProps_.find(key) != removedProps_.end();
            PropType prop = GetPropAt(x, y, terrain);
            if (removed && prop != PropType::House) {
                prop = PropType::None;
            }
            if (prop == PropType::None) {
                continue;
            }

            const float dx = static_cast<float>(x - centerX);
            const float dy = static_cast<float>(y - centerY);
            const float distSq = (dx * dx) + (dy * dy);
            if (distSq > 6.25f) {
                continue;
            }

            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearestTx = x;
                nearestTy = y;
                nearestProp = prop;
            }
        }
    }

    if (nearestProp == PropType::Tree || nearestProp == PropType::Rock || nearestProp == PropType::Bush) {
        removedProps_.insert(PropKey(nearestTx, nearestTy));
        if (outHitX) {
            *outHitX = (static_cast<float>(nearestTx) + 0.5f) * static_cast<float>(tileSize_);
        }
        if (outHitY) {
            *outHitY = (static_cast<float>(nearestTy) + 1.0f) * static_cast<float>(tileSize_);
        }
        return nearestProp == PropType::Tree ? InteractionResult::ChoppedTree : InteractionResult::BrokeRock;
    }
    if (nearestProp == PropType::House) {
        return InteractionResult::EnteredHouse;
    }

    return InteractionResult::None;
}

HarvestResult TileMap::AttackAt(float worldX, float worldY, int currentLayer, float facingX, float* outHitX, float* outHitY) {
    const float attackX = worldX + ((facingX >= 0.0f ? 1.0f : -1.0f) * static_cast<float>(tileSize_) * 1.8f);
    const float attackY = worldY - static_cast<float>(tileSize_) * 0.35f;

    const int centerX = WorldToTile(attackX, tileSize_);
    const int centerY = WorldToTile(attackY, tileSize_);

    float nearestDistSq = 999999.0f;
    int nearestTx = 0;
    int nearestTy = 0;
    PropType nearestProp = PropType::None;

    for (int y = centerY - 2; y <= centerY + 2; ++y) {
        for (int x = centerX - 2; x <= centerX + 2; ++x) {
            const bool mountain = IsMountainTile(x, y);
            if ((currentLayer == 1 && !mountain) || (currentLayer == 0 && mountain && !IsMountainRampTile(x, y))) {
                continue;
            }

            const Terrain terrain = GetTerrainAt(x, y);
            const long long key = PropKey(x, y);
            const bool removed = removedProps_.find(key) != removedProps_.end();

            PropType prop = GetPropAt(x, y, terrain);
            if (removed && prop != PropType::House) {
                prop = PropType::None;
            }

            if (!(prop == PropType::AppleTree || prop == PropType::BerryTree || prop == PropType::PearTree)) {
                continue;
            }

            const float dx = static_cast<float>(x - centerX);
            const float dy = static_cast<float>(y - centerY);
            const float distSq = (dx * dx) + (dy * dy);
            if (distSq > 9.0f) {
                continue;
            }

            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                nearestTx = x;
                nearestTy = y;
                nearestProp = prop;
            }
        }
    }

    if (nearestProp == PropType::None) {
        return HarvestResult::None;
    }

    removedProps_.insert(PropKey(nearestTx, nearestTy));

    if (outHitX) {
        *outHitX = (static_cast<float>(nearestTx) + 0.5f) * static_cast<float>(tileSize_);
    }
    if (outHitY) {
        *outHitY = (static_cast<float>(nearestTy) + 1.0f) * static_cast<float>(tileSize_);
    }

    if (nearestProp == PropType::AppleTree) {
        return HarvestResult::Apple;
    }
    if (nearestProp == PropType::BerryTree) {
        return HarvestResult::Berry;
    }
    return HarvestResult::Pear;
}

void TileMap::DrawGround(SDL_Renderer* renderer, const Camera2D& camera, int currentLayer) const {
    const int startX = static_cast<int>(std::floor(camera.x / static_cast<float>(tileSize_))) - 1;
    const int startY = static_cast<int>(std::floor(camera.y / static_cast<float>(tileSize_))) - 1;
    const int endX = static_cast<int>(std::floor((camera.x + camera.width) / static_cast<float>(tileSize_))) + 2;
    const int endY = static_cast<int>(std::floor((camera.y + camera.height) / static_cast<float>(tileSize_))) + 2;

    const float tile = static_cast<float>(tileSize_);
    const float waterTime = static_cast<float>(SDL_GetTicks()) * 0.0016f;

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            const std::uint8_t variant = HashTile(x, y);
            const Terrain terrain = GetTerrainAt(x, y);
            const bool mountain = IsMountainTile(x, y);
            const bool cliffEdge = IsCliffEdgeTile(x, y);
            const bool ramp = IsMountainRampTile(x, y);

            if (currentLayer == 0) {
                if (mountain) {
                    SDL_SetRenderDrawColor(renderer, 92, 90, 102, 255);
                } else {
                    SetTerrainColor(renderer, terrain, variant);
                }
            } else {
                if (mountain) {
                    SDL_SetRenderDrawColor(renderer, 116, 127, 124, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 34, 40, 44, 255);
                }
            }

            SDL_FRect tileRect{
                static_cast<float>(x * tileSize_) - camera.x,
                static_cast<float>(y * tileSize_) - camera.y,
                tile,
                tile
            };
            SDL_RenderFillRect(renderer, &tileRect);

            const std::uint32_t tileHash = Hash32(x, y, 4242U);
            DrawGroundTexture(renderer, terrain, tileRect.x, tileRect.y, tile, tileHash, waterTime);

            if (currentLayer == 0 && mountain && cliffEdge && !ramp) {
                DrawCliffFace(renderer, tileRect.x, tileRect.y, tile);
            }

            if (ramp) {
                DrawRampMark(renderer, tileRect.x, tileRect.y, tile);
            }
        }
    }
}

void TileMap::DrawShadows(SDL_Renderer* renderer, const Camera2D& camera, int currentLayer, const std::vector<SDL_FRect>* clearedGround) const {
    const int startX = static_cast<int>(std::floor(camera.x / static_cast<float>(tileSize_))) - 3;
    const int startY = static_cast<int>(std::floor(camera.y / static_cast<float>(tileSize_))) - 5;
    const int endX = static_cast<int>(std::floor((camera.x + camera.width) / static_cast<float>(tileSize_))) + 4;
    const int endY = static_cast<int>(std::floor((camera.y + camera.height) / static_cast<float>(tileSize_))) + 4;

    const float tile = static_cast<float>(tileSize_);

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            const Terrain terrain = GetTerrainAt(x, y);
            if (terrain == Terrain::Water) {
                continue;
            }

            const bool mountain = IsMountainTile(x, y);
            if (currentLayer == 1 && !mountain) {
                continue;
            }
            if (currentLayer == 0 && mountain && !IsMountainRampTile(x, y)) {
                continue;
            }

            const long long key = PropKey(x, y);
            const bool removed = removedProps_.find(key) != removedProps_.end();

            PropType prop = GetPropAt(x, y, terrain);
            if (removed && prop != PropType::House) {
                prop = PropType::None;
            }
            if (prop == PropType::None) {
                continue;
            }

            if (IsSoftGroundProp(prop) &&
                OverlapsClearedGround(
                    static_cast<float>(x * tileSize_),
                    static_cast<float>(y * tileSize_),
                    tile * 2.0f,
                    tile * 2.0f,
                    clearedGround)) {
                continue;
            }

            const float drawX = static_cast<float>(x * tileSize_) - camera.x;
            const float drawY = static_cast<float>(y * tileSize_) - camera.y;
            DrawPropShadow(renderer, drawX, drawY + tile, tile, prop);
        }
    }
}

void TileMap::DrawProps(
    SDL_Renderer* renderer,
    const Camera2D& camera,
    float splitWorldY,
    bool drawBeforePlayer,
    int currentLayer,
    const std::vector<SDL_FRect>* clearedGround) const {
    const int startX = static_cast<int>(std::floor(camera.x / static_cast<float>(tileSize_))) - 2;
    const int startY = static_cast<int>(std::floor(camera.y / static_cast<float>(tileSize_))) - 4;
    const int endX = static_cast<int>(std::floor((camera.x + camera.width) / static_cast<float>(tileSize_))) + 3;
    const int endY = static_cast<int>(std::floor((camera.y + camera.height) / static_cast<float>(tileSize_))) + 3;

    const float tile = static_cast<float>(tileSize_);

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            const Terrain terrain = GetTerrainAt(x, y);
            if (terrain == Terrain::Water) {
                continue;
            }

            const bool mountain = IsMountainTile(x, y);
            if (currentLayer == 1 && !mountain) {
                continue;
            }
            if (currentLayer == 0 && mountain && !IsMountainRampTile(x, y)) {
                continue;
            }

            const long long key = PropKey(x, y);
            const bool removed = removedProps_.find(key) != removedProps_.end();

            PropType prop = GetPropAt(x, y, terrain);
            if (removed && prop != PropType::House) {
                prop = PropType::None;
            }
            if (prop == PropType::None) {
                continue;
            }

            if (IsSoftGroundProp(prop) &&
                OverlapsClearedGround(
                    static_cast<float>(x * tileSize_),
                    static_cast<float>(y * tileSize_),
                    tile * 2.0f,
                    tile * 2.0f,
                    clearedGround)) {
                continue;
            }

            const float baseY = PropBaseY(y, tileSize_, prop);
            const bool behind = baseY <= splitWorldY;
            if (behind != drawBeforePlayer) {
                continue;
            }

            const float drawX = static_cast<float>(x * tileSize_) - camera.x;
            const float drawY = static_cast<float>(y * tileSize_) - camera.y;

            if (prop == PropType::Tree) {
                DrawTree(renderer, drawX, drawY + tile, tile);
            } else if (prop == PropType::Rock) {
                DrawRock(renderer, drawX, drawY, tile);
            } else if (prop == PropType::House) {
                DrawHouse(renderer, drawX, drawY + tile, tile);
            } else if (prop == PropType::Bush) {
                DrawBush(renderer, drawX, drawY, tile, Hash32(x, y, 991U));
            } else if (prop == PropType::TallGrass) {
                DrawTallGrass(renderer, drawX, drawY, tile, Hash32(x, y, 1999U));
            } else if (prop == PropType::FlowerPatch) {
                DrawFlowerPatch(renderer, drawX, drawY, tile, Hash32(x, y, 42499U));
            } else if (prop == PropType::AppleTree || prop == PropType::BerryTree || prop == PropType::PearTree) {
                const int fruitTypeCode = (prop == PropType::AppleTree) ? 0 : ((prop == PropType::BerryTree) ? 1 : 2);
                DrawFruitTree(renderer, drawX, drawY + tile, tile, fruitTypeCode);
            }
        }
    }
}
