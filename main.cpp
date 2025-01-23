
#include <memory>
#include <mutex>
#include <string>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "agent/agent.h"
#include "core/configuration.h"
#include "provider/lua_provider.h"
#include "server/server.h"

void runTestMode(core::Configuration &config)
{
    // Initialize components
    auto providerManager = ProviderManager::getInstance();
    providerManager->initialize(config);

    // Create agent manager with scoped configuration
    auto agentConfig = config.create_scoped_config("agents");
    auto agentManager = std::make_shared<AgentManager>(agentConfig);

    // Create test agent
    auto agent = agentManager->create_agent("test-agent");

    if (!agent) {
        LOG_ERROR << "Failed to create test agent";
        return;
    }

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
            std::string a =  agent->process_message(input);
            LOG_DEBUG << a;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "Response time: " << duration.count() << "ms\n";
        } catch (const std::exception &e) {
            LOG_ERROR << "Error during think: " << e.what();
        }
    }
}

int main(int argc, char *argv[])
{

    json defaults = R"({
        "version": 1,
        "default_provider": "openai",
        "server": {
            "host": "localhost",
            "port": 18080
        },
        "services": {
            "whisper": {
                "url": "ws://localhost:9090"
            },
            "auralis": {
                "url": "ws://localhost:9091"
            }
        },
        "providers": {
            "openai": {
                "enabled": true,
                "script_path": "lua/openai.lua",
                "config": {
                    "model": "gpt-4"
                }
            }
        }
    })"_json;

    auto config = std::make_shared<core::Configuration>("config.json", defaults, false);
    config->parse_command_line(argc, argv);

    config->add_observer("providers.*", [](const auto &cfg) {
        std::cout << "Provider configuration changed!\n";
        std::cout << cfg.dump(4) << "\n";
    });
    // config.set("providers.groq.enabled", true);
    auto test = config->get<int>("test");
    LOG_CRITICAL << "Test mode: " << test;


    // config.atomic_save();
    //  Create AgentManager

    try {
        if (test) {
             runTestMode(*config);
        } else {
            Server server(config);
            server.run();
        }

    } catch (const std::exception &e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
