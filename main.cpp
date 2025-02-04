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
    Logger::setMinLevel(Level::Debug);
    AppConfig &config = AppConfig::getInstance();
    config.add_options({ { "test", "t", CLIParser::Type::Boolean, "test", "false" } });
    config.initialize(argc, argv);
    GlobalDatabase::instance().configureAutoPersist("db_backup.bson");
    GlobalDatabase::instance().configurePersistStrategy(true);
    GlobalDatabase::instance().initialize("config.json");
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
