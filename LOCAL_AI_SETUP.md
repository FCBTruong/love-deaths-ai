# Local AI Setup

This project now includes a local AI runtime scaffold so the game can start or talk to an on-device model backend.

## Recommended backend

Start with `Ollama` for quick iteration, then move to bundled `llama.cpp` if you want to ship the model with the game.

## Folder layout for bundled llama.cpp

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

## Option A: Ollama

1. Install Ollama on Windows.
2. Pull a model:
   - `ollama pull gemma3:4b`
   - or `ollama pull qwen2.5:3b`
3. Run:
   - `ollama serve`
4. The default local API is:
   - `http://127.0.0.1:11434`

## Option B: llama.cpp bundled with the game

1. Put `llama-server.exe` in `third_party/`.
2. Put a `.gguf` model in `models/`.
3. Configure `AiRuntimeManager::Config` in the game to:
   - `backend = LlamaServer`
   - `executablePath = third_party/llama-bin/llama-server.exe`
   - `modelPath = models/npc-brain.gguf`
   - `baseUrl = http://127.0.0.1:8080`
   - `port = 8080`

## Suggested first integration point

1. Create one shared runtime manager for the whole game.
2. Create one shared `LocalLLMClient`.
3. Let nearby NPCs think every 2-4 seconds.
4. Let distant NPCs think every 6-10 seconds.
5. Never block the render thread waiting on the model.

## First JSON contract

```json
{
  "thought": "I trust the player enough to stay nearby tonight.",
  "speech": "I will follow, but stay alert.",
  "intent": "follow",
  "target": "player",
  "priority": 0.82
}
```
