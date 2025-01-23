#pragma once
#include <deps/json.hpp>
#include <string>

using json = nlohmann::json;

class Message {
public:
    std::string role;
    std::string content;

    Message() = default;
    Message(const std::string& role, const std::string& content) :
        role(role), content(content) { }
};

inline void to_json(json& j, const Message& m) {
    j = json { { "role", m.role }, { "content", m.content } };
}

inline void from_json(const json& j, Message& m) {
    j.at("role").get_to(m.role);
    j.at("content").get_to(m.content);
}
