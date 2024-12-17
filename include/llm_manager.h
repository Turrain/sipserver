#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "httplib.h"
#include "json.hpp"

//------------------------------------------------------------------------------
// Base abstractions for request/response, including optional streaming support
//------------------------------------------------------------------------------
class LLMRequest {
public:
    virtual ~LLMRequest() = default;
};

class LLMResponse {
public:
    virtual ~LLMResponse() = default;
};

using TokenCallback = std::function<bool(const std::string &token)>;

//------------------------------------------------------------------------------
// Ollama-specific request/response
// Ollama provides /api/generate and /api/chat. We'll support both.
//
// /api/generate: single prompt -> single completion
// /api/chat: conversation with a message array similar to OpenAI chat.
class OllamaRequest: public LLMRequest {
public:
    std::string model = "gemma2:9b";
    double temperature = 0.7;
    int maxTokens = 256;
    bool stream = false;
};

class OllamaGenerateRequest: public OllamaRequest {
public:
    std::string prompt;
};

class OllamaChatRequest: public OllamaRequest {
public:
    struct Message {
        std::string role; // "system", "user", "assistant"
        std::string content;
    };
    std::vector<Message> messages;
};

class OllamaResponse: public LLMResponse {
public:
    std::string generatedText;
};

//------------------------------------------------------------------------------
// Groq-specific request/response

class GroqRequest: public LLMRequest {
public:
    // Required Fields
    std::string model; // ID of the model to use
    struct Message {
        std::string role;
        std::string content;
    };
    std::vector<Message>
        messages; // List of messages comprising the conversation

    // Optional Fields
    std::optional<double> frequency_penalty; // Between -2.0 and 2.0
    std::optional<std::string>
        function_call; // Deprecated in favor of tool_choice
    std::optional<std::vector<std::string>>
        functions; // Deprecated in favor of tools
    std::optional<std::map<std::string, double>> logit_bias; // Not supported yet
    std::optional<bool> logprobs; // Defaults to false
    std::optional<int> max_tokens; // Max tokens to generate
    std::optional<int> n; // Defaults to 1
    std::optional<bool> parallel_tool_calls; // Defaults to true
    std::optional<double> presence_penalty; // Between -2.0 and 2.0
    std::optional<std::map<std::string, std::string>>
        response_format; // e.g., {"type": "json_object"}
    std::optional<int> seed; // For deterministic results
    std::optional<std::variant<std::string, std::vector<std::string>>>
        stop; // Stop sequences
    std::optional<bool> stream = false; // Defaults to false
    std::optional<std::map<std::string, std::string>>
        stream_options; // Only if stream is true
    std::optional<double> temperature; // Sampling temperature between 0 and 2
    std::optional<std::variant<std::string, std::map<std::string, std::string>>>
        tool_choice; // Controls tool usage
    std::optional<std::vector<std::string>>
        tools; // List of tools (functions) supported
    std::optional<int> top_logprobs; // Not supported yet
    std::optional<double> top_p; // Defaults to 1
    std::optional<std::string> user; // Unique end-user identifier
};

class GroqResponse: public LLMResponse {
public:
    std::string id;
    std::string object;
    long created;
    std::string model;

    // Nested Choice with Message
    struct Choice {
        int index;
        struct Message {
            std::string role;
            std::string content;
        } message;
        std::string finish_reason;
    };
    std::vector<Choice> choices;

    // Usage information
    struct Usage {
        double queue_time;
        int prompt_tokens;
        double prompt_time;
        int completion_tokens;
        double completion_time;
        int total_tokens;
        double total_time;
    } usage;

    // XGroq metadata
    struct XGroq {
        std::string id;
    } x_groq;

