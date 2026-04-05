#include "ai/AiRuntimeManager.h"

#include <filesystem>
#include <string>

#include "ai/LocalLLMClient.h"

namespace {
std::wstring Utf8ToWide(const std::string& value) {
#if defined(_WIN32)
    if (value.empty()) {
        return std::wstring();
    }

    const int count = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), count);
    return out;
#else
    return std::wstring(value.begin(), value.end());
#endif
}
}

AiRuntimeManager::AiRuntimeManager()
    : config_{Backend::None, std::string(), "gemma3:4b", std::string(), "http://127.0.0.1:11434", 11434, false},
      lastError_()
#if defined(_WIN32)
      ,
      processInfo_{},
      processRunning_(false)
#endif
{
}

AiRuntimeManager::~AiRuntimeManager() {
    Stop();
}

void AiRuntimeManager::SetConfig(Config config) {
    config_ = std::move(config);
}

const AiRuntimeManager::Config& AiRuntimeManager::GetConfig() const {
    return config_;
}

bool AiRuntimeManager::Start() {
    lastError_.clear();
    if (config_.backend == Backend::None) {
        lastError_ = "AI backend is disabled";
        return false;
    }

    if (ProbeReady()) {
        return true;
    }

    if (config_.backend == Backend::Ollama) {
        return StartOllama();
    }
    if (config_.backend == Backend::LlamaServer) {
        return StartLlamaServer();
    }

    lastError_ = "Unknown backend";
    return false;
}

void AiRuntimeManager::Stop() {
#if defined(_WIN32)
    if (!processRunning_) {
        return;
    }

    TerminateProcess(processInfo_.hProcess, 0);
    CloseHandle(processInfo_.hThread);
    CloseHandle(processInfo_.hProcess);
    processInfo_ = {};
    processRunning_ = false;
#endif
}

bool AiRuntimeManager::IsReady() const {
    return ProbeReady();
}

const std::string& AiRuntimeManager::LastError() const {
    return lastError_;
}

bool AiRuntimeManager::StartOllama() {
#if defined(_WIN32)
    std::wstring executable = config_.executablePath.empty() ? L"ollama.exe" : Utf8ToWide(config_.executablePath);
    std::wstring commandLine = executable + L" serve";

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo{};
    std::wstring mutableCommand = commandLine;
    const BOOL ok = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                                   &startupInfo, &processInfo);
    if (!ok) {
        lastError_ = "Could not launch ollama serve";
        return false;
    }

    processInfo_ = processInfo;
    processRunning_ = true;
    return true;
#else
    lastError_ = "Unsupported platform";
    return false;
#endif
}

bool AiRuntimeManager::StartLlamaServer() {
#if defined(_WIN32)
    if (config_.executablePath.empty() || config_.modelPath.empty()) {
        lastError_ = "llama.cpp requires executablePath and modelPath";
        return false;
    }
    if (!std::filesystem::exists(config_.executablePath)) {
        lastError_ = "llama-server executable was not found";
        return false;
    }
    if (!std::filesystem::exists(config_.modelPath)) {
        lastError_ = "Model file was not found";
        return false;
    }

    const std::wstring executable = Utf8ToWide(config_.executablePath);
    const std::wstring modelPath = Utf8ToWide(config_.modelPath);
    std::wstring commandLine = L"\"" + executable + L"\" -m \"" + modelPath + L"\" --port " +
                               Utf8ToWide(std::to_string(config_.port)) + L" --ctx-size 4096";
    const std::wstring workingDir = Utf8ToWide(std::filesystem::path(config_.executablePath).parent_path().string());

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo{};
    std::wstring mutableCommand = commandLine;
    const BOOL ok = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                                   workingDir.empty() ? nullptr : workingDir.c_str(),
                                   &startupInfo, &processInfo);
    if (!ok) {
        lastError_ = "Could not launch llama-server";
        return false;
    }

    processInfo_ = processInfo;
    processRunning_ = true;
    return true;
#else
    lastError_ = "Unsupported platform";
    return false;
#endif
}

bool AiRuntimeManager::ProbeReady() const {
    LocalLLMClient client(config_.baseUrl);
    const LocalLLMClient::Response response =
        client.Get(config_.backend == Backend::Ollama ? "/api/tags" : "/health", 1200);
    return response.ok;
}
