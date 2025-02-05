#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "agent/agent.h"
#include "core/configuration.h"
#include "db/GlobalDatabase.h"
#include "provider/provider_manager.h"
#include "server/server.h"
#include "utils/logger.h"
// Function implementations
#include "stream/ultravox_client.h"
static void runTestMode()
{
    // Initialize components
    ProviderManager::getInstance().load_providers_from_folder("./lua");
    auto result = ProviderManager::getInstance().process_request(
        "ollama",
        "Explain the difference between RAG and fine-tuning",
        {
            { "temperature", 0.5 },
            { "model", "llama3.2:1b" },
        },
        { { { "role", "system" }, { "content", "system_prompt" } } });

    LOG_DEBUG << result.response;
}

int main(int argc, char **argv)
{

// {
//     try {
//         // Replace with your actual API key.
//         std::string api_key = "EeyPncss.N5YiUZ7uPUaHJa21XyRHPudF5ZQnOVsq";

//         std::cout << "Requesting join URL from Ultravox API..." << std::endl;
//         std::string joinUrl = get_join_url(api_key);
//         std::cout << "Received join URL: " << joinUrl << std::endl;

//         // Create the WebSocket client and connect to the join URL.
//         WebSocketClient wsClient;
//         wsClient.connect(joinUrl);

//         // Simulate sending PCM audio data:
//         // Create a 1-second buffer of silence at 48000Hz.
//         constexpr size_t sampleRate = 48000;
//         std::vector<int16_t> audioData(sampleRate, 0);

//         // Send the audio data.
//         wsClient.sendAudio(audioData);

//         // Wait to receive messages (simulate a running call)
//         std::this_thread::sleep_for(std::chrono::seconds(5));

//         // Close the WebSocket connection.
//         wsClient.close();
//     }
//     catch (const std::exception &ex) {
//         std::cerr << "Error: " << ex.what() << std::endl;
//         return EXIT_FAILURE;
//     }

//     }
    Logger::setMinLevel(Level::Debug);
    AppConfig &config = AppConfig::getInstance();
    config.add_options({ { "test", "t", CLIParser::Type::Boolean, "test", "false" } });
    config.initialize(argc, argv);
    GlobalDatabase::instance().configureAutoPersist("db_backup.bson");
    GlobalDatabase::instance().configurePersistStrategy(true);
    GlobalDatabase::instance().initialize("");
    const bool test_mode = config.get<bool>("test");
    try {

        if (test_mode) {
            LOG_CRITICAL << "Test mode enabled";
            runTestMode();
        } else {
            Server server;
            server.run();
        }
    } catch (const std::exception &e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
