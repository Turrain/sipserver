#pragma once

#include "deps/json.hpp"
#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

class Response {
public:
    virtual ~Response() = default;
    virtual std::string toString() const = 0;
    virtual json toJson() const = 0;
};

class OllamaResponse: public Response {
public:
    std::string model;
    std::string createdAt;
    std::string response;
    bool done;
    std::string error;

    OllamaResponse(const std::string &response);
    OllamaResponse(const std::string &response, const std::string &model);
    std::string toString() const override;
    json toJson() const override;
};

class GroqResponse: public Response {
public:
    std::string id;
    std::string object;
    int created;
    std::string model;
    struct Choice {
        int index;
        std::map<std::string, std::string> message;
        std::string finishReason;
    };
    std::vector<Choice> choices;
    std::map<std::string, int> usage;
    std::string systemFingerprint;

    std::string error;

    GroqResponse();
    GroqResponse(const std::string &error);
    std::string toString() const override;
    json toJson() const override;
};

