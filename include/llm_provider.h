#include "httplib.h"
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using json = nlohmann::json;

class Provider;
class Request;
class Response;

class Request {
public:
    virtual ~Request() = default;
    virtual std::string getProviderName() const = 0;
    virtual json toJson() const = 0;
    virtual void fromJson(const json &j) = 0;
};

class Response {
public:
    virtual ~Response() = default;
    virtual std::string toString() const = 0;
    virtual json toJson() const = 0;
};

class Provider {
public:
    virtual ~Provider() = default;
    virtual std::string getName() const = 0;
    virtual std::unique_ptr<Response> handleRequest(const std::unique_ptr<Request> &request) = 0;
    virtual void configure(const json &config) = 0;
};

class ProviderFactory {
public:
    virtual ~ProviderFactory() = default;
    virtual std::unique_ptr<Provider> createProvider() = 0;
};

class Agent {
public:
    std::string id;
    json config;
    Agent(const std::string &agent_id, const json &cfg) :
        id(agent_id), config(cfg) { }

    virtual ~Agent() = default;
    virtual void think(const std::string &message) = 0;
    virtual void listen(const std::vector<int16_t> &audio_data) = 0;
    virtual void speak(std::function<void(const std::vector<int16_t> &)> &get_audio_callback) = 0;
    virtual void configure(const json &newConfig)
    {
        config.merge_patch(newConfig); // Use JSON merge patch to update
        LOG_DEBUG("Agent %s updated config: %s", id.c_str(), config.dump().c_str());
    }
};

class OllamaRequest: public Request {
public:
    std::string prompt;
    std::string model;
    bool stream = false;
    std::string format;
    std::map<std::string, json> options;

    OllamaRequest(const std::string &prompt, const std::string &model) :
        prompt(prompt), model(model) { }

    std::string getProviderName() const override { return "Ollama"; }

