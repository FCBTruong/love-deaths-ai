#include "ai/LocalLLMClient.h"

#include <algorithm>
#include <cctype>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace {
struct ParsedUrl {
    std::wstring host;
    std::wstring path;
    int port;
    bool secure;
};

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

std::string WideToUtf8(const std::wstring& value) {
#if defined(_WIN32)
    if (value.empty()) {
        return std::string();
    }

    const int count = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), count, nullptr, nullptr);
    return out;
#else
    return std::string(value.begin(), value.end());
#endif
}

bool StartsWithNoCase(const std::string& value, const char* prefix) {
    const std::size_t prefixLen = std::char_traits<char>::length(prefix);
    if (value.size() < prefixLen) {
        return false;
    }

    for (std::size_t i = 0; i < prefixLen; ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

ParsedUrl ParseUrl(const std::string& baseUrl, const std::string& endpoint) {
    ParsedUrl parsed{};
    parsed.port = 80;
    parsed.secure = false;

    std::string working = baseUrl;
    if (StartsWithNoCase(working, "https://")) {
        parsed.secure = true;
        parsed.port = 443;
        working.erase(0, 8);
    } else if (StartsWithNoCase(working, "http://")) {
        working.erase(0, 7);
    }

    std::string hostPort = working;
    std::string basePath = "/";
    const std::size_t firstSlash = working.find('/');
    if (firstSlash != std::string::npos) {
        hostPort = working.substr(0, firstSlash);
        basePath = working.substr(firstSlash);
    }

    std::string host = hostPort;
    const std::size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos) {
        host = hostPort.substr(0, colon);
        parsed.port = std::stoi(hostPort.substr(colon + 1));
    }

    std::string path = endpoint.empty() ? basePath : endpoint;
    if (path.empty() || path.front() != '/') {
        path = "/" + path;
    }

    parsed.host = Utf8ToWide(host);
    parsed.path = Utf8ToWide(path);
    return parsed;
}
}

LocalLLMClient::LocalLLMClient(std::string baseUrl)
    : baseUrl_(std::move(baseUrl)) {}

void LocalLLMClient::SetBaseUrl(std::string baseUrl) {
    baseUrl_ = std::move(baseUrl);
}

const std::string& LocalLLMClient::BaseUrl() const {
    return baseUrl_;
}

LocalLLMClient::Response LocalLLMClient::Get(const std::string& endpoint, int timeoutMs) const {
    return Send("GET", endpoint, std::string(), timeoutMs);
}

LocalLLMClient::Response LocalLLMClient::PostJson(const Request& request) const {
    return Send("POST", request.endpoint, request.bodyJson, request.timeoutMs);
}

std::string LocalLLMClient::EscapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8U);
    for (char c : value) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(c);
            break;
        }
    }
    return out;
}

LocalLLMClient::Response LocalLLMClient::Send(
    const std::string& method,
    const std::string& endpoint,
    const std::string& bodyJson,
    int timeoutMs
) const {
    Response result{false, 0, std::string(), "Unsupported platform"};

#if defined(_WIN32)
    const ParsedUrl parsed = ParseUrl(baseUrl_, endpoint);
    const std::wstring wideMethod = Utf8ToWide(method);
    const std::wstring userAgent = L"LoveDeathsAI/1.0";

    HINTERNET session = WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        result.error = "WinHttpOpen failed";
        return result;
    }

    WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    HINTERNET connect =
        WinHttpConnect(session, parsed.host.c_str(), static_cast<INTERNET_PORT>(parsed.port), 0);
    if (connect == nullptr) {
        result.error = "WinHttpConnect failed";
        WinHttpCloseHandle(session);
        return result;
    }

    const DWORD flags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, wideMethod.c_str(), parsed.path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (request == nullptr) {
        result.error = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return result;
    }

    std::wstring headers;
    const void* bodyPtr = WINHTTP_NO_REQUEST_DATA;
    DWORD bodyBytes = 0;
    if (!bodyJson.empty()) {
        headers = L"Content-Type: application/json\r\n";
        bodyPtr = bodyJson.data();
        bodyBytes = static_cast<DWORD>(bodyJson.size());
    }

    const BOOL sent = WinHttpSendRequest(request, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                                         headers.empty() ? 0 : static_cast<DWORD>(headers.size()), const_cast<void*>(bodyPtr),
                                         bodyBytes, bodyBytes, 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        result.error = "HTTP request failed";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    result.statusCode = static_cast<int>(statusCode);

    std::string responseBody;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) {
            break;
        }

        std::string buffer(static_cast<std::size_t>(available), '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &bytesRead) || bytesRead == 0) {
            break;
        }
        buffer.resize(static_cast<std::size_t>(bytesRead));
        responseBody += buffer;
    }

    result.ok = result.statusCode >= 200 && result.statusCode < 300;
    result.body = std::move(responseBody);
    if (!result.ok && result.error.empty()) {
        result.error = "HTTP " + std::to_string(result.statusCode);
    } else if (result.ok) {
        result.error.clear();
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
#endif

    return result;
}
