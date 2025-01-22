#pragma once
#include "deps/json.hpp"
#include "provider/provider_manager.h"
#include "utils/logger.h"
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "stream/auralis_client.h"
#include "stream/whisper_client.h"

using json = nlohmann::json;


using Messages = std::vector<Message>;

class Agent {
public:
    using SpeechCallbackData = std::function<void(const std::vector<int16_t> &)>;

    std::string id;
    json config;
    SpeechCallbackData speechCallback;
    Messages history;
    int stmCapacity;
    Agent(const std::string &agent_id, const json &cfg) :
        id(agent_id), config(cfg)
    {
        stmCapacity = config.value("stm_capacity", 10);
        history.clear();
    }
    virtual void setSpeechCallback(SpeechCallbackData callback)
    {
        speechCallback = callback;
    }

    virtual ~Agent() = default;
    virtual void think(const std::string &message) = 0;
    virtual void listen(const std::vector<int16_t> &audio_data) = 0;
    virtual void speak(const std::string &text) = 0;
    virtual void configure(const json &newConfig)
    {
        config.merge_patch(newConfig);
        if (newConfig.contains("stm_capacity")) {
            stmCapacity = newConfig["stm_capacity"];
        }
        LOG_DEBUG << "Agent " << id << " updated config: " << config.dump();
    }
};

class BaseAgent: public Agent {
private:
    WhisperClient whisper_client;
    AuralisClient auralis_client;

public:
    BaseAgent(const std::string &agent_id, const json &cfg) :
        Agent(agent_id, cfg)
    {
        whisper_client.connect("ws://localhost:9090");
        auralis_client.connect("ws://localhost:9091");

        whisper_client.set_transcription_callback([this](const std::string &text) {
            LOG_INFO << "Agent " << id << " received transcription: " << text;
            this->think(text);
        });
    }

    void setSpeechCallback(SpeechCallbackData callback) override
    {
        speechCallback = callback;
        auralis_client.set_audio_callback(speechCallback);
    }

    void think(const std::string &message) override
    {
        LOG_DEBUG << "Agent " << id << " thinks: " << message; 

        history.push_back({"user", message});

        std::string providerName = config.value("provider", "");

        if (providerName.empty()) {
            LOG_ERROR << "Provider not specified for agent " << id;
            return;
        }

        if (!ProviderManager::getInstance()->hasProvider(providerName)) {
            LOG_ERROR << "Provider " << providerName << " not found";
            return;
        }

        // Build conversation history string
        std::string conversation;
        for (const auto &msg : history) {
            conversation += msg.role + ": " + msg.content + "\n";
        }

        auto response = ProviderManager::getInstance()->processRequest(providerName, conversation);
        
        if (!response.content.empty()) {
            LOG_INFO << "Agent " << providerName << id << " received response: " << response.content;

            history.push_back({"assistant", response.content});
            this->speak(response.content);
        } else {
            LOG_ERROR << "Agent " << id << " failed to process request";
        }

        // Maintain history size
        while (history.size() > static_cast<size_t>(stmCapacity)) {
            history.erase(history.begin());
        }
    }

    void listen(const std::vector<int16_t> &audio_data) override
    {
        LOG_DEBUG << "Agent " << id << " received audio data";
        whisper_client.send_audio(audio_data);
    }

    void speak(const std::string &text) override
    {
        LOG_DEBUG << "Agent " << id << " speaks: " << text << " (provider: auralis)";

        // Configure status callback for logging
        auralis_client.set_status_callback([this](const std::string &status) {
            LOG_DEBUG << "Agent " << id << " received status: " << status;
        });

        // Send text for synthesis
        std::string voice = config.value("voice", "default");
        float temperature = config.value("temperature", 0.5f);
        auralis_client.synthesize_text(text, voice, true, temperature);
    }

    void configure(const json &newConfig) override
    {
        config.merge_patch(newConfig);
        LOG_DEBUG << "Agent " << id << " updated config: " << config.dump();
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
            return agents_[id];
        }

        std::shared_ptr<Agent> agentPtr;
        if (type == "BaseAgent") {
            agentPtr = std::make_shared<BaseAgent>(id, cfg);
        } else {
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

    std::vector<std::shared_ptr<Agent>> getAgents()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Agent>> agents;
        for (const auto &[id, agent]: agents_) {
            agents.push_back(agent);
        }
        return agents;
    }

    bool updateAgentConfig(const std::string &agentId, const json &newConfig)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (agents_.find(agentId) != agents_.end()) {
            agents_[agentId]->configure(newConfig);
            return true;
        }
        return false;
    }

    bool removeAgent(const std::string &id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return agents_.erase(id) > 0;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Agent>> agents_;
    std::mutex mutex_;
};
