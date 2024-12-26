#include "auralis_client.h"
#include "llm_manager.h"
#include "logger.h"
#include "whisper_client.h"
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
class AgentSIP {
public:
    AgentSIP(const std::string &whisper_url = "ws://localhost:8765",
        const std::string &auralis_url = "ws://localhost:8766",
        const std::string &model_name = "gemma2-9b-it",
        const std::string &api_key = "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") :
        messages_ { { "system", "You are a helpful assistant." }, { "user", "Tell me a joke." } },
        model_(model_name),
        groqClient_(LLMClientFactory::instance().create("groq", { { "apiKey", api_key } }))
    {
        connectClients(whisper_url, auralis_url);
        configureCallbacks();
    }

    void sendAudio(const std::vector<int16_t> &audio_data)
    {
        whisperClient_.send_audio(audio_data);
    }

    void sendText(const std::string &text)
    {
        auralisClient_.synthesize_text(text);
    }

    void setAudioChunkCallback(AuralisClient::AudioChunkCallback callback)
    {
        audioCallback_ = std::move(callback);
    }

private:
    void connectClients(const std::string &whisper_url, const std::string &auralis_url)
    {
        whisperClient_.connect(whisper_url);
        auralisClient_.connect(auralis_url);
        std::this_thread::sleep_for(std::chrono::seconds(2));

    }

    void configureCallbacks()
    {
        whisperClient_.set_transcription_callback(
            [this](const std::string &transcription) {
                LOG_DEBUG("Transcription: %s", transcription.c_str());
                messages_.emplace_back("user", transcription);
                auto response = generateText(transcription);
                sendText(response);
            });

        auralisClient_.set_audio_callback(
            [this](const std::vector<int16_t> &audio_data) {
                LOG_DEBUG("Audio size: %zu", audio_data.size());
                if (audioCallback_) {
                    audioCallback_(audio_data);
                }
            });
    }

    std::string generateText(const std::string &input)
    {
        GroqRequest request;
        request.messages = messages_;
        request.model = model_;
        auto responsePtr = groqClient_->generateResponse(request);
        auto groqResponse = dynamic_cast<GroqResponse *>(responsePtr.get());

        if (!groqResponse || groqResponse->choices.empty()) {
            throw std::runtime_error("Invalid response from GroqClient");
        }

        const auto &choice = groqResponse->choices.front();
        LOG_DEBUG("Groq Response: %s", choice.message.content.c_str());
        messages_.emplace_back(choice.message.role, choice.message.content);
        return choice.message.content;
    }
    std::vector<GroqRequest::Message> messages_ = { { "system", "You are a helpful assistant, make a short response for one sentence." } };
    const std::string model_;
    WhisperClient whisperClient_;
    
    AuralisClient auralisClient_;
    AuralisClient::AudioChunkCallback audioCallback_;
    std::unique_ptr<LLMClient> groqClient_;
};
