#pragma once
#include "core/configuration.h"
#include "provider/provider_manager.h"
#include "stream/whisper_client.h"
#include "stream/auralis_client.h"
#include "utils/logger.h"
#include <deps/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "common/message.h"

using json = nlohmann::json;

class Agent {
public:
    using SpeechCallback = std::function<void(const std::vector<int16_t>&)>;

    explicit Agent(std::string id, std::shared_ptr<core::Configuration> config)
        : id_(std::move(id)), config_(std::move(config)) 
    {
        // Register configuration observers
        config_->add_observer("stm_capacity", [this](const auto&) {
            update_stm_capacity();
        });
        
        config_->add_observer("provider", [this](const auto&) {
            update_provider();
        });

        // Initial setup
        update_stm_capacity();
        update_provider();
    }

    virtual ~Agent() = default;

    const std::string& id() const { return id_; }
    std::shared_ptr<core::Configuration> config() const { return config_; }

    void configure(const std::string& key_path, const std::string& value) {
        this->config_->set(key_path, value);
    }

    void configure(const json& patch) {
        this->config_->atomic_patch(patch);
    };

    template<typename T>
    T get_config(const std::string& key_path) const {
        return config_->get<T>(key_path);
    }

    // Core functionality
    virtual std::string process_message(const std::string& message) = 0;
    virtual void process_audio(const std::vector<int16_t>& audio_data) = 0;
    virtual std::string generate_response(const std::string& text) = 0;
    virtual void set_speech_callback(SpeechCallback callback) = 0;

    void think(const std::string& input) {
        process_message(input);
    }

protected:
    void maintain_history() {
        while (history_.size() > stm_capacity_) {
            history_.erase(history_.begin());
        }
    }

    std::string id_;
    std::shared_ptr<core::Configuration> config_;
    std::vector<Message> history_;
    size_t stm_capacity_ = 10;
    std::string current_provider_;

private:
    void update_stm_capacity() {
        stm_capacity_ = config_->get<size_t>("stm_capacity");
    }

    void update_provider() {
        current_provider_ = config_->get<std::string>("provider");
    }
};

class BaseAgent : public Agent {
private:
    WhisperClient whisper_client_;
    AuralisClient auralis_client_;
    std::string whisper_endpoint_;
    std::string auralis_endpoint_;
    std::string voice_style_;
    float voice_temperature_;

    void connect_services() {
        whisper_client_.connect(whisper_endpoint_);
        auralis_client_.connect(auralis_endpoint_);
        
        whisper_client_.set_transcription_callback([this](const auto& text) {
            this->process_message(text);
        });
    }

    void setup_voice_parameters() {
        voice_style_ = config_->get<std::string>("voice.style");
        voice_temperature_ = config_->get<float>("voice.temperature");
    }

protected:
    std::string generate_response(const std::string& text) override {
        auto* provider_mgr = ProviderManager::getInstance();
        if (!provider_mgr->hasProvider(current_provider_)) {
            LOG_ERROR << "Provider unavailable: " << current_provider_;
            return "";
        }

        auto response = provider_mgr->processRequest(current_provider_, text);
        LOG_DEBUG << "Received response: " << response.content;
        if (!response.content.empty()) {
            history_.emplace_back("assistant", response.content);
           // auralis_client_.synthesize_text(response.content, voice_style_, voice_temperature_);
        }
        
        maintain_history();
        return response.content;
    }

public:
    explicit BaseAgent(const std::string& id, 
                      std::shared_ptr<core::Configuration> config)
        : Agent(id, config),
          whisper_endpoint_(config->get<std::string>("services.whisper.url")),
          auralis_endpoint_(config->get<std::string>("services.auralis.url"))
    {
        setup_voice_parameters();
        connect_services();
        
        config_->add_observer("voice", [this](const auto&) {
            setup_voice_parameters();
        });
    }

    void set_speech_callback(SpeechCallback callback) override {
        auralis_client_.set_audio_callback(std::move(callback));
    }

