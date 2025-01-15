#pragma once

#include "deps/json.hpp"
#include <map>
#include <string>
#include <vector>
#include <string_view>
using json = nlohmann::json;


class Message {
public:
    std::string role;
    std::string content;

    Message() = default; // Add a default constructor
    Message(const std::string &role, const std::string &content) : role(role), content(content) {}
};
inline void to_json(json& j, const Message& m) {
    j = json{{"role", m.role}, {"content", m.content}};
}

inline void from_json(const json& j, Message& m) {
    j.at("role").get_to(m.role);
    j.at("content").get_to(m.content);
}

typedef std::vector<Message> Messages;
inline std::string toString(const Messages& messages, std::string_view delimiter = "\n") {
    if (messages.empty()) return {};

    const size_t total_size = std::transform_reduce(
        messages.begin(), messages.end(),
        delimiter.length() * (messages.size() - 1),
        std::plus{},
        [](const auto& msg) {
            return msg.role.length() + msg.content.length() + 13;
        }
    );

    std::string result;
    result.reserve(total_size);

    return std::accumulate(
        messages.begin() + 1, messages.end(),
        result + "Role: " + messages.front().role + ", Content: " + messages.front().content,
        [&delimiter](std::string acc, const auto& msg) {
            return std::move(acc).append(delimiter.data(), delimiter.length())
                   .append("Role: ").append(msg.role)
                   .append(", Content: ").append(msg.content);
        }
    );
}

class Request {
public:
    virtual ~Request() = default;
    virtual std::string getProviderName() const = 0;
    virtual json toJson() const = 0;
    virtual void fromJson(const json &j) = 0;
};

class OllamaRequest: public Request {
public:
    Messages messages;
    std::string model;
    bool stream;
    std::string format;
    std::map<std::string, json> options;

    OllamaRequest(Messages messages);
    std::string getProviderName() const override;
    json toJson() const override;
    void fromJson(const json &j) override;
};

class GroqRequest2: public Request {
public:
    Messages messages;
    std::string model;
    double temperature;
    int maxTokens = 512;
    std::vector<std::string> stop;

    GroqRequest2(Messages messages);
    std::string getProviderName() const override;
    json toJson() const override;
    void fromJson(const json &j) override;
};

class DifyRequest: public Request {
public:
    std::string response_mode = "blocking";
    json inputs = {};
    std::string user;
    std::string conversation_id;
    
};