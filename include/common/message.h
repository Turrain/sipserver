#pragma once
#include <deps/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

class Message {
public:
    std::string role;
    std::string content;

    Message() = default;
    Message(const std::string &role, const std::string &content) 
        : role(role), content(content) {}
};

inline void to_json(json &j, const Message &m) {
    j = json{{"role", m.role}, {"content", m.content}};
}

inline void from_json(const json &j, Message &m) {
    if (!j.is_object()) {
        throw json::type_error::create(302, "Message must be a JSON object", &j);
    }
    m.role = j.value("role", "");
    m.content = j.value("content", "");
}

using MessageList = std::vector<Message>;