#include "agent/agent.h"
#include "provider/provider_manager.h"
#include "utils/logger.h"

// Agent Implementation
Agent::Agent(core::ScopedConfiguration config)
    : config_(std::move(config)) {
}

void Agent::configure(const std::string & path, const nlohmann::json& patch) {

    config_.set(path, patch);
}

void Agent::maintain_history() {
    std::lock_guard<std::mutex> lock(history_mutex_);
for (const auto& entry : history_) {
    LOG_DEBUG << "History - Role: " << entry.role << ", Message: " << entry.content;
}

    auto capacity_opt = config_.get<size_t>("/stm_capacity");   
    auto stm_capacity = capacity_opt;
    if (history_.size() > stm_capacity) {
        history_.erase(history_.begin(), history_.end() - stm_capacity);
    }
}

// BaseAgent Implementation
BaseAgent::BaseAgent(core::ScopedConfiguration config)
    : Agent(std::move(config)),
      whisper_client_(std::make_unique<WhisperClient>()),
      auralis_client_(std::make_unique<AuralisClient>()) {
    
    setup_services_config();
    setup_voice_parameters();
    connect_services();
}

void BaseAgent::setup_services_config() {
}

void BaseAgent::set_speech_callback(SpeechCallback callback) {
    auralis_client_->set_audio_callback(std::move(callback));
}

std::string BaseAgent::process_message(const std::string& message) {
    LOG_DEBUG << "Processing message: " << message;
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        history_.emplace_back("user", message);
    }
    return generate_response(message);
}

void BaseAgent::process_audio(const std::vector<int16_t>& audio_data) {
    whisper_client_->send_audio(audio_data);
}

std::string BaseAgent::generate_response(const std::string& text) {
    auto provider_mgr = ProviderManager::getInstance();
    auto provider_opt = config_.get<std::string>("/provider");
  
    
    if (!provider_mgr->hasProvider(provider_opt)) {
        LOG_ERROR << "Provider unavailable: " << provider_opt;
        return "";
    }

    auto provider_options = config_.get<nlohmann::json>("/provider_options");
    auto history = this->history_;
    auto response = provider_mgr->processRequest(provider_opt, text, provider_options, history);
    LOG_DEBUG << "Received response: " << response.content;

    if (!response.content.empty()) {
        std::lock_guard<std::mutex> lock(history_mutex_);
        history_.emplace_back("assistant", response.content);
        // std::string voice_style = style_opt.value_or("neutral");
        // float temperature = temp_opt.value_or(0.7);
        // auralis_client_->synthesize_text(response.content, voice_style, static_cast<float>(temperature));
    }

    maintain_history();
    return response.content;
}

void BaseAgent::generate_audio(const std::string& text) {
    auto voice_style_opt = "default";
    auto temperature_opt = 0.7;

    std::string voice_style = voice_style_opt;
    float temperature = temperature_opt;
    LOG_DEBUG << "Voice parameters: style=" << voice_style << ", temperature=" << temperature;
    auralis_client_->synthesize_text(text, voice_style, temperature);
}

void BaseAgent::connect_services() {
    try {
        auto whisper_opt = config_.get<std::string>("/services/whisper/url");
        auto auralis_opt = config_.get<std::string>("/services/auralis/url");
        
        std::string whisper_url = whisper_opt;
        std::string auralis_url = auralis_opt;

        whisper_client_->connect(whisper_url);
        auralis_client_->connect(auralis_url);

        whisper_client_->set_transcription_callback([this](const std::string& text) {
            std::string text_res = this->process_message(text);
            if (!text_res.empty()) {
                this->generate_audio(text_res);
            }
        });
    } catch (const std::exception& e) {
        LOG_ERROR << "Service connection failed: " << e.what();
        throw;
    }
}

void BaseAgent::setup_voice_parameters() {
    // Parameters are handled through observers and synthesize_text
    LOG_DEBUG << "Voice parameters updated";
}

// AgentManager Implementation
AgentManager::AgentManager(core::ScopedConfiguration config)
    : config_(std::move(config)) {
}

std::vector<std::shared_ptr<Agent>> AgentManager::get_agents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Agent>> result;
    result.reserve(agents_.size());
    for (const auto& [id, agent] : agents_) {
        result.push_back(agent);
    }
    return result;
}

std::shared_ptr<Agent> AgentManager::get_agent(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = agents_.find(id);
    return it != agents_.end() ? it->second : nullptr;
}

std::shared_ptr<Agent> AgentManager::create_agent(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (agents_.count(id)) {
        return agents_.at(id);
    }

    try {
        config_.beginTransaction();
        config_.set("/" + id, {
            {"id", id},
            {"type", "base"},
            {"stm_capacity", 15},

            {"voice", {
                {"style", "neutral"},
                {"temperature", 0.7}
            }},

            {"provider", "ollama"},
            {"provider_options", {
                {"model", "llama3.2:1b"},
                {"temperature", 0.7},
            }},

            {"services", {
                {"whisper", {{"url", "ws://stt:8765"}}},
                {"auralis", {{"url", "ws://tts:8766"}}}
            }}
        });
        config_.commit();
        auto agent_config = core::ScopedConfiguration(config_, "/agents/" + id);
        auto agent = std::make_shared<BaseAgent>(std::move(agent_config));
        agents_.emplace(id, agent);
        return agent;
    } catch (const std::exception& e) {
        LOG_ERROR << "Agent creation failed: " << e.what();
        throw;
    }
}

void AgentManager::remove_agent(const std::string& id) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (agents_.erase(id) > 0) {
        config_.beginTransaction();
        config_.set("/" + id, nullptr);
        config_.commit();
    }
}


void AgentManager::validate_agent_config(const std::string& id, const json& config) const {
    if (!config.is_object()) {
        throw std::runtime_error("Invalid agent configuration for " + id + ": must be an object");
    }
}

