#pragma once
#include "json.hpp"
#include <deque>
#include "logger.h"
#include <string>
#include <vector>
#include "provider_manager.h"
#include "request.h"
using json = nlohmann::json;

class Agent {
public:
    std::string id;
    json config;

    Messages history;
    int stmCapacity;
    Agent(const std::string &agent_id, const json &cfg) :
        id(agent_id), config(cfg)
    {
        stmCapacity = config.value("stm_capacity", 10); // Default STM capacity
        history.clear();
    }


    virtual ~Agent() = default;
    virtual void think(const std::string &message) = 0;
    virtual void listen(const std::vector<int16_t> &audio_data) = 0;
    virtual void speak(std::function<void(const std::vector<int16_t> &)> &get_audio_callback) = 0;
    virtual void configure(const json &newConfig)
    {
        config.merge_patch(newConfig); // Use JSON merge patch to update
        if (newConfig.contains("stm_capacity")) {
            stmCapacity = newConfig["stm_capacity"];
        }
        LOG_DEBUG("Agent %s updated config: %s", id.c_str(), config.dump().c_str());
    }
};

class BaseAgent: public Agent {
public:
    BaseAgent(const std::string &agent_id, const json &cfg) :
        Agent(agent_id, cfg) { }

    void think(const std::string &message) override
    {
        LOG_DEBUG("BaseAgent %s thinks: %s", id.c_str(), message.c_str());
   //     addToSTM("user", message);
        history.push_back({"user", message});
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
        std::unique_ptr<Request> request = ProviderManager::getInstance()->createRequest(providerName, history);

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
            
            history.push_back({"assistant", response->toString()});
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
    // Singleton instance
    static AgentManager *getInstance()
    {
        static AgentManager instance; // Guaranteed to be initialized only once
        return &instance;
    }

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
    bool updateAgentConfig(const std::string &agentId, const json &newConfig)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (agents_.find(agentId) != agents_.end()) {
            agents_[agentId]->configure(newConfig);
            return true;
        }
        return false;
    }

private:
    AgentManager() { } // Private constructor
    ~AgentManager() { } // Private destructor
    AgentManager(const AgentManager &) = delete; // Prevent copy-construction
    AgentManager &operator=(const AgentManager &) = delete; // Prevent assignment

    std::unordered_map<std::string, std::shared_ptr<Agent>> agents_;
    std::mutex mutex_;
};