#pragma once

#include <string>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

class AiRuntimeManager {
public:
    enum class Backend {
        None,
        Ollama,
        LlamaServer
    };

    struct Config {
        Backend backend;
        std::string executablePath;
        std::string modelName;
        std::string modelPath;
        std::string baseUrl;
        int port;
        bool autoStart;
    };

    AiRuntimeManager();
    ~AiRuntimeManager();

    void SetConfig(Config config);
    const Config& GetConfig() const;

    bool Start();
    void Stop();
    bool IsReady() const;

    const std::string& LastError() const;

private:
    bool StartOllama();
    bool StartLlamaServer();
    bool ProbeReady() const;

    Config config_;
    std::string lastError_;

#if defined(_WIN32)
    PROCESS_INFORMATION processInfo_;
    bool processRunning_;
#endif
};
