#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "httplib.h"
#include "jmanager.h"
#include "json.hpp"
#include "llm_provider.h"

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

            // Initiate the action on Manager
            m_sipManager.addAccount(accountId, domain, username, password, registrar);

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
ProviderManager *ProviderManager::instance = nullptr;
std::mutex ProviderManager::mutex;
std::mutex Logger::logMutex;
int main()
{
    registerLLMClients();
    ProviderManager *providerManager = ProviderManager::getInstance();
    providerManager->registerProviderFactory("Ollama", std::make_unique<OllamaProviderFactory>());
    providerManager->registerProviderFactory("Groq", std::make_unique<GroqProviderFactory>());

    // Load configuration from file
    std::ifstream f("config.json");
    json configData = json::parse(f);

    // Load provider configurations
    providerManager->loadConfig(configData); // Pass json object to loadConfig

    // Create AgentManager and register agent factory
    auto agentFactory = std::make_unique<ConcreteAgentFactory>();
    AgentManager agentManager(std::move(agentFactory));

    // Load agent configurations
    agentManager.loadConfig("config.json");

    json newGroqConfig;
    newGroqConfig["model"] = "gemma2-9b-it"; // Example: change to a different model
    agentManager.changeProvider("agent1", "Groq", newGroqConfig);
    agentManager.saveConfig("config.json");

    // Get agent1 and test
    Agent *agent1 = agentManager.getAgent("agent1");
    if (agent1) {
        agent1->think("What is the capital of France?");
    }

    // Change provider for agent1 back to Ollama and update model
    json newOllamaConfig;
    newOllamaConfig["model"] = "llama3.2:3b"; // Example: change to a different model
    agentManager.changeProvider("agent1", "Ollama", newOllamaConfig);
    agentManager.saveConfig("config.json");

    // Get agent1 and test again
    agent1 = agentManager.getAgent("agent1");
    if (agent1) {
        agent1->think("What is the capital of Germany?");
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
