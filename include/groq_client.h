#include "httplib.h"
#include "json.hpp"
#include <string>
#include <vector>
#include <chrono>

struct CompletionResponse {
    std::string content;
    double inference_time_ms;
};

class GroqClient {
public:
    GroqClient(const std::string& apiKey, const std::string& model = "gemma2-9b-it")
        : api_key(apiKey), model_name(model), client("api.groq.com", 443) {
            client.set_default_headers({{"Authorization", "Bearer " + api_key}});
            client.enable_server_certificate_verification(true);

        }

    CompletionResponse getChatCompletion(const std::string& user_message, int max_tokens = 256, double temperature = 0.7) {
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
        };
        nlohmann::json payload = {
            {"model", model_name},
            {"messages", {
                { {"role", "user"}, {"content", user_message} }
            }},
            // {"max_tokens", max_tokens},
            // {"temperature", temperature}
        };

        auto start_time = std::chrono::high_resolution_clock::now();
        auto res = client.Post("/openai/v1/chat/completions", headers, payload.dump(), "application/json");
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        if (res && res->status == 200) {
            auto response_json = nlohmann::json::parse(res->body);
            return CompletionResponse{
                response_json["choices"][0]["message"]["content"].get<std::string>(),
                static_cast<double>(duration.count())
            };
        } else {
            if (res) {
                throw std::runtime_error("HTTP Error: " + std::to_string(res->status) + "\n" + res->body);
            } else {
                throw std::runtime_error("Failed to connect to Groq API.");
            }
        }
    }

private:
    std::string api_key;
    std::string model_name;
    httplib::SSLClient client;
};
