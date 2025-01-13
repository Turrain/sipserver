#include "server/server.h"
#include <memory>

Server::Server()
{
    m_agentManager = std::make_shared<AgentManager>();
    m_manager = std::make_shared<Manager>(m_agentManager);
    setupRoutes();
}

Server::~Server() { }

void Server::run()
{
    m_server.listen("127.0.0.1", 18080);
}

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

            m_manager->addAccount(
                accountId,
                data["domain"],
                data["username"],
                data["password"],
                data["registrarUri"],
                agentId);

            res.status = 201;
            res.set_content(json { { "accountId", accountId } }.dump(), "application/json");
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
    m_server.Get("/agents", [this](const httplib::Request &req, httplib::Response &res) {
        json response = json::array();
        auto agents = m_agentManager->getAgents();

        for (const auto &agent: agents) {
            response.push_back({ { "id", agent->id },
                { "config", agent->config } });
        }

        res.set_content(response.dump(), "application/json");
    });

    // POST /agents - Create new agent
    m_server.Post("/agents", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            auto body = json::parse(req.body);

            if (!body.contains("id") || !body.contains("type") || !body.contains("config")) {
                res.status = 400;
                res.set_content("{\"error\":\"Missing required fields: id, type, config\"}", "application/json");
                return;
            }

            auto agent = m_agentManager->createAgent(
                body["id"].get<std::string>(),
                body["type"].get<std::string>(),
                body["config"]);

            if (!agent) {
                res.status = 409;
                res.set_content("{\"error\":\"Agent already exists\"}", "application/json");
                return;
            }

            json response = {
                { "id", agent->id },
                { "config", agent->config }
            };

            res.status = 201;
            res.set_content(response.dump(), "application/json");
        } catch (const json::exception &e) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // GET /agents/:id - Get specific agent
    m_server.Get("/agents/:id", [this](const httplib::Request &req, httplib::Response &res) {
        auto id = req.path_params.at("id");
        auto agent = m_agentManager->getAgent(id);

        if (!agent) {
            res.status = 404;
            res.set_content("{\"error\":\"Agent not found\"}", "application/json");
            return;
        }

        json response = {
            { "id", agent->id },
            { "config", agent->config }
        };

        res.set_content(response.dump(), "application/json");
    });

    // PUT /agents/:id - Update agent configuration
    m_server.Put("/agents/:id", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            auto id = req.path_params.at("id");
            auto body = json::parse(req.body);

            if (!body.contains("config")) {
                res.status = 400;
                res.set_content("{\"error\":\"Missing config field\"}", "application/json");
                return;
            }

            if (!m_agentManager->updateAgentConfig(id, body["config"])) {
                res.status = 404;
                res.set_content("{\"error\":\"Agent not found\"}", "application/json");
                return;
            }

            auto agent = m_agentManager->getAgent(id);
            json response = {
                { "id", agent->id },
                { "config", agent->config }
            };

            res.set_content(response.dump(), "application/json");
        } catch (const json::exception &e) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // DELETE /agents/:id - Remove an agent
    m_server.Delete("/agents/:id", [this](const httplib::Request &req, httplib::Response &res) {
        auto id = req.path_params.at("id");

        if (!m_agentManager->removeAgent(id)) {
            res.status = 404;
            res.set_content("{\"error\":\"Agent not found\"}", "application/json");
            return;
        }

        res.status = 204;
    });

#pragma endregion
}
