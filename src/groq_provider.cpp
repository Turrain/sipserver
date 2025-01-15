#include "provider/groq_provider.h"
#include "provider/request.h"
#include "provider/response.h"
#include "utils/logger.h"
#include <deps/json.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <deps/httplib.h>

using json = nlohmann::json;

std::string GroqProvider::getName() const { return "Groq"; }

void GroqProvider::configure(const json &config)
{
    apiKey = config.value("apiKey", "");
    baseUrl = config.value("baseUrl", "api.groq.com");
    LOG_INFO("GroqProvider configured with baseUrl: %s", baseUrl.c_str());
}

std::unique_ptr<Response> GroqProvider::handleRequest(const std::unique_ptr<Request> &request)
{
    auto groqRequest = dynamic_cast<GroqRequest2 *>(request.get());
    LOG_DEBUG("GroqProvider: Received request: %s", groqRequest->toJson().dump().c_str());

    httplib::SSLClient cli(baseUrl.c_str(), 443);
    cli.enable_server_certificate_verification(true);
    httplib::Headers headers = {
        { "Content-Type", "application/json" },
        { "Authorization", "Bearer " + apiKey }
    };
    auto json_dump = groqRequest->toJson().dump();
    auto res = cli.Post("/openai/v1/chat/completions", headers, json_dump, "application/json");
    if (res) {
        if (res->status == 200) {
            LOG_DEBUG("Groq Response: %s", res->body.c_str());
            json responseJson = json::parse(res->body);
            auto response = std::make_unique<GroqResponse>();

            response->id = responseJson.value("id", "");
            response->object = responseJson.value("object", "");
            response->created = responseJson.value("created", 0);
            response->model = responseJson.value("model", "");
            response->systemFingerprint = responseJson.value("system_fingerprint", "");

            if (responseJson.contains("choices") && responseJson["choices"].is_array()) {
                for (const auto &choiceJson: responseJson["choices"]) {
                    GroqResponse::Choice choice;
                    choice.index = choiceJson.value("index", 0);
                    if (choiceJson.contains("message") && choiceJson["message"].is_object()) {
                        for (const auto &[key, value]: choiceJson["message"].items()) {
                            choice.message[key] = value;
                        }
                    }
                    choice.finishReason = choiceJson.value("finish_reason", "");
                    response->choices.push_back(choice);
                }
            }

            if (responseJson.contains("usage") && responseJson["usage"].is_object()) {
                for (const auto &[key, value]: responseJson["usage"].items()) {
                    response->usage[key] = value;
                }
            }

            return response;
        } else {
            LOG_ERROR("Groq Error: %d", res->status);
            return std::make_unique<GroqResponse>(res->body);
        }
    } else {
        LOG_ERROR("Groq Connection Error");
        return std::make_unique<GroqResponse>("Failed to connect to Groq");
    }
}

std::unique_ptr<Provider> GroqProviderFactory::createProvider()
{
    LOG_DEBUG("Creating GroqProvider");
    return std::make_unique<GroqProvider>();
}

std::string GroqRequest2::getProviderName() const { return "Groq"; }

GroqRequest2::GroqRequest2(Messages messages)
{
    this->messages = messages;
}

json GroqRequest2::toJson() const
{
    json j;
    j["messages"] = messages;
    j["temperature"] = temperature;
    j["max_tokens"] = maxTokens;
    if (!model.empty()) {
        j["model"] = model;
    }
    if (!stop.empty()) {
        j["stop"] = stop;
    }
    return j;
}

void GroqRequest2::fromJson(const json &j)
{
    if (j.contains("messages")) {
        messages = j["messages"].get<Messages>();
    }
    if (j.contains("temperature")) {
        temperature = j["temperature"];
    }
    if (j.contains("max_tokens")) {
        maxTokens = j["max_tokens"];
    }
    if (j.contains("model")) {
        model = j["model"];
    }
    if (j.contains("stop")) {
        stop = j["stop"].get<std::vector<std::string>>();
    }
}

GroqResponse::GroqResponse() = default;

GroqResponse::GroqResponse(const std::string &error) :
    error(error) { }

std::string GroqResponse::toString() const
{
    if (!error.empty()) {
        return "Error: " + error;
    }
    std::string result;
    for (const auto &choice: choices) {
        result += choice.message.at("content") + "\n";
    }
    return result;
}

json GroqResponse::toJson() const
{
    json j;
    if (!error.empty()) {
        j["error"] = error;
        return j;
    }

    j["id"] = id;
    j["object"] = object;
    j["created"] = created;
    j["model"] = model;
    j["system_fingerprint"] = systemFingerprint;

    j["choices"] = json::array();
    for (const auto &choice: choices) {
        json choiceJson;
        choiceJson["index"] = choice.index;
        choiceJson["message"] = choice.message;
        choiceJson["finish_reason"] = choice.finishReason;
        j["choices"].push_back(choiceJson);
    }

    j["usage"] = usage;
    return j;
}