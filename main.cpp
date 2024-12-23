#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "httplib.h"
#include "jmanager.h"
#include "json.hpp"


//TODO: Update a docker files to use cache, or optimize build
//TODO: Make a tests for the project
//TODO: Rework architecture of project
//TODO: Update a LLM Providers
//TODO: Rework Logger
//TODO: Implement a start speech callback, rework VAD
//TODO: Rework TTS,STT, make a abstractions, fabrics.
//TODO: Implement  CI/CD pipeline
//TODO: Implement a WebUI
//TODO: Implement a second backend server like endpoint for SIP server



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

int main()
{
    registerLLMClients();
 
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
