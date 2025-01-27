#include "server/server.h"
#include <memory>
#include <deps/json.hpp>
#include <httplib.h>
#include "agent/agent.h"
#include "sip/manager.h"

using json = nlohmann::json;

Server::Server(core::ScopedConfiguration  config)
    : m_config(config)
{
    // Create scoped configuration for server component

    auto providerManager = ProviderManager::getInstance();
    Logger::setMinLevel(Level::Debug);
  
    // config->add_observer("*", [](const auto& cfg) {
    //     LOG_CRITICAL << "Config changed: " << cfg.dump(4);
    // });
     providerManager->initialize(config);
    // Initialize managers
    m_agentManager = std::make_shared<AgentManager>(m_config);
    m_manager = std::make_shared<Manager>(m_agentManager);

    setupRoutes();
}

Server::~Server() {
    m_server.stop();
}

void Server::run()
{

    const auto host = m_config.get<std::string>("server.host");
    const auto port = m_config.get<int>("server.port");
    
    LOG_INFO << "Starting server on " << host << ":" << port;
    
    if (!m_server.listen(host, port)) {
        LOG_ERROR << "Server failed to start on " << host << ":" << port;
    }
}

static uint64_t event_id = 0;
void Server::setupRoutes()
{
    //-----------------------------------------------
    // ACCOUNT
    //-----------------------------------------------
#pragma region Account

    // POST /accounts - Create new account
    m_server.Post("/accounts", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            auto data = json::parse(req.body);

            // Validate required fields
            if (!data.contains("domain") || !data.contains("username") || !data.contains("password") || !data.contains("registrarUri")) {
                res.status = 400;
                res.set_content(json { { "error", "Missing required fields" } }.dump(), "application/json");
                return;
            }

            std::string accountId = data["username"].get<std::string>() + "@" + data["domain"].get<std::string>();
            std::string agentId = data.value("agentId", "");

            auto result = m_manager->addAccount(
                accountId,
                data["domain"],
                data["username"],
                data["password"],
                data["registrarUri"],
                agentId);

            if (result.success) {
                res.status = 201;
                res.set_content(json {
                                    { "accountId", accountId },
                                    { "status", "registered" },
                                    { "message", result.message } }
                                    .dump(),
                    "application/json");
            } else {
                res.status = result.statusCode;
                res.set_content(json {
                                    { "error", result.message } }
                                    .dump(),
                    "application/json");
            }
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(json { { "error", e.what() } }.dump(), "application/json");
        }
    });

    // PUT /accounts/:id - Update account
    m_server.Put(R"(/accounts/([^/]+))", [this](const httplib::Request &req, httplib::Response &res) {
        std::string accountId = req.matches[1];
        try {
            auto data = json::parse(req.body);

            // Remove existing account
            m_manager->removeAccount(accountId);

            // Add updated account
            m_manager->addAccount(
                accountId,
                data["domain"],
                data["username"],
                data["password"],
                data["registrarUri"],
                data.value("agentId", ""));

            res.set_content(json { { "accountId", accountId } }.dump(), "application/json");
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(json { { "error", e.what() } }.dump(), "application/json");
        }
    });

    // DELETE /accounts/:id - Remove account
    m_server.Delete(R"(/accounts/([^/]+))", [this](const httplib::Request &req, httplib::Response &res) {
        std::string accountId = req.matches[1];
        try {
            m_manager->removeAccount(accountId);
            res.status = 204;
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(json { { "error", e.what() } }.dump(), "application/json");
        }
    });

#pragma endregion

//-----------------------------------------------
// CALL
//-----------------------------------------------
#pragma region Call

    m_server.Post("/calls/make", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            auto data = json::parse(req.body);

            // Validate required fields
            if (!data.contains("accountId") || !data.contains("destUri")) {
                res.status = 400;
                res.set_content(json {
                                    { "error", "Missing required fields: accountId and destUri" } }
                                    .dump(),
                    "application/json");
                return;
            }

            std::string accountId = data["accountId"];
            std::string destUri = data["destUri"];

            m_manager->makeCall(accountId, destUri);

            res.status = 202; // Accepted
            res.set_content(json {
                                { "status", "Call initiated" },
                                { "accountId", accountId },
                                { "destUri", destUri } }
                                .dump(),
                "application/json");

        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(json { { "error", e.what() } }.dump(), "application/json");
        }
    });

    m_server.Post("/calls/hangup", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            auto data = json::parse(req.body);

            // Validate required fields
            if (!data.contains("callId")) {
                res.status = 400;
                res.set_content(json {
                                    { "error", "Missing required field: callId" } }
                                    .dump(),
                    "application/json");
                return;
            }

            int callId = data["callId"].get<int>();
            m_manager->hangupCall(callId);

            res.status = 200;
            res.set_content(json {
                                { "status", "Call terminated" },
                                { "callId", callId } }
                                .dump(),
                "application/json");

        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(json { { "error", e.what() } }.dump(), "application/json");
        }
    });

#pragma endregion

    //-----------------------------------------------
    // AGENT
    //-----------------------------------------------
#pragma region Agent

