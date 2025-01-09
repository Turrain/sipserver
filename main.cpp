
#include <map>
#include <mutex>
#include <string>
#include <thread>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "deps/httplib.h"
#include "sip/manager.h"
#include "provider/request_factory.h"
#include "provider/ollama_provider.h"
#include "provider/groq_provider.h"
#include "deps/json.hpp"
#include "agent/agent.h"


// TODO: Update a docker files to use cache, or optimize build
// TODO: Make a tests for the project
// TODO: Rework architecture of project
// TODO: Update a LLM Providers
// TODO: Rework Logger
// TODO: Implement a start speech callback, rework VAD | 50%
// TODO: Rework TTS,STT, make a abstractions, fabrics.
// TODO: Implement  CI/CD pipeline
// TODO: Implement a WebUI
// TODO: Implement a second backend server like endpoint for SIP server

class PJSIPController {
private:
    Manager &m_sipManager;
    httplib::Server m_server;

public:
    PJSIPController(Manager &sipManager) :
        m_sipManager(sipManager)
    {
        setupRoutes();
    }

    void run(uint16_t port = 18080)
    {
        std::cout << "Starting PJSIP REST API on port " << port << "..."
                  << std::endl;
        m_server.listen("127.0.0.1", port);
    }

private:
    void setupRoutes()
    {
        // Accounts Add
        m_server.Post("/accounts/add", [this](const httplib::Request &req, httplib::Response &res) {
            // Parse JSON
            nlohmann::json x;
            try {
                x = nlohmann::json::parse(req.body);
            } catch (...) {
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
                return;
            }

            if (!x.contains("accountId") || !x.contains("domain") || !x.contains("username") || !x.contains("password") || !x.contains("registrarUri")) {
                res.status = 400;
                res.set_content("Missing required fields", "text/plain");
                return;
            }

            std::string accountId = x["accountId"].get<std::string>();
            std::string domain = x["domain"].get<std::string>();
            std::string username = x["username"].get<std::string>();
            std::string password = x["password"].get<std::string>();
            std::string registrar = x["registrarUri"].get<std::string>();
            std::string agentId = x["agentId"].get<std::string>();

            // Initiate the action on Manager
            m_sipManager.addAccount(accountId, domain, username, password, registrar,agentId);

            // Block and wait for the manager to complete and set a status
            // code/response Assuming the manager sets some flag or triggers
            // completion For demo, we just loop until we hypothetically have a status
            // set
            while (res.status == 0) {
                std::this_thread::yield();
            }
        });

        // Make Call
        m_server.Post("/calls/make",
            [this](const httplib::Request &req, httplib::Response &res) {
                nlohmann::json x;
                try {
                    x = nlohmann::json::parse(req.body);
                } catch (...) {
                    res.status = 400;
                    res.set_content("Invalid JSON", "text/plain");
                    return;
                }

                if (!x.contains("accountId") || !x.contains("destUri")) {
                    res.status = 400;
                    res.set_content("Missing required fields", "text/plain");
                    return;
                }

                std::string accountId = x["accountId"].get<std::string>();
                std::string destUri = x["destUri"].get<std::string>();

                m_sipManager.makeCall(accountId, destUri);

                // Block until done
                while (res.status == 0) {
                    std::this_thread::yield();
                }
            });

        // Hangup Call
        m_server.Post("/calls/hangup",
            [this](const httplib::Request &req, httplib::Response &res) {
                nlohmann::json x;
                try {
                    x = nlohmann::json::parse(req.body);
                } catch (...) {
                    res.status = 400;
                    res.set_content("Invalid JSON", "text/plain");
                    return;
                }

                if (!x.contains("callId")) {
                    res.status = 400;
                    res.set_content("Missing callId field", "text/plain");
                    return;
                }

                int callId = x["callId"].get<int>();
                m_sipManager.hangupCall(callId);

                // Block until done
                while (res.status == 0) {
                    std::this_thread::yield();
                }
            });

        // Remove Account
        m_server.Delete("/accounts/remove", [this](const httplib::Request &req, httplib::Response &res) {
            nlohmann::json x;
            try {
                x = nlohmann::json::parse(req.body);
            } catch (...) {
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
                return;
            }

            if (!x.contains("accountId")) {
                res.status = 400;
                res.set_content("Missing accountId field", "text/plain");
                return;
            }

            std::string accountId = x["accountId"].get<std::string>();
            m_sipManager.removeAccount(accountId);

            // Block until done
            while (res.status == 0) {
                std::this_thread::yield();
            }
        });
    }
};

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
    AgentManager* agentManager = AgentManager::getInstance();
    json agent1Config;
    agent1Config["provider"] = "Ollama";
    agent1Config["Ollama"]["model"] = "llama3.2:3b"; // Specify the Ollama model
    agent1Config["Ollama"]["stream"] = false; // Example of provider-specific config

    auto agent1 = agentManager->createAgent("agent1", "BaseAgent", agent1Config);

    // Agent 2: Using Groq
    json agent2Config;
    agent2Config["provider"] = "Groq";
    agent2Config["Groq"]["model"] = "gemma2-9b-it";
    agent2Config["Groq"]["temperature"] = 0.8; // Example of provider-specific config

    auto agent2 = agentManager->createAgent("agent2", "BaseAgent", agent2Config);
    std::string command;
    std::cout << "Enter a command (or 'exit'): ";
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

            auto agent = agentManager->getAgent(agentId);
            if (agent) {
                agent->think(message);
            } else {
                std::cout << "Agent not found.\n";
            }
        } else if (command.rfind("config ", 0) == 0) {
            std::istringstream iss(command);
            std::string cmd, agentId;
            iss >> cmd >> agentId;

            auto agent = agentManager->getAgent(agentId);
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
                if (agentManager->updateAgentConfig(agentId, newConfig)) {
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
        Manager sipManager;
        PJSIPController apiController(sipManager);
        apiController.run();
    } catch (const std::exception &e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