    std::string system_fingerprint;
};
// -----------------------------------------------------------------------------
// GroqRequest Serialization
// -----------------------------------------------------------------------------
namespace nlohmann {
// Serialization for GroqRequest::Message
inline void to_json(json &j, const GroqRequest::Message &m)
{
    j = json { { "role", m.role }, { "content", m.content } };
}

// Serialization for GroqRequest
inline void to_json(json &j, const GroqRequest &r)
{
    j = json { { "model", r.model }, { "messages", r.messages } };

    auto set_if = [&](const std::string &key, const auto &opt) {
        if (opt.has_value()) {
            j[key] = *opt;
        }
    };

    // Handle optional fields only if set
    set_if("frequency_penalty", r.frequency_penalty);
    set_if("function_call", r.function_call);
    set_if("functions", r.functions);
    set_if("logit_bias", r.logit_bias);
    set_if("logprobs", r.logprobs);
    set_if("max_tokens", r.max_tokens);
    set_if("n", r.n);
    set_if("parallel_tool_calls", r.parallel_tool_calls);
    set_if("presence_penalty", r.presence_penalty);
    set_if("response_format", r.response_format);
    set_if("seed", r.seed);

    // 'stop' is a variant<string, vector<string>>
    if (r.stop.has_value()) {
        if (std::holds_alternative<std::string>(*r.stop)) {
            j["stop"] = std::get<std::string>(*r.stop);
        } else {
            j["stop"] = std::get<std::vector<std::string>>(*r.stop);
        }
    }

    set_if("stream", r.stream);
    set_if("stream_options", r.stream_options);
    set_if("temperature", r.temperature);

    // 'tool_choice' is a variant<string, map<string,string>>
    if (r.tool_choice.has_value()) {
        if (std::holds_alternative<std::string>(*r.tool_choice)) {
            j["tool_choice"] = std::get<std::string>(*r.tool_choice);
        } else {
            j["tool_choice"] = std::get<std::map<std::string, std::string>>(*r.tool_choice);
        }
    }

    set_if("tools", r.tools);
    set_if("top_logprobs", r.top_logprobs);
    set_if("top_p", r.top_p);
    set_if("user", r.user);
}

// Deserialization for GroqResponse::Choice::Message
inline void from_json(const json &j, GroqResponse::Choice::Message &m)
{
    m.role = j.value("role", "");
    m.content = j.value("content", "");
}

// Deserialization for GroqResponse::Choice
inline void from_json(const json &j, GroqResponse::Choice &c)
{
    c.index = j.value("index", 0);
    c.message = j.value("message", GroqResponse::Choice::Message {});
    c.finish_reason = j.value("finish_reason", "");
}

// Deserialization for GroqResponse::Usage
inline void from_json(const json &j, GroqResponse::Usage &u)
{
    u.queue_time = j.value("queue_time", 0.0);
    u.prompt_tokens = j.value("prompt_tokens", 0);
    u.prompt_time = j.value("prompt_time", 0.0);
    u.completion_tokens = j.value("completion_tokens", 0);
    u.completion_time = j.value("completion_time", 0.0);
    u.total_tokens = j.value("total_tokens", 0);
    u.total_time = j.value("total_time", 0.0);
}

// Deserialization for GroqResponse::XGroq
inline void from_json(const json &j, GroqResponse::XGroq &x)
{
    x.id = j.value("id", "");
}

// Deserialization for GroqResponse
inline void from_json(const json &j, GroqResponse &r)
{
    r.id = j.value("id", "");
    r.object = j.value("object", "");
    r.created = j.value("created", 0L);
    r.model = j.value("model", "");
    r.choices = j.value("choices", std::vector<GroqResponse::Choice>());
    r.usage = j.value("usage", GroqResponse::Usage {});
    r.x_groq = j.value("x_groq", GroqResponse::XGroq {});
    r.system_fingerprint = j.value("system_fingerprint", "");
}
} // namespace nlohmann

//------------------------------------------------------------------------------
// Abstract client interface
//------------------------------------------------------------------------------
class LLMClient {
public:
    virtual ~LLMClient() = default;
    virtual std::unique_ptr<LLMResponse> generateResponse(
        const LLMRequest &request)
        = 0;
    virtual std::unique_ptr<LLMResponse> generateResponseStream(
        const LLMRequest &request, TokenCallback tokenCallback)
    {
        // Default: no streaming optimization
        auto resp = generateResponse(request);
        return resp;
    }
};

