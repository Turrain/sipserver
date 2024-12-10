#pragma once

#include <json.hpp>
#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

class Request {
public:
    virtual ~Request() = default;
    virtual std::string getProviderName() const = 0;
    virtual json toJson() const = 0;
    virtual void fromJson(const json &j) = 0;
};

class OllamaRequest: public Request {
public:
    std::string prompt;
    std::string model;
    bool stream;
    std::string format;
    std::map<std::string, json> options;

    OllamaRequest(const std::string &prompt, const std::string &model);
    std::string getProviderName() const override;
    json toJson() const override;
    void fromJson(const json &j) override;
};

class GroqRequest2: public Request {
public:
    std::vector<std::map<std::string, std::string>> messages;
    std::string model;
    double temperature;
    int maxTokens;
    std::vector<std::string> stop;

    GroqRequest2();
    GroqRequest2(const std::string &message);
    std::string getProviderName() const override;
    json toJson() const override;
    void fromJson(const json &j) override;
};