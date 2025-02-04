#include "server/server.h"
#include "agent/agent.h"
#include "sip/manager.h"
#include <deps/json.hpp>
#include <httplib.h>
#include <memory>

using json = nlohmann::json;

Server::Server()
{
    ProviderManager::getInstance().load_providers_from_folder("./lua");
    m_manager = std::make_shared<Manager>();
    setupRoutes();
}

Server::~Server()
{
    m_server.stop();
}

void Server::run()
{
    const auto host = config.get<std::string>("SERVER_HOST");
    const auto port = config.get<int>("SERVER_PORT");
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

            std::string agentId = data.value("agentId", "");

            auto result = m_manager->addAccount(
                data["accountId"],
                data["domain"],
                data["username"],
                data["password"],
                data["registrarUri"],
                agentId);

            if (result.success) {
                res.status = 201;
                res.set_content(json {
                                    { "accountId", data["accountId"] },
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
            m_manager->removeAccount(accountId);
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
    m_server.Get("/agents", [](const httplib::Request &req, httplib::Response &res) {
        auto &manager = AgentManager::getInstance();
        auto agents = manager.get_agents();

        nlohmann::json json_agents;
        for (const auto &agent: agents) {
            json_agents.push_back({ { "config", agent->get_config() } });
        }

        res.set_content(json_agents.dump(2), "application/json");
    });

    // GET /agents/:id/think - Process agent's thinking
    m_server.Post("/agents/(.*)/think", [](const httplib::Request &req, httplib::Response &res) {
        std::string id = req.matches[1];
        try{
        auto data = json::parse(req.body);
        std::string text = data["text"];
        std::string result = AgentManager::getInstance().get_agent(id)->process_message(text);
        res.status = 200;
        res.set_content(result, "text/plain");
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(json { { "error", e.what() } }.dump(), "application/json");
        }
    });

    // GET /agents/:id - Get agent by ID
    m_server.Get("/agents/(.*)", [](const httplib::Request &req, httplib::Response &res) {
        std::string id = req.matches[1];
        auto agent = AgentManager::getInstance().get_agent(id);

        if (!agent) {
            res.status = 404;
            return;
        }

        nlohmann::json response = {
            { "id", id },
            { "config", agent->get_config() }
        };
        res.set_content(response.dump(), "application/json");
    });

    // Create new agent
    m_server.Post("/agents", [](const httplib::Request &req, httplib::Response &res) {
        try {
            auto json_body = nlohmann::json::parse(req.body);
            std::string id = json_body["id"];
            auto config = json_body["config"];

            AgentManager::getInstance().add_agent(id, config);
            res.status = 201;
        } catch (const std::exception &e) {
            res.status = 400;
            res.set_content(e.what(), "text/plain");
        }
    });

    // Update agent config
    m_server.Put("/agents/(.*)", [](const httplib::Request &req, httplib::Response &res) {
        std::string id = req.matches[1];
        try {
            auto json_body = nlohmann::json::parse(req.body);
            AgentManager::getInstance().update_agent_config(id, json_body);
            res.status = 204;
        } catch (const std::exception &e) {
            res.status = 400;
            res.set_content(e.what(), "text/plain");
        }
    });

    // Delete agent
    m_server.Delete("/agents/(.*)", [](const httplib::Request &req, httplib::Response &res) {
        std::string id = req.matches[1];
        if (!AgentManager::getInstance().get_agent(id)) {
            res.status = 404;
            return;
        }

        AgentManager::getInstance().remove_agent(id);
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