//------------------------------------------------------------------------------
// Ollama Client Implementation (supports /api/generate and /api/chat)
//------------------------------------------------------------------------------

class OllamaLLMClient: public LLMClient {
public:
    OllamaLLMClient() = default;

    std::unique_ptr<LLMResponse> generateResponse(
        const LLMRequest &request) override
    {
        // Decide endpoint by request type
        if (auto genReq = dynamic_cast<const OllamaGenerateRequest *>(&request)) {
            return generateResponseGenerate(*genReq);
        } else if (auto chatReq = dynamic_cast<const OllamaChatRequest *>(&request)) {
            return generateResponseChat(*chatReq);
        } else {
            throw std::runtime_error("Ollama request type not supported");
        }
    }

    std::unique_ptr<LLMResponse> generateResponseStream(
        const LLMRequest &request, TokenCallback tokenCallback) override
    {
        // Decide endpoint by request type
        if (auto genReq = dynamic_cast<const OllamaGenerateRequest *>(&request)) {
            return generateResponseGenerateStream(*genReq, tokenCallback);
        } else if (auto chatReq = dynamic_cast<const OllamaChatRequest *>(&request)) {
            return generateResponseChatStream(*chatReq, tokenCallback);
        } else {
            throw std::runtime_error(
                "Ollama request type not supported for streaming");
        }
    }

private:
    // /api/generate (single prompt)
    std::unique_ptr<LLMResponse> generateResponseGenerate(
        const OllamaGenerateRequest &req)
    {
        nlohmann::json body;
        body["model"] = req.model;
        body["prompt"] = req.prompt;
        body["temperature"] = req.temperature;
        body["max_tokens"] = req.maxTokens;

        httplib::Client cli("localhost", 11437);
        auto res = cli.Post("/api/generate", body.dump(), "application/json");
        if (!res || res->status != 200) {
            throw std::runtime_error("Ollama generate request failed");
        }

        auto jsonResp = nlohmann::json::parse(res->body);
        auto response = std::make_unique<OllamaResponse>();
        response->generatedText = jsonResp.value("completion", "");
        return response;
    }

    std::unique_ptr<LLMResponse> generateResponseGenerateStream(
        const OllamaGenerateRequest &req, TokenCallback tokenCallback)
    {
        nlohmann::json body;
        body["model"] = req.model;
        body["prompt"] = req.prompt;
        body["temperature"] = req.temperature;
        body["max_tokens"] = req.maxTokens;

        httplib::Client cli("localhost", 11437);
        std::string finalText;
        // auto res = cli.Post(
        //     "/api/generate",
        //     body.dump(),
        //     "application/json",
        //     [&](const char* data, size_t length) {
        //         std::string chunk(data, length);
        //         auto j = nlohmann::json::parse(chunk, nullptr, false);
        //         if (!j.is_discarded() && j.contains("token")) {
        //             std::string token = j["token"].get<std::string>();
        //             finalText += token;
        //             if (!tokenCallback(token)) {
        //                 return false;
        //             }
        //         }
        //         return true;
        //     }
        // );

        auto response = std::make_unique<OllamaResponse>();
        response->generatedText = finalText;
        return response;
    }

    // /api/chat (conversation with messages)
    std::unique_ptr<LLMResponse> generateResponseChat(
        const OllamaChatRequest &req)
    {
        nlohmann::json body;
        body["model"] = req.model;
        body["temperature"] = req.temperature;
        body["max_tokens"] = req.maxTokens;
        for (auto &msg: req.messages) {
            body["messages"].push_back(
                { { "role", msg.role }, { "content", msg.content } });
        }

        httplib::Client cli("localhost", 11437);
        auto res = cli.Post("/api/chat", body.dump(), "application/json");
        if (!res || res->status != 200) {
            throw std::runtime_error("Ollama chat request failed");
        }

        auto jsonResp = nlohmann::json::parse(res->body);
        auto response = std::make_unique<OllamaResponse>();
        response->generatedText = jsonResp.value("completion", "");
        return response;
    }