    std::string process_message(const std::string& message) override {
        LOG_DEBUG << "Processing message: " << message;
        history_.emplace_back("user", message);
        return generate_response(message);
    }

    void process_audio(const std::vector<int16_t>& audio_data) override {
        whisper_client_.send_audio(audio_data);
    }

    void bulk_configure(const json& patch) {
        auto txn = config_->json_transaction(patch);
        txn.commit();
    }
};

class AgentManager {
public:
    explicit AgentManager(std::shared_ptr<core::Configuration> config)
        : global_config_(std::move(config))
    {
        setup_config_observers();
        initialize_agents();
    }

    std::vector<std::shared_ptr<Agent>> getAgents() {
        std::lock_guard lock(mutex_);
        std::vector<std::shared_ptr<Agent>> result;
        for (const auto& [_, agent] : agents_) {
            result.push_back(agent);
        }
        return result;
    }

    std::shared_ptr<core::Configuration> getConfig() {
        return global_config_;
    }

    std::shared_ptr<Agent> getAgent(const std::string& id) {
        return get_agent(id);
    }

    std::shared_ptr<Agent> create_agent(const std::string& id) {
        std::lock_guard lock(mutex_);
        
        if (auto it = agents_.find(id); it != agents_.end()) {
            return it->second;
        }

        auto agent_config = global_config_->create_scoped_config("agents." + id);
        configure_agent_defaults(*agent_config);
        
        auto agent = std::make_shared<BaseAgent>(id, agent_config);
        agents_[id] = agent;
        return agent;
    }

    std::shared_ptr<Agent> get_agent(const std::string& id) {
        std::lock_guard lock(mutex_);
        auto it = agents_.find(id);
        return (it != agents_.end()) ? it->second : nullptr;
    }

    void update_agent_config(const std::string& id, 
                           const std::string& key_path,
                           const std::string& value) {
        if (auto agent = get_agent(id)) {
            agent->configure(key_path, value);
        }
    }

    void remove_agent(const std::string& id) {
        std::lock_guard lock(mutex_);
        if (auto it = agents_.find(id); it != agents_.end()) {
            agents_.erase(it);
        }
    }

private:
    void setup_config_observers() {
        global_config_->add_observer("agents", [this](const auto&) {
            sync_agents_with_config();
        });
    }

    void initialize_agents() {
        try {
            auto agent_ids = global_config_->get<std::vector<std::string>>("agents.ids");
            for (const auto& id : agent_ids) {
                try {
                    create_agent(id);
                } catch (const std::exception& e) {
                    LOG_ERROR << "Failed to initialize agent " << id << ": " << e.what();
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to get agent IDs: " << e.what();
        }
    }

    void sync_agents_with_config() {
        std::lock_guard lock(mutex_);
        auto current_agents = global_config_->get<std::vector<std::string>>("agents.ids");
        
        // Remove obsolete agents
        std::vector<std::string> to_remove;
        for (const auto& [id, _] : agents_) {
            if (std::find(current_agents.begin(), current_agents.end(), id) == current_agents.end()) {
                to_remove.push_back(id);
            }
        }
        for (const auto& id : to_remove) {
            agents_.erase(id);
        }

        // Add new agents
        for (const auto& id : current_agents) {
            if (agents_.find(id) == agents_.end()) {
                create_agent(id);
            }
        }
    }

    void configure_agent_defaults(core::Configuration& config) {
        try {
            core::Configuration::Transaction tx(config);
            tx.data()["stm_capacity"] = 15;
            tx.data()["provider"] = "groq";
            tx.data()["services"] = {
                {"whisper", {{"url", "ws://localhost:9090"}}},
                {"auralis", {{"url", "ws://localhost:9091"}}}
            };
            tx.data()["voice"] = {
                {"style", "neutral"},
                {"temperature", 0.7}
            };
            tx.commit();
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to configure agent defaults: " << e.what();
            throw;
        }
    }

    std::shared_ptr<core::Configuration> global_config_;
    std::unordered_map<std::string, std::shared_ptr<Agent>> agents_;
    std::mutex mutex_;
};
