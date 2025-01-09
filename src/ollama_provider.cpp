#include "provider/ollama_provider.h"
#include "provider/request.h"
#include "provider/response.h"
#include "utils/logger.h"
#include <deps/json.hpp>
#include <deps/httplib.h>

using json = nlohmann::json;

std::string OllamaProvider::getName() const  { return "Ollama"; }

void OllamaProvider::configure(const json &config)
{
    baseUrl = config.value("baseUrl", "http://localhost:11434");
    LOG_INFO("OllamaProvider configured with baseUrl: %s", baseUrl.c_str());
}

std::unique_ptr<Response> OllamaProvider::handleRequest(const std::unique_ptr<Request> &request)
{
    auto ollamaRequest = dynamic_cast<OllamaRequest *>(request.get());
    LOG_DEBUG("OllamaProvider: Received request: %s", ollamaRequest->toJson().dump().c_str());

    httplib::Client cli(baseUrl.c_str());
    httplib::Headers headers = {
        { "Content-Type", "application/json" }
    };

    auto res = cli.Post("/api/generate", headers, ollamaRequest->toJson().dump(), "application/json");
    if (res) {
        if (res->status == 200) {
            LOG_DEBUG("Ollama Response: %s", res->body.c_str());

            json responseJson = json::parse(res->body);
            auto response = std::make_unique<OllamaResponse>(
                responseJson.value("response", ""),
                responseJson.value("model", ""));

            response->createdAt = responseJson.value("created_at", "");
            response->done = responseJson.value("done", false);

            return response;
        } else {
            LOG_ERROR("Ollama Error: %d", res->status);
            json errorJson = json::parse(res->body);
            auto errorResponse = std::make_unique<OllamaResponse>("");
            errorResponse->error = errorJson.value("error", "Unknown error");
            return errorResponse;
        }
    } else {
        LOG_ERROR("Ollama Connection Error");
        auto errorResponse = std::make_unique<OllamaResponse>("");
        errorResponse->error = "Failed to connect to Ollama";
        return errorResponse;
    }
}

std::unique_ptr<Provider> OllamaProviderFactory::createProvider()
{
    LOG_DEBUG("Creating OllamaProvider");
    return std::make_unique<OllamaProvider>();
}

std::string OllamaRequest::getProviderName() const { return "Ollama"; }

OllamaRequest::OllamaRequest(Messages messages) : messages(messages) {}

json OllamaRequest::toJson() const
{
    json j;
    j["model"] = model;
    j["prompt"] = toString(messages);
    j["stream"] = stream;
    if (!format.empty()) {
        j["format"] = format;
    }
    if (!options.empty()) {
        j["options"] = options;
    }
    return j;
}

void OllamaRequest::fromJson(const json &j)
{
    if (j.contains("model")) {
        model = j["model"];
    }
    if (j.contains("prompt")) {
        messages = j["prompt"];
    }
    if (j.contains("stream")) {
        stream = j["stream"];
    }
    if (j.contains("format")) {
        format = j["format"];
    }
    if (j.contains("options")) {
        options = j["options"].get<std::map<std::string, json>>();
    }
}

OllamaResponse::OllamaResponse(const std::string &response) : response(response) {}

OllamaResponse::OllamaResponse(const std::string &response, const std::string &model) : response(response), model(model) {}

std::string OllamaResponse::toString() const
{
    return response;
}

json OllamaResponse::toJson() const
{
    json j;
    j["model"] = model;
    j["created_at"] = createdAt;
    j["response"] = response;
    j["done"] = done;
    j["error"] = error;

    return j;
}