// GET /agents - List all agents
m_server.Get("/agents", [this](const httplib::Request& req, httplib::Response& res) {
    auto agents = m_agentManager->get_agents();
    json response = json::array();

    {
        //core::Configuration::Transaction tx(*m_agentManager->getConfig());
        for (const auto& agent : agents) {
            response.push_back({
           //     {"id", agent->id()},
           //     {"config", tx.data()} // Thread-safe snapshot
            });
        }
    }

    res.set_content(response.dump(), "application/json");
});

// POST /agents - Create new agent with initial config
m_server.Post("/agents", [this](const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = json::parse(req.body);

        if (!body.contains("id") || !body["id"].is_string()) {
            res.status = 400;
            res.set_content(json{{"error", "Missing/invalid 'id' field"}}.dump(), 
                          "application/json");
            return;
        }

        const auto& id = body["id"].get<std::string>();
        const auto type = body.value("type", "BaseAgent");
        const auto& config_patch = body.value("config", json::object());

        // Check if agent already exists
        if (m_agentManager->get_agent(id)) {
            res.status = 409;
            res.set_content(json{{"error", "Agent already exists"}}.dump(),
                          "application/json");
            return;
        }
        
        auto agent = m_agentManager->create_agent(id);
        if (agent) {
            res.status = 201;
            res.set_content(json{
                {"id", id},
         //       {"config", agent->config()->get_json()}
            }.dump(), "application/json");
        } else {
            res.status = 500;
            res.set_content(json{{"error", "Agent creation failed"}}.dump(), 
                          "application/json");
        }
    } catch (const json::exception& e) {
        res.status = 400;
        res.set_content(json{{"error", "Invalid JSON"}}.dump(), 
                      "application/json");
    }
});

// POST /agents/:id/think - Make agent think
m_server.Post(R"(/agents/([^/]+)/think)", [this](const httplib::Request& req, httplib::Response& res) {
    try {
        std::string id = req.matches[1];
        auto body = json::parse(req.body);

        if (!body.contains("input")) {
            res.status = 400;
            res.set_content(json{{"error", "Missing 'input' field"}}.dump(),
                          "application/json");
            return;
        }

        auto agent = m_agentManager->get_agent(id);
        if (!agent) {
            res.status = 404;
            res.set_content(json{{"error", "Agent not found"}}.dump(),
                          "application/json");
            return;
        }

        agent->think(body["input"].get<std::string>());
        res.status = 200;
        res.set_content(json{{"response", "Processed successfully"}}.dump(),
                      "application/json");
    } catch (const json::exception& e) {
        res.status = 400;
        res.set_content(json{{"error", "Invalid JSON"}}.dump(),
                      "application/json");
    }
});

// PATCH /agents/:id - Update agent configuration
m_server.Patch(R"(/agents/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
    try {
        std::string id = req.matches[1];
        auto patch = json::parse(req.body);

        // Validate JSON structure
        if (!patch.is_object()) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON format"}}.dump(), 
                          "application/json");
            return;
        }

        // Check if agent exists
        auto agent = m_agentManager->get_agent(id);
        if (!agent) {
            res.status = 404;
            res.set_content(json{{"error", "Agent not found"}}.dump(), 
                          "application/json");
            return;
        }

        // Validate configuration values
        if (patch.contains("stm_capacity") && patch["stm_capacity"].get<int>() < 0) {
            res.status = 422;
            res.set_content(json{{"error", "Invalid stm_capacity value"}}.dump(),
                          "application/json");
            return;
        }
  //      agent->configure(patch);


        res.status = 200;
        res.set_content(json{
            {"id", id},
          //  {"config", agent->config()->get_json()}
        }.dump(), "application/json");

    } catch (const json::exception& e) {
        res.status = 400;
        res.set_content(json{{"error", "Invalid JSON"}}.dump(), 
                      "application/json");
    }
});

// GET /agents/:id - Get agent config state
m_server.Get("/agents/:id", [this](const httplib::Request& req, httplib::Response& res) {
    auto id = req.path_params.at("id");
    
    if (auto agent = m_agentManager->get_agent(id)) {
 //       res.set_content(agent->config()->get_json().dump(), "application/json");
    } else {
        res.status = 404;
        res.set_content(json{{"error", "Agent not found"}}.dump(), 
                      "application/json");
    }
});

// DELETE /agents/:id - Remove agent
m_server.Delete(R"(/agents/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
    std::string id = req.matches[1];
    
    // Remove agent
    m_agentManager->remove_agent(id);
    
    // Always return 204 as per REST convention
    res.status = 204;
});

#pragma endregion

#pragma region Status

    m_server.Get("/status", [this](const httplib::Request &req, httplib::Response &res) {
        json response = {
            { "status", "OK" }
        };

        res.set_content(response.dump(), "application/json");
    });

    m_server.Get("/events", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_chunked_content_provider("text/event-stream", [this](size_t offset, httplib::DataSink &sink) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            auto text = "data: {\"id\": " + std::to_string(event_id++) + "}\n\n";
            sink.write(text.c_str(), text.size());
            return true;
        });
    });

#pragma endregion
}
