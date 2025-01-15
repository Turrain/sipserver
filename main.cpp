
#include <memory>
#include <mutex>
#include <string>
#define CPPHTTPLIB_OPENSSL_SUPPORT

#include "provider/ollama_provider.h"
#include "provider/groq_provider.h"
#include "agent/agent.h"
#include "server/server.h"

// TODO: Update a docker files to use cache, or optimize build [o]
// TODO: Make a tests for the project [x]
// TODO: Implement  CI/CD pipeline [x]
// TODO: Implement a WebUI (Stepan) [o]

// TODO: Implement a SIP logic: Call info, Call transfer
// TODO: Event stream
//


std::mutex Logger::logMutex;
int main()
{
    ProviderManager *providerManager = ProviderManager::getInstance();
    providerManager->registerProviderFactory("Ollama", std::make_unique<OllamaProviderFactory>());
    providerManager->registerProviderFactory("Groq", std::make_unique<GroqProviderFactory>());

    // Load configuration from file
    std::ifstream f("config.json");
    json configData = json::parse(f);

    // Load provider configurations
    providerManager->loadConfig(configData); // Pass json object to loadConfig

    // Create AgentManager and register agent factory
    // AgentManager agentManager;
    // json agent1Config;
    // agent1Config["provider"] = "Ollama";
    // agent1Config["Ollama"]["model"] = "llama3.2:3b"; // Specify the Ollama model
    // agent1Config["Ollama"]["stream"] = false; // Example of provider-specific config

    // auto agent1 = agentManager.createAgent("agent1", "BaseAgent", agent1Config);

    // // Agent 2: Using Groq
    // json agent2Config;
    // agent2Config["provider"] = "Groq";
    // agent2Config["Groq"]["model"] = "gemma2-9b-it";
    // agent2Config["Groq"]["temperature"] = 0.8; // Example of provider-specific config

    // auto agent2 = agentManager.createAgent("agent2", "BaseAgent", agent2Config);
    // std::string command;
    // std::cout << "Enter a command (or 'exit'): ";
    // while (std::getline(std::cin, command) && command != "exit") {
    //     if (command == "help") {
    //         std::cout << "Available commands:\n";
    //         std::cout << "  think <agent_id> <message> - Make the agent think\n";
    //         std::cout << "  config <agent_id> - View agent's config\n";
    //         std::cout << "  update <agent_id> <json_config> - Update agent's config\n";
    //         std::cout << "  exit - Exit the program\n";
    //     } else if (command.rfind("think ", 0) == 0) {
    //         std::istringstream iss(command);
    //         std::string cmd, agentId, message;
    //         iss >> cmd >> agentId;
    //         std::getline(iss >> std::ws, message);
    //         auto agent = agentManager.getAgent(agentId);
    //         if (agent) {
    //             agent->think(message);
    //         } else {
    //             std::cout << "Agent not found.\n";
    //         }
    //     } else if (command.rfind("config ", 0) == 0) {
    //         std::istringstream iss(command);
    //         std::string cmd, agentId;
    //         iss >> cmd >> agentId;
    //         auto agent = agentManager.getAgent(agentId);
    //         if (agent) {
    //             std::cout << agent->config.dump(4) << std::endl;
    //         } else {
    //             std::cout << "Agent not found.\n";
    //         }
    //     } else if (command.rfind("update ", 0) == 0) {
    //         std::istringstream iss(command);
    //         std::string cmd, agentId, jsonConfigStr;
    //         iss >> cmd >> agentId;
    //         std::getline(iss >> std::ws, jsonConfigStr);
    //         try {
    //             json newConfig = json::parse(jsonConfigStr);
    //             if (agentManager.updateAgentConfig(agentId, newConfig)) {
    //                 std::cout << "Agent config updated.\n";
    //             } else {
    //                 std::cout << "Agent not found.\n";
    //             }
    //         } catch (const json::parse_error &e) {
    //             std::cout << "Invalid JSON format: " << e.what() << std::endl;
    //         }
    //     } else {
    //         std::cout << "Unknown command. Type 'help' for a list of commands.\n";
    //     }
    //     std::cout << "\nEnter a command (or 'exit'): ";
    // }
    try {
        Server server;
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
