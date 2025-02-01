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
    m_server.Get("/agents", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content(m_agentManager->config().getData().dump(), "application/json");
    });

    // POST /agents - Create new agent with initial config
    m_server.Post("/agents", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            auto body = json::parse(req.body);

            if (!body.contains("id") || !body["id"].is_string()) {
                res.status = 400;
                res.set_content(json { { "error", "Missing/invalid 'id' field" } }.dump(),
                    "application/json");
                return;
            }

            // id
            const auto &id = body["id"].get<std::string>();
            // provider
            const auto &provider = body.value("provider", "ollama");
            // provider_options
            const auto &config_patch = body.value("provider_options", json::object());
            // Check if agent already exists
            if (m_agentManager->get_agent(id)) {
                res.status = 409;
                res.set_content(json { { "error", "Agent already exists" } }.dump(),
                    "application/json");
                return;
            }

            auto agent = m_agentManager->create_agent(id);

            agent->configure("/provider", provider);
            agent->configure("/provider_options", config_patch);

            if (agent) {
                res.status = 201;
                res.set_content(agent->config().getData().dump(),
                    "application/json");
            } else {
                res.status = 500;
                res.set_content(json { { "error", "Agent creation failed" } }.dump(),
                    "application/json");
            }
        } catch (const json::exception &e) {
            res.status = 400;
            res.set_content(json { { "error", "Invalid JSON" } }.dump(),
                "application/json");
        }
    });

    // POST /agents/:id/think - Make agent think
    m_server.Post("/agents/:id/think", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            auto id = req.path_params.at("id");
            auto body = json::parse(req.body);

            if (!body.contains("input")) {
                res.status = 400;
                res.set_content(json { { "error", "Missing 'input' field" } }.dump(),
                    "application/json");
                return;
            }

            auto agent = m_agentManager->get_agent(id);
            if (!agent) {
                res.status = 404;
                res.set_content(json { { "error", "Agent not found" } }.dump(),
                    "application/json");
                return;
            }
            auto text = agent->process_message(body["input"].get<std::string>());
            LOG_DEBUG << text;
            res.status = 200;
            res.set_content(json { { "text", text } }.dump(),
                "application/json");
        } catch (const json::exception &e) {
            res.status = 400;
            res.set_content(json { { "error", "Invalid JSON" } }.dump(),
                "application/json");
        }
    });

    // PATCH /agents/:id - Update agent configuration
    m_server.Patch(R"(/agents/([^/]+))", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            std::string id = req.matches[1];
            auto patch = json::parse(req.body);

            if (!patch.is_object()) {
                res.status = 400;
                res.set_content(json { { "error", "Invalid JSON format" } }.dump(),
                    "application/json");
                return;
            }

            auto agent = m_agentManager->get_agent(id);
            if (!agent) {
                res.status = 404;
                res.set_content(json { { "error", "Agent not found" } }.dump(),
                    "application/json");
                return;
            }

            // Handle configuration updates
            for (const auto &[path, value]: patch.items()) {
                agent->configure("/" + path, value);
            }
            LOG_DEBUG << agent->config().getData().dump(4);

            res.status = 200;
            res.set_content(json {
                                { "id", id },
                                { "status", "updated" } }
                                .dump(),
                "application/json");

        } catch (const json::exception &e) {
            res.status = 400;
            res.set_content(json { { "error", "Invalid JSON" } }.dump(),
                "application/json");
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(json { { "error", e.what() } }.dump(),
                "application/json");
        }
    });

    // GET /agents/:id - Get agent config state
    m_server.Get("/agents/:id", [this](const httplib::Request &req, httplib::Response &res) {
        auto id = req.path_params.at("id");

        if (auto agent = m_agentManager->get_agent(id)) {
            auto cfg = agent->config().getData().dump();
            res.status = 200;
            res.set_content(cfg, "application/json");
        } else {
            res.status = 404;
            res.set_content(json { { "error", "Agent not found" } }.dump(),
                "application/json");
        }
    });

    // DELETE /agents/:id - Remove agent
    m_server.Delete(R"(/agents/([^/]+))", [this](const httplib::Request &req, httplib::Response &res) {
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
