// Include websocketpp headers for TLS (wss:// support)
#pragma once
#include <websocketpp/config/asio_client.hpp>

#include <websocketpp/client.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstring> // for std::memcpy

#include "deps/json.hpp"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "deps/httplib.h"

// For convenience
using json = nlohmann::json;
typedef websocketpp::client<websocketpp::config::asio_tls_client> ws_client;



inline std::shared_ptr<websocketpp::lib::asio::ssl::context> on_tls_init(websocketpp::connection_hdl) {
    // Create a new SSL context with TLS v1.2
    auto ctx = std::make_shared<websocketpp::lib::asio::ssl::context>(websocketpp::lib::asio::ssl::context::tlsv12);
    try {
        ctx->set_options(websocketpp::lib::asio::ssl::context::default_workarounds |
                         websocketpp::lib::asio::ssl::context::no_sslv2 |
                         websocketpp::lib::asio::ssl::context::no_sslv3 |
                         websocketpp::lib::asio::ssl::context::single_dh_use);
    }
    catch (std::exception& e) {
        std::cerr << "Error in TLS initialization: " << e.what() << std::endl;
    }
    return ctx;
}

//
// Function to call Ultravox API and retrieve the join URL.
// Note: Replace "your_api_key" with your actual API key.
//
inline std::string get_join_url(const std::string& api_key) {
    // The Ultravox API endpoint for calls.
    const std::string api_endpoint = "https://api.ultravox.ai/api/calls";

    // Create an HTTP client for HTTPS endpoints.
    httplib::SSLClient cli("api.ultravox.ai");
    // Optionally, disable certificate verification for testing:
    cli.enable_server_certificate_verification(false);

    // Prepare the JSON payload.
    json payload = {
        {"systemPrompt", "You are a helpful assistant..."},
        {"model", "fixie-ai/ultravox"},
        {"voice", "Mark"},
        {"medium", {
            {"serverWebSocket", {
                {"inputSampleRate", 8000},
                {"outputSampleRate", 8000}
            }}
        }}
    };

    // Convert JSON payload to string.
    std::string body = payload.dump();

    // Set up headers.
    httplib::Headers headers = {
        {"X-API-Key", api_key},
     //   {"Content-Type", "application/json"}
    };

    // Send the POST request.
    auto res = cli.Post("/api/calls", headers, body, "application/json");

    if (!res) {
        throw std::runtime_error("No response from Ultravox API.");
    }
    if (res->status != 200 && res->status != 201) {
        throw std::runtime_error("Ultravox API returned status code " + std::to_string(res->status));
    }

    // Parse the JSON response.
    auto response_json = json::parse(res->body);
    if (!response_json.contains("joinUrl")) {
        throw std::runtime_error("Response JSON does not contain joinUrl.");
    }
    std::string joinUrl = response_json["joinUrl"].get<std::string>();
    return joinUrl;
}

//
// WebSocketClient class using websocketpp
//
class WebSocketClient {
public:
 using AudioChunkCallback = std::function<void(const std::vector<int16_t> &)>;
    WebSocketClient() : m_connected(false) {
        // Disable logging (optional).
        m_client.clear_access_channels(websocketpp::log::alevel::all);
        m_client.clear_error_channels(websocketpp::log::elevel::all);

        // Initialize ASIO.
        m_client.init_asio();

        m_client.set_tls_init_handler(on_tls_init);

        // Set handlers.
        m_client.set_open_handler([this](websocketpp::connection_hdl hdl) {
            this->onOpen(hdl);
        });

        m_client.set_message_handler([this](websocketpp::connection_hdl hdl, ws_client::message_ptr msg) {
            this->onMessage(hdl, msg);
        });

        m_client.set_close_handler([this](websocketpp::connection_hdl hdl) {
            this->onClose(hdl);
        });
    }

    ~WebSocketClient() {
        close();
    }

    /// Connect to the given WebSocket URI.
    void connect(const std::string& uri) {
        websocketpp::lib::error_code ec;
        ws_client::connection_ptr con = m_client.get_connection(uri, ec);
        if (ec) {
            std::cerr << "Could not create connection because: " << ec.message() << std::endl;
            return;
        }
        m_hdl = con->get_handle();
        m_client.connect(con);

        // Run the ASIO io_service in a separate thread.
        m_thread = std::thread([this]() {
            m_client.run();
        });

        // Wait until the connection is opened.
        while (!m_connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    /// Send PCM audio data (vector of int16_t samples) as a binary message.
    void sendAudio(const std::vector<int16_t>& audioData) {
        if (!m_connected) {
            std::cerr << "Not connected. Cannot send audio." << std::endl;
            return;
        }

        // Convert the audio vector to a byte stream.
        const char* dataPtr = reinterpret_cast<const char*>(audioData.data());
        size_t dataSize = audioData.size() * sizeof(int16_t);

        websocketpp::lib::error_code ec;
        m_client.send(m_hdl, dataPtr, dataSize, websocketpp::frame::opcode::binary, ec);
        if (ec) {
            std::cerr << "Failed to send audio: " << ec.message() << std::endl;
        }
    }

    /// Close the WebSocket connection.
    void close() {
        if (m_connected) {
            websocketpp::lib::error_code ec;
            m_client.close(m_hdl, websocketpp::close::status::normal, "Normal closure", ec);
            if (ec) {
                std::cerr << "Error initiating close: " << ec.message() << std::endl;
            }
            m_connected = false;
        }

        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
  void set_audio_callback(AudioChunkCallback callback)
    {
        audio_callback = callback;
    }
private:
    // Called when the connection is opened.
    void onOpen(websocketpp::connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(m_connectionMutex);
        m_connected = true;
        std::cout << "Connection opened." << std::endl;
    }

    // Called when a message is received.
    void onMessage(websocketpp::connection_hdl hdl, ws_client::message_ptr msg) {
        if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
            // The payload contains binary audio data.
            std::string payload = msg->get_payload();
            size_t numSamples = payload.size() / sizeof(int16_t);
            std::vector<int16_t> audioSamples(numSamples);

            // Copy the received binary data into the vector.
            std::memcpy(audioSamples.data(), payload.data(), payload.size());
            if (audio_callback) {
                    audio_callback(audioSamples);
                }
            std::cout << "Received binary audio message: " << numSamples << " samples." << std::endl;
            // TODO: Process or play the audioSamples.
        }
        else if (msg->get_opcode() == websocketpp::frame::opcode::text) {
            // Handle text messages (e.g., JSON data messages).
            std::string payload = msg->get_payload();
            std::cout << "Received text message: " << payload << std::endl;
            // TODO: Parse and process JSON messages as required.
        }
    }

    // Called when the connection is closed.
    void onClose(websocketpp::connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(m_connectionMutex);
        m_connected = false;
        std::cout << "Connection closed." << std::endl;
    }

    ws_client m_client;
    websocketpp::connection_hdl m_hdl;
    std::thread m_thread;
    bool m_connected;
    std::mutex m_connectionMutex;
        AudioChunkCallback audio_callback;
};