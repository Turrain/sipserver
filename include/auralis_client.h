#pragma once
#include "abs_ws_client.h"
#include "json.hpp"
#include <vector>
#include <functional>
#include <iostream>
#include "logger.h"
class AuralisClient : public AbstractWebSocketClient {
public:
    using AudioChunkCallback = std::function<void(const std::vector<int16_t>&)>;
    using StatusCallback = std::function<void(const std::string&)>;

    void set_audio_callback(AudioChunkCallback callback) {
        audio_callback = callback;
    }

    void set_status_callback(StatusCallback callback) {
        status_callback = callback;
    }

     void synthesize_text(const std::string& text, const std::string& voice = "default", bool stream = true, float temperature = 0.5) {
        if (!connected) {
            logger.error("Not connected to Auralis TTS server");
            return;
        }

        try {
            nlohmann::json request = {
                {"input", text},
                {"voice", voice},
                {"stream", stream},
                {"temperature", temperature},
                {"type", "synthesize"}
            };
            client.send(connection, request.dump(), websocketpp::frame::opcode::text);
        } catch (const std::exception& e) {
            logger.error("Error synthesizing text: %s", e.what());
        }
    }

protected:
      void on_message(websocketpp::connection_hdl hdl, MessagePtr msg) override {
        try {
            if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
                // Handle binary audio data
                const auto& payload = msg->get_payload();
                std::vector<int16_t> audio_data(payload.size() / sizeof(int16_t));
                std::memcpy(audio_data.data(), payload.data(), payload.size());
                logger.info("Received %d audio samples", audio_data.size());

                if (audio_callback) {
                    audio_callback(audio_data);
                }
            } else {
                // Handle JSON status and error messages
                auto json_msg = nlohmann::json::parse(msg->get_payload());
                if (json_msg.contains("status")) {
                    std::string status = json_msg["status"].get<std::string>();
                    if (status_callback) {
                        status_callback(status);
                    }
                } else if (json_msg.contains("error")) {
                    std::string error = json_msg["error"].get<std::string>();
                   
                }
            }
        } catch (const std::exception& e) {
            std::cout << "Error processing message: " << e.what() << std::endl;
           
        }
    }

    void on_open(websocketpp::connection_hdl hdl) override {
        std::cout << "Connected to Auralis TTS server" << std::endl;
    }

    void on_close(websocketpp::connection_hdl hdl) override {
        std::cout << "Disconnected from Auralis TTS server" << std::endl;
    }

    void on_error(const std::string& error) override {
        std::cout << "Error: " << error << std::endl;
    }

private:
    AudioChunkCallback audio_callback;
    StatusCallback status_callback;
    ::Logger &logger = ::Logger::getInstance();
};