    json toJson() const override
    {
        json j;
        j["model"] = model;
        j["prompt"] = prompt;
        j["stream"] = stream;
        if (!format.empty()) {
            j["format"] = format;
        }
        if (!options.empty()) {
            j["options"] = options;
        }
        return j;
    }
    void fromJson(const json &j) override
    {
        if (j.contains("model")) {
            model = j["model"];
        }
        if (j.contains("prompt")) {
            prompt = j["prompt"];
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
};
class OllamaResponse: public Response {
public:
    std::string model;
    std::string createdAt;
    std::string response;
    bool done;

    std::string error;

    OllamaResponse(const std::string &response) :
        response(response) { }
    OllamaResponse(const std::string &response, const std::string &model) :
        response(response), model(model) { }

    std::string toString() const override
    {
        return response;
    }

    json toJson() const override
    {
        json j;
        j["model"] = model;
        j["created_at"] = createdAt;
        j["response"] = response;
        j["done"] = done;
        j["error"] = error;

        return j;
    }
};

class OllamaProvider: public Provider {
private:
    std::string baseUrl;

public:
    std::string getName() const override { return "Ollama"; }

    void configure(const json &config) override
    {
        baseUrl = config.value("baseUrl", "http://localhost:11434");
        LOG_INFO("OllamaProvider configured with baseUrl: %s", baseUrl.c_str());
    }

    std::unique_ptr<Response> handleRequest(const std::unique_ptr<Request> &request) override
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
};
class OllamaProviderFactory: public ProviderFactory {
public:
    std::unique_ptr<Provider> createProvider() override
    {
        LOG_DEBUG("Creating OllamaProvider");
        return std::make_unique<OllamaProvider>();
    }
};

class GroqRequest2: public Request {
public:
    std::vector<std::map<std::string, std::string>> messages;
    std::string model;
    double temperature = 0.7;
    int maxTokens = 2048;
    std::vector<std::string> stop;
    GroqRequest2() = default;
    GroqRequest2(const std::string &message)
    {
        messages.push_back({ { "role", "user" }, { "content", message } });
    }

    std::string getProviderName() const override { return "Groq"; }

    json toJson() const override
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
    void fromJson(const json &j) override
    {
        if (j.contains("messages")) {
            messages = j["messages"].get<std::vector<std::map<std::string, std::string>>>();
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
};

class GroqResponse2: public Response {
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

    GroqResponse2() = default;
    GroqResponse2(const std::string &error) :
        error(error) { }

    std::string toString() const override
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

    json toJson() const override
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
};

class GroqProvider: public Provider {
private:
    std::string apiKey;
    std::string baseUrl;

public:
    std::string getName() const override { return "Groq"; }

    void configure(const json &config) override
    {
        apiKey = config.value("apiKey", "");
        baseUrl = config.value("baseUrl", "https://api.groq.com/openai/v1");
        LOG_INFO("GroqProvider configured with baseUrl: %s", baseUrl.c_str());
    }

    std::unique_ptr<Response> handleRequest(const std::unique_ptr<Request> &request) override
    {
        auto groqRequest = dynamic_cast<GroqRequest2 *>(request.get());
        LOG_DEBUG("GroqProvider: Received request: %s", groqRequest->toJson().dump().c_str());

        httplib::SSLClient cli(baseUrl.c_str(), 443);
        cli.enable_server_certificate_verification(true);
        httplib::Headers headers = {
            { "Content-Type", "application/json" },
            { "Authorization", "Bearer " + apiKey }
        };

        auto res = cli.Post("/openai/v1/chat/completions", headers, groqRequest->toJson().dump(), "application/json");
        if (res) {
            if (res->status == 200) {
                LOG_DEBUG("Groq Response: %s", res->body.c_str());
                json responseJson = json::parse(res->body);
                auto response = std::make_unique<GroqResponse2>();

                response->id = responseJson.value("id", "");
                response->object = responseJson.value("object", "");
                response->created = responseJson.value("created", 0);
                response->model = responseJson.value("model", "");
                response->systemFingerprint = responseJson.value("system_fingerprint", "");

                if (responseJson.contains("choices") && responseJson["choices"].is_array()) {
                    for (const auto &choiceJson: responseJson["choices"]) {
                        GroqResponse2::Choice choice;
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
                return std::make_unique<GroqResponse2>(res->body);
            }
        } else {
            LOG_ERROR("Groq Connection Error");
            return std::make_unique<GroqResponse2>("Failed to connect to Groq");
        }
    }
};

class GroqProviderFactory: public ProviderFactory {
public:
    std::unique_ptr<Provider> createProvider() override
    {
        LOG_DEBUG("Creating GroqProvider");
        return std::make_unique<GroqProvider>();
    }
};

class RequestFactory {
public:
    virtual ~RequestFactory() = default;
    virtual std::unique_ptr<Request> createRequest(const std::string &message) = 0;
};

// Concrete Request Factories
class OllamaRequestFactory: public RequestFactory {
public:
    std::unique_ptr<Request> createRequest(const std::string &message) override
    {
        return std::make_unique<OllamaRequest>(message, ""); // Model will be set later
    }
};

class GroqRequestFactory: public RequestFactory {
public:
    std::unique_ptr<Request> createRequest(const std::string &message) override
    {
        return std::make_unique<GroqRequest2>(message);
    }
};

class ProviderManager {
private:
    std::map<std::string, std::unique_ptr<Provider>> providers;
    std::map<std::string, std::unique_ptr<RequestFactory>> requestFactories;
    std::map<std::string, std::unique_ptr<ProviderFactory>> providerFactories;
    static ProviderManager *instance;
    static std::mutex mutex;

    ProviderManager()
    {

        registerProviderFactory("Ollama", std::make_unique<OllamaProviderFactory>());
        registerProviderFactory("Groq", std::make_unique<GroqProviderFactory>());
        registerRequestFactory("Ollama", std::make_unique<OllamaRequestFactory>());
        registerRequestFactory("Groq", std::make_unique<GroqRequestFactory>());
    }

public:
    static ProviderManager *getInstance()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (instance == nullptr) {
            instance = new ProviderManager();
        }
        return instance;
    }

    void registerProviderFactory(const std::string &name, std::unique_ptr<ProviderFactory> factory)
    {
        providerFactories[name] = std::move(factory);
    }
    void registerRequestFactory(const std::string &name, std::unique_ptr<RequestFactory> factory)
    {
        requestFactories[name] = std::move(factory);
    }

    void loadConfig(const json &configData)
    {
        if (configData.contains("providers") && configData["providers"].is_object()) {
            for (auto &[providerName, providerConfig]: configData["providers"].items()) {
                if (providerFactories.count(providerName)) {
                    auto provider = providerFactories[providerName]->createProvider();
                    provider->configure(providerConfig);
                    providers[providerName] = std::move(provider);
                } else {
                    std::cerr << "Error: Unknown provider type '" << providerName << "' in config." << std::endl;
                }
            }
        }
    }

    std::unique_ptr<Response> processRequest(const std::unique_ptr<Request> &request)
    {
        std::string providerName = request->getProviderName();
        if (providers.count(providerName)) {
            return providers[providerName]->handleRequest(request);
        } else {
            std::cerr << "Error: Provider '" << providerName << "' not found." << std::endl;
            return nullptr;
        }
    }

    bool hasProvider(const std::string &providerName) const
    {
        return providers.count(providerName) > 0;
    }
    std::unique_ptr<Request> createRequest(const std::string &providerName, const std::string &message)
    {
        if (requestFactories.count(providerName)) {
            return requestFactories[providerName]->createRequest(message);
        }
        LOG_ERROR("No request factory found for provider: %s", providerName.c_str());
        return nullptr;
    }
    ProviderManager(const ProviderManager &) = delete;
    ProviderManager &operator=(const ProviderManager &) = delete;
};

class BaseAgent: public Agent {
public:
    BaseAgent(const std::string &agent_id, const json &cfg) :
        Agent(agent_id, cfg) { }

    void think(const std::string &message) override
    {
        LOG_DEBUG("BaseAgent %s thinks: %s", id.c_str(), message.c_str());

        // Get the provider name from the agent's configuration
        std::string providerName = config.value("provider", "");

        if (providerName.empty()) {
            LOG_ERROR("Agent %s does not have a provider specified in its configuration.", id.c_str());
            return;
        }

        // Check if the provider is available in the ProviderManager
        if (!ProviderManager::getInstance()->hasProvider(providerName)) {
            LOG_ERROR("Provider '%s' specified for agent %s is not available.", providerName.c_str(), id.c_str());
            return;
        }

        // Use ProviderManager to create the request
        std::unique_ptr<Request> request = ProviderManager::getInstance()->createRequest(providerName, message);

        if (!request) {
            LOG_ERROR("Failed to create a request for provider: %s", providerName.c_str());
            return;
        }

        // Apply provider-specific configurations from the agent's config
        if (config.contains(providerName)) {
            request->fromJson(config[providerName]);
        }
      

        // Process the request using the ProviderManager
        auto response = ProviderManager::getInstance()->processRequest(request);

        if (response) {
            LOG_INFO("Agent %s received response: %s", id.c_str(), response->toString().c_str());
        } else {
            LOG_ERROR("Agent %s did not receive a response.", id.c_str());
        }
    }

    void listen(const std::vector<int16_t> &audio_data) override
    {
        LOG_DEBUG("BaseAgent %s listens to audio data", id.c_str());
    }

    void speak(std::function<void(const std::vector<int16_t> &)> &get_audio_callback) override
    {
        LOG_DEBUG("BaseAgent %s speaks", id.c_str());
    }

    void configure(const json &newConfig) override
    {
        config.merge_patch(newConfig);
        LOG_DEBUG("Agent %s updated config: %s", id.c_str(), config.dump().c_str());
    }
};
class AgentManager {
public:
    std::shared_ptr<Agent> createAgent(const std::string &id,
                                       const std::string &type,
                                       const json &cfg)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (agents_.find(id) != agents_.end()) {
            // Return existing if found; or throw an error if you want to restrict duplicates
            return agents_[id];
        }

        std::shared_ptr<Agent> agentPtr;
        if (type == "BaseAgent") {
            agentPtr = std::make_shared<BaseAgent>(id, cfg);
        } else {
            // Default
            agentPtr = std::make_shared<BaseAgent>(id, cfg);
        }
        agents_[id] = agentPtr;
        return agentPtr;
    }

    std::shared_ptr<Agent> getAgent(const std::string &id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (agents_.find(id) != agents_.end()) {
            return agents_[id];
        }
        return nullptr;
    }

    // Method to update an agent's configuration
    bool updateAgentConfig(const std::string &agentId, const json &newConfig) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (agents_.find(agentId) != agents_.end()) {
            agents_[agentId]->configure(newConfig);
            return true;
        }
        return false;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Agent>> agents_;
    std::mutex mutex_;
};