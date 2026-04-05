#pragma once

#include <string>

class LocalLLMClient {
public:
    struct Request {
        std::string endpoint;
        std::string bodyJson;
        int timeoutMs;
    };

    struct Response {
        bool ok;
        int statusCode;
        std::string body;
        std::string error;
    };

    explicit LocalLLMClient(std::string baseUrl = "http://127.0.0.1:11434");

    void SetBaseUrl(std::string baseUrl);
    const std::string& BaseUrl() const;

    Response Get(const std::string& endpoint, int timeoutMs = 3000) const;
    Response PostJson(const Request& request) const;

    static std::string EscapeJson(const std::string& value);

private:
    Response Send(const std::string& method, const std::string& endpoint, const std::string& bodyJson, int timeoutMs) const;

    std::string baseUrl_;
};
