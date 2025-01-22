
#include <memory>
#include <mutex>
#include <string>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "agent/agent.h"
#include "core/configuration.h"
#include "provider/lua_provider.h"
#include "server/server.h"
// TODO: Update a docker files to use cache, or optimize build [o]
// TODO: Make a tests for the project [x]
// TODO: Implement  CI/CD pipeline [x]
// TODO: Implement a WebUI (Stepan) [o]

// TODO: Implement a SIP logic: Call info, Call transfer
// TODO: Global management system: REWORK

void benchmark_call(LuaProviderManager &manager, const std::string &provider, const std::string &prompt, int num_calls = 1)
{
    std::cout << "Benchmarking " << provider << " (" << num_calls << " calls):\n";

    for (int i = 0; i < num_calls; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto response = manager.call_provider(provider, prompt);
        auto end = std::chrono::high_resolution_clock::now();

        std::cout << "Call " << i + 1 << ":\n";
        std::cout << "Response: " << response.content << "\n";
        std::cout << "Time taken: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << "ms\n\n";
    }
}

int main(int argc, char *argv[])
{

    json defaults = R"({
        "version": 1,
        "default_provider": "openai",
        "providers": {
            "openai": {
                "enabled": true,
                "script_path": "lua/openai.lua",
                "config_overrides": {
                    "model": "gpt-4"
                }
            }
        }
    })"_json;
    
    core::Configuration config("config.json", defaults, false);
    config.parse_command_line(argc, argv);
    config.add_observer("providers.*", [](const auto &cfg) {
        std::cout << "Provider configuration changed!\n";
        std::cout << cfg.dump(4) << "\n";
    });
    config.bulk_set({ { "providers.openai.config_overrides.temperature", 0.7 },
        { "providers.groq.enabled", true } });
    {
        core::Configuration::Transaction txn(config);
        txn.data()["version"] = 2;
        txn.data()["providers"]["openai"]["enabled"] = false;
        txn.commit();
    }
    LOG_CRITICAL << "Configuration: " << config.get_json().dump(4);
    //config.atomic_save();
    // Create AgentManager
    AgentManager agentManager;

    // Initialize provider system with JSON configuration
    ProviderManager::getInstance()->initialize(config);

    // Create agents using Lua providers
    json agent1Config;
    agent1Config["provider"] = "ollama";
    agent1Config["model"] = "llama3.2:1b";
    agent1Config["stream"] = false;
    auto agent1 = agentManager.createAgent("agent1", "BaseAgent", agent1Config);

    json agent2Config;
    agent2Config["provider"] = "groq";
    agent2Config["model"] = "mixtral-8x7b-32768";
    agent2Config["temperature"] = 0.8;
    auto agent2 = agentManager.createAgent("agent2", "BaseAgent", agent2Config);

    std::string command;
    std::cout << "Enter a command (or 'exit'): ";

    std::cout << "Default provider: " << config.get<std::string>("default_provider") << "\n";

    while (std::getline(std::cin, command) && command != "exit") {
        if (command == "help") {
            std::cout << "Available commands:\n";
            std::cout << "  think <agent_id> <message> - Make the agent think\n";
            std::cout << "  config <agent_id> - View agent's config\n";
            std::cout << "  update <agent_id> <json_config> - Update agent's config\n";
            std::cout << "  exit - Exit the program\n";
        } else if (command.rfind("think ", 0) == 0) {
            std::istringstream iss(command);
            std::string cmd, agentId, message;
            iss >> cmd >> agentId;
            std::getline(iss >> std::ws, message);
            auto agent = agentManager.getAgent(agentId);
            if (agent) {
                auto start = std::chrono::high_resolution_clock::now();
                agent->think(message);
                auto end = std::chrono::high_resolution_clock::now();
                std::cout << "Time taken: "
                          << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                          << "ms\n\n";

            } else {
                std::cout << "Agent not found.\n";
            }
        } else if (command.rfind("config ", 0) == 0) {
            std::istringstream iss(command);
            std::string cmd, agentId;
            iss >> cmd >> agentId;
            auto agent = agentManager.getAgent(agentId);
            if (agent) {
                std::cout << agent->config.dump(4) << std::endl;
            } else {
                std::cout << "Agent not found.\n";
            }
        } else if (command.rfind("update ", 0) == 0) {
            std::istringstream iss(command);
            std::string cmd, agentId, jsonConfigStr;
            iss >> cmd >> agentId;
            std::getline(iss >> std::ws, jsonConfigStr);
            try {
                json newConfig = json::parse(jsonConfigStr);
                if (agentManager.updateAgentConfig(agentId, newConfig)) {
                    std::cout << "Agent config updated.\n";
                } else {
                    std::cout << "Agent not found.\n";
                }
            } catch (const json::parse_error &e) {
                std::cout << "Invalid JSON format: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Unknown command. Type 'help' for a list of commands.\n";
        }
        std::cout << "\nEnter a command (or 'exit'): ";
    }
    try {
        Server server;
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
