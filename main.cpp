#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "agent/agent.h"
#include "core/configuration.h"
#include "provider/lua_provider.h"
#include "provider/provider_manager.h"
#include "server/server.h"
#include "utils/logger.h"

// Function implementations
static void runTestMode(core::Configuration config)
{
    // Initialize components
    auto providerManager = ProviderManager::getInstance();
    auto pdv = core::ScopedConfiguration(config, "/providers");
    LOG_CRITICAL << pdv.getData().dump(4);
    providerManager->initialize(pdv);
    auto cfg = core::ScopedConfiguration(config, "/agents");
   // providerManager->processRequest("groq", "test");
    // Create agent manager with shared configuration pointer
    auto agentManager = std::make_shared<AgentManager>(cfg);

    // Create test agent
    auto agent = agentManager->create_agent("test-agent");
    auto sa = agentManager->get_agents()[0];
    LOG_DEBUG << sa->config().getData().dump(4);
    if (!agent) {
        LOG_ERROR << "Failed to create test agent";
        return;
    }
     auto agent2 = agentManager->create_agent("test-agent2");
    if (!agent2) {
        LOG_ERROR << "Failed to create test agent";
        return;
    }
    agent->configure("/provider", "groq");
  //  agent->configure("provider", "groq");

    // Interactive test loop
    std::string input;
    std::cout << "\nEnter messages to test agent thinking (or 'exit' to quit):\n";
    while (true) {
        std::cout << "\nYou: ";
        std::getline(std::cin, input);

        if (input == "exit") {
            break;
        }

        try {
            auto start = std::chrono::high_resolution_clock::now();
            std::string a = agent2->process_message(input);
            LOG_DEBUG << a;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "Response time: " << duration.count() << "ms\n";
            config.saveToFile("config_temp.json");
        } catch (const std::exception &e) {
            LOG_ERROR << "Error during think: " << e.what();
        }
    }
}

int main(int argc, char *argv[])
{
    using json = nlohmann::json;

    core::Configuration core;
    core.loadFromFile("config.json");
    auto parser = core::CLIParser({ { "test", "t", core::CLIArgument::Type::Boolean, "Enable test" }});
  
    auto parsed = parser.parse(argc, argv);
    core.set("/cli",parsed);
    
    LOG_CRITICAL << core.getData().dump(4);

    try {
        auto test_mode = core.get<bool>("/cli/test");
        if (test_mode) {
            LOG_CRITICAL << "Test mode enabled";
            runTestMode(core);
        } else {
            Server server(core);
            server.run();
        }
    } catch (const std::exception &e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
