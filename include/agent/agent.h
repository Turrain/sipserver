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
#include <shared_mutex>
#include <db/GlobalDatabase.h>


class Agent {
public:
    explicit Agent(const json& config = json::object()) : config_(config) {}
    
    using SpeechCallback = std::function<void(const std::vector<int16_t>&)>;
    // LLM RESPONSE
    std::string process_message(const std::string& text);
    // WHISPER
    void process_audio(const std::vector<int16_t>& audio_data);
    // TTS
    void generate_audio(const std::string& text);
    void set_speech_callback(SpeechCallback callback);
    
    void connect_services();
    
    json get_config() const { return config_; }
    void update_config(const json& config) { config_ = config; }
    
protected:
    SpeechCallback on_speech;
    MessageList history_;
    std::mutex history_mutex_;
    json config_;
    json metadata_;

    std::unique_ptr<WhisperClient> whisper_client_;
    std::unique_ptr<AuralisClient> auralis_client_;
};


class AgentManager {
public:
    static AgentManager& getInstance() {
        static AgentManager instance;
        return instance;
    }
    std::vector<std::shared_ptr<Agent>> get_agents() const {
        std::shared_lock lock(agents_mutex_);
        std::vector<std::shared_ptr<Agent>> agents;
        for (const auto& [id, agent] : agents_) {
            agents.push_back(agent);
        }
        return agents;
    }

    void add_agent(const std::string& id, const json& config) {
        auto agent = std::make_shared<Agent>(config);
        agent->connect_services();
        
        {
            std::unique_lock lock(agents_mutex_);
            agents_[id] = agent;
        }
        
        persist_agent(id, config);
    }

    void remove_agent(const std::string& id) {
        {
            std::unique_lock lock(agents_mutex_);
            agents_.erase(id);
        }
        
        GlobalDatabase::instance().execute([&](auto& db) {
            if (db.hasTable("agents")) {
                db.getTable("agents").deleteDocuments([&](const Document&) { return true; });
            }
        });
    }

    std::shared_ptr<Agent> get_agent(const std::string& id) const {
        std::shared_lock lock(agents_mutex_);
        auto it = agents_.find(id);
        return it != agents_.end() ? it->second : nullptr;
    }

     void clear_agents() {
        std::unique_lock lock(agents_mutex_);
        agents_.clear();
        GlobalDatabase::instance().execute([&](auto& db) {
            db.getTable("agents").deleteDocuments([](const Document&) { return true; });
        });
    }

     void update_agent_config(const std::string& id, const json& config) {
        auto agent = get_agent(id);
        if (agent) {
            agent->update_config(config);
            agent->connect_services();  // Reconnect with new config
            persist_agent(id, config);
        }
    }

private:
 AgentManager() {
        load_agents_from_db();
    }
    void load_agents_from_db() {
        GlobalDatabase::instance().query([&](const auto& db) {
            if (!db.hasTable("agents")) return;

            const auto& agents_table = db.getTable("agents");
            for (const auto& [id, doc] : agents_table) {
                try {
                    json config = json::parse(doc.toJson().dump());
                    internal_add_agent(id, config);
                } catch (const std::exception& e) {
                    LOG_ERROR << "Failed to load agent " << id << ": " << e.what();
                }
            }
        });
    }
     void internal_add_agent(const std::string& id, const json& config) {
        auto agent = std::make_shared<Agent>(config);
        agent->connect_services();
        {
            std::unique_lock lock(agents_mutex_);
            agents_[id] = agent;
        }
    }

     void persist_agent(const std::string& id, const json& config) {
        GlobalDatabase::instance().execute([&](auto& db) {
            if (!db.hasTable("agents")) {
                db.createTable("agents");
            }
            auto& agents_table = db.getTable("agents");
            Document doc(config);
            agents_table.insertDocument(id, doc);
        });
    }
    mutable std::shared_mutex agents_mutex_;
    std::unordered_map<std::string, std::shared_ptr<Agent>> agents_;
};
