# LoveDeathsAI

`LoveDeathsAI` is a small SDL3-based 2D sandbox/survival prototype with:

- wandering animals
- villager NPCs with chat and command handling
- hostile dark-faction enemies
- farming, fences, fishing, and harvesting
- bundled local AI runtime support for on-device NPC behavior

The project is currently focused on experimentation: world simulation, lightweight survival mechanics, and local AI agents running alongside the game.

## Current Features

- Top-down pixel-art world with layered terrain, props, water, and fog
- Villager NPCs with names, roles, traits, focus text, chat bubbles, and command states
- Hostiles such as zombies, marauders, ghouls, and wraiths
- Buildable fences that can block enemies and be destroyed
- Soil plots, seeds, crop growth, and simple farming loops
- Fishing rod flow with water casting
- Player chat box and speech bubble
- Bundled `llama.cpp` server + GGUF model support for local AI

## Controls

- `WASD` or arrow keys: move
- `Shift`: slow walk without changing facing
- `1`: blade
- `2`: fence
- `3`: soil
- `4`: seed
- `5`: fishing rod
- `J`: use current item / attack / place
- `E`: talk to nearby NPC or rotate fence while fence is selected
- `Enter`: open player chat, send message, and close chat
- `Ctrl+Z`: undo latest fence or soil placement
- `R` / `F`: move between terrain layers where allowed

## Build

This repo uses CMake and fetches SDL3 automatically.

### Windows

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

The game executable will be:

`build/Debug/My2DGame.exe`

## Run

Run:

```powershell
.\build\Debug\My2DGame.exe
```

If local AI assets are present, the game will try to start the bundled backend on launch.

## Local AI Runtime

This repo is set up to prefer a bundled `llama.cpp` server.

Expected layout:

```text
LoveDeathsAI/
  build/
  models/
    npc-brain.gguf
  third_party/
    llama-bin/
      llama-server.exe
      *.dll
```

Bundled assets already used by the project:

- model: [models/npc-brain.gguf](models/npc-brain.gguf)
- server: [third_party/llama-bin/llama-server.exe](third_party/llama-bin/llama-server.exe)

The game looks for these files relative to the executable and project root, then tries to launch the local backend automatically.

Extra notes are in [LOCAL_AI_SETUP.md](LOCAL_AI_SETUP.md).

## AI Chat Commands

The current command flow is:

1. Open player chat with `Enter`
2. Type an NPC name and a command
3. Send with `Enter`

Example:

```text
MARA FOLLOW ME
MARA STAY HERE
MARA GO HOME
```

The project currently applies core command behavior locally first, then uses the local model to generate NPC speech on top when available.

## Project Structure

```text
src/
  ai/
    AiRuntimeManager.*
    LocalLLMClient.*
  core/
    App.*
    Camera2D.*
  game/
    Animal.*
    HostileAI.*
    NpcAI.*
    Player.*
    TileMap.*
```

## Notes

- The `build/Debug/My2DGame.exe` file is often locked if the game is still open, which will cause `LNK1168` during rebuilds. Close the running game before building again.
- Some SDL helper steps may warn about `pwsh.exe` on Windows; the main game executable can still build successfully.
- The local AI side is still experimental and under active iteration.
