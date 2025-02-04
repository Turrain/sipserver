
#include "agent/agent.h"
#include "provider/provider_manager.h"
#include "utils/logger.h"

void Agent::set_speech_callback(SpeechCallback callback)
{
    this->on_speech = callback;
}

void Agent::connect_services()
{
    try {
        this->whisper_client_->connect("ws://stt:8765");
        this->whisper_client_->set_transcription_callback(
            [this](const std::string &transcription) {
                auto res = this->process_message(transcription);
                this->generate_audio(res);
            });
        this->auralis_client_->connect("ws://tts:8766");
    } catch (...) {
    }
}
void Agent::process_audio(const std::vector<int16_t> &audio_data)
{

    this->whisper_client_->send_audio(audio_data);
}

void Agent::generate_audio(const std::string &text)
{
    this->auralis_client_->synthesize_text(text);
}

std::string Agent::process_message(const std::string &text)
{
   
    json history = history_;
    auto response = ProviderManager::getInstance()
        .process_request(
            config_.value("provider", "ollama"), 
            text, 
            config_.value("provider_options", json::object()), 
            history, 
            metadata_
        );

    std::string result;

    if (response.success) {
        result = response.response;
    } else {
        LOG_ERROR << "Failed to process message: " << response.error;
        result = response.error;
    }

    history_mutex_.lock();
    this->history_.push_back(Message("user", text));
    history_.push_back(Message("assistant", result));
    history_mutex_.unlock();

   
    this->metadata_ = response.metadata;
    

    return result;
}
