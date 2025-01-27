#pragma once
#include "common/message.h"
#include "core/configuration.h"
#include "provider/provider_manager.h"
#include "stream/auralis_client.h"
#include "stream/whisper_client.h"
#include "utils/logger.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class Agent {
public:
    using SpeechCallback = std::function<void(const std::vector<int16_t>&)>;

    explicit Agent(core::ScopedConfiguration config);
    virtual ~Agent() = default;

    // Get agent ID from config

    const core::ScopedConfiguration& config() const { return config_; }

    // Configure agent settings using a JSON patch
    void configure(const std::string & path, const nlohmann::json& patch);

    // Core functionality
    virtual std::string process_message(const std::string& message) = 0;
    virtual void process_audio(const std::vector<int16_t>& audio_data) = 0;
    virtual std::string generate_response(const std::string& text) = 0;
    virtual void set_speech_callback(SpeechCallback callback) = 0;

    void think(const std::string& input) { process_message(input); }

protected:
    void maintain_history();

    core::ScopedConfiguration config_;
    std::vector<Message> history_;
    std::mutex history_mutex_;
};

class BaseAgent : public Agent {
public:
    explicit BaseAgent(core::ScopedConfiguration config);
    void set_speech_callback(SpeechCallback callback) override;
    std::string process_message(const std::string& message) override;
    void process_audio(const std::vector<int16_t>& audio_data) override;

protected:
    std::string generate_response(const std::string& text) override;

private:
    void connect_services();
    void setup_voice_parameters();
    void setup_services_config();

    std::unique_ptr<WhisperClient> whisper_client_;
    std::unique_ptr<AuralisClient> auralis_client_;
};

class AgentManager {
public:
    explicit AgentManager(core::ScopedConfiguration config);

    std::vector<std::shared_ptr<Agent>> get_agents() const;
    const core::ScopedConfiguration& config() const { return config_; }
    std::shared_ptr<Agent> get_agent(const std::string& id) const;
    std::shared_ptr<Agent> create_agent(const std::string& id);
    void remove_agent(const std::string& id);

private:
    void validate_agent_config(const std::string& id, const nlohmann::json& config) const;
    void set_default_agent_config(core::ScopedConfiguration& agent_config) const;

    core::ScopedConfiguration config_;
    std::unordered_map<std::string, std::shared_ptr<Agent>> agents_;
    mutable std::mutex mutex_;
};