    std::unique_ptr<LLMResponse> generateResponseChatStream(
        const OllamaChatRequest &req, TokenCallback tokenCallback)
    {
        nlohmann::json body;
        body["model"] = req.model;
        body["temperature"] = req.temperature;
        body["max_tokens"] = req.maxTokens;
        for (auto &msg: req.messages) {
            body["messages"].push_back(
                { { "role", msg.role }, { "content", msg.content } });
        }

        httplib::Client cli("localhost", 11437);
        std::string finalText;

        // auto res = cli.Post(
        //     "/api/chat",
        //     body.dump(),
        //     "application/json",
        //     [&](const char* data, size_t length) {
        //         std::string chunk(data, length);
        //         auto j = nlohmann::json::parse(chunk, nullptr, false);
        //         if (!j.is_discarded() && j.contains("token")) {
        //             std::string token = j["token"].get<std::string>();
        //             finalText += token;
        //             if (!tokenCallback(token)) {
        //                 return false;
        //             }
        //         }
        //         return true;
        //     }
        // );

        auto response = std::make_unique<OllamaResponse>();
        response->generatedText = finalText;
        return response;
    }
};

//------------------------------------------------------------------------------
// Groq Client
//------------------------------------------------------------------------------
class GroqLLMClient: public LLMClient {
public:
    explicit GroqLLMClient(std::string apiKey = "") :
        apiKey_(std::move(apiKey)) { }

    std::unique_ptr<LLMResponse> generateResponse(
        const LLMRequest &request) override
    {
        const auto &req = dynamic_cast<const GroqRequest &>(request);

        nlohmann::json json_data;
        to_json(json_data, req);

        httplib::SSLClient cli("api.groq.com", 443);
        cli.enable_server_certificate_verification(true);
        cli.set_default_headers({ { "Authorization", "Bearer " + apiKey_ } });
        httplib::Headers headers = { { "Content-Type", "application/json" } };
        std::cout << json_data.dump() << std::endl;
        auto res = cli.Post("/openai/v1/chat/completions", headers,
            json_data.dump(), "application/json");
        if (!res || res->status != 200) {
            throw std::runtime_error(
                "Groq request failed: " + res->reason + res->body + (res ? std::to_string(res->status) : "no response"));
        }

        try {
            auto jsonResp = nlohmann::json::parse(res->body);
            auto response = std::make_unique<GroqResponse>();

            // Deserialize JSON response into GroqResponse
            from_json(jsonResp, *response);

            return response;
        } catch (const std::exception &e) {
            std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
            throw; // Re-throw the exception after logging
        }
    }

private:
    std::string apiKey_;
};

//------------------------------------------------------------------------------
// Factory
//------------------------------------------------------------------------------
class LLMClientFactory {
public:
    using Creator = std::function<std::unique_ptr<LLMClient>(
        const std::unordered_map<std::string, std::string> &)>;

    static LLMClientFactory &instance()
    {
        static LLMClientFactory factory;
        return factory;
    }

    void registerClient(const std::string &type, Creator creator)
    {
        registry_[type] = std::move(creator);
    }

    std::unique_ptr<LLMClient> create(
        const std::string &type,
        const std::unordered_map<std::string, std::string> &config) const
    {
        auto it = registry_.find(type);
        if (it == registry_.end())
            throw std::runtime_error("Unknown LLM client type: " + type);
        return it->second(config);
    }

private:
    LLMClientFactory() = default;
    std::unordered_map<std::string, Creator> registry_;
};

//------------------------------------------------------------------------------
// Registration
//------------------------------------------------------------------------------
inline void registerLLMClients()
{
    LLMClientFactory::instance().registerClient(
        "ollama", [](const std::unordered_map<std::string, std::string> &) {
            return std::make_unique<OllamaLLMClient>();
        });

    LLMClientFactory::instance().registerClient(
        "groq", [](const std::unordered_map<std::string, std::string> &cfg) {
            std::string apiKey = "";
            if (auto it = cfg.find("apiKey"); it != cfg.end())
                apiKey = it->second;
            return std::make_unique<GroqLLMClient>(apiKey);
        });
}
