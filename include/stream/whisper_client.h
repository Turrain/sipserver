#pragma once
#include "abs_ws_client.h"
#include "deps/json.hpp"
class WhisperClient : public AbstractWebSocketClient {
public:
    using TranscriptionCallback = std::function<void(const std::string&)>;
    void set_transcription_callback(TranscriptionCallback callback) {
        transcription_callback = callback;
    }
    void send_audio(const std::vector<int16_t>& audio_data) {
        if (!connected) return;

        try {
            client.send(connection,
                       audio_data.data(),
                       audio_data.size() * sizeof(int16_t),
                       websocketpp::frame::opcode::binary);
        } catch (const std::exception& e) {
            on_error(e.what());
        }
    }

protected:
    void on_message(websocketpp::connection_hdl hdl, MessagePtr msg) override {
        try {
            auto json_msg = nlohmann::json::parse(msg->get_payload());

            // Handle transcription result
            if (json_msg.contains("text")) {
                std::string transcription = json_msg["text"].get<std::string>();
                if (transcription_callback) {
                    transcription_callback(transcription);
                }
            }

        } catch (const std::exception& e) {
            std::cout << "Error parsing message: " << e.what() << std::endl;
        }
    }

    void on_open(websocketpp::connection_hdl hdl) override {
        std::cout << "Connected to server" << std::endl;
    }

    void on_close(websocketpp::connection_hdl hdl) override {
        std::cout << "Disconnected from server" << std::endl;
    }

    void on_error(const std::string& error) override {
        std::cout << "Error: " << error << std::endl;
    }
private:
    TranscriptionCallback transcription_callback;
};