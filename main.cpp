#include "json.hpp"
#include "jmanager.h"
#include <crow.h>
#include <map>
#include <mutex>
#include "whisper_client.h"
class PJSIPController {
private:
  Manager &m_sipManager;
  crow::SimpleApp m_app;

public:
  PJSIPController(Manager &sipManager) : m_sipManager(sipManager) {
    m_app.loglevel(crow::LogLevel::Warning);
    setupRoutes();
  }

  void run(uint16_t port = 18080) { m_app.port(port).multithreaded().run(); }

private:
  void setupRoutes() {
    // Account Management
    CROW_ROUTE(m_app, "/accounts/add")
        .methods("POST"_method)([this](const crow::request &req) {
          auto x = crow::json::load(req.body);
          if (!x) {
            return crow::response(400, "Invalid JSON");
          }

          std::string accountId = x["accountId"].s();
          std::string domain = x["domain"].s();
          std::string username = x["username"].s();
          std::string password = x["password"].s();
          std::string registrar = x["registrarUri"].s();

          crow::response res;
          m_sipManager.addAccount(accountId, domain, username, password,
                                  registrar);

          // Block and wait for callback
          while (res.code == 0) {
            std::this_thread::yield();
          }
          return res;
        });

    // Make Call Route
    CROW_ROUTE(m_app, "/calls/make")
        .methods("POST"_method)([this](const crow::request &req) {
          auto x = crow::json::load(req.body);
          if (!x) {
            return crow::response(400, "Invalid JSON");
          }

          std::string accountId = x["accountId"].s();
          std::string destUri = x["destUri"].s();

          crow::response res;
          m_sipManager.makeCall(accountId, destUri

          );

          // Block and wait for callback
          while (res.code == 0) {
            std::this_thread::yield();
          }
          return res;
        });

    // Hangup Call Route
    CROW_ROUTE(m_app, "/calls/hangup")
        .methods("POST"_method)([this](const crow::request &req) {
          auto x = crow::json::load(req.body);
          if (!x) {
            return crow::response(400, "Invalid JSON");
          }

          int callId = x["callId"].i();

          crow::response res;
          m_sipManager.hangupCall(callId

          );

          // Block and wait for callback
          while (res.code == 0) {
            std::this_thread::yield();
          }
          return res;
        });

    // Account Removal Route
    CROW_ROUTE(m_app, "/accounts/remove")
        .methods("DELETE"_method)([this](const crow::request &req) {
          auto x = crow::json::load(req.body);
          if (!x) {
            return crow::response(400, "Invalid JSON");
          }

          std::string accountId = x["accountId"].s();

          crow::response res;
          m_sipManager.removeAccount(accountId

          );

          // Block and wait for callback
          while (res.code == 0) {
            std::this_thread::yield();
          }
          return res;
        });
  }
};

int main() {
  try {
    WhisperClient client;
    client.connect("http://localhost:8080");
    Manager sipManager;
    PJSIPController apiController(sipManager);

    std::cout << "Starting PJSIP REST API on port 18080..." << std::endl;
    apiController.run();
  } catch (const std::exception &e) {
    std::cerr << "Initialization Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}