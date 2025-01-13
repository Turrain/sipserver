#include "server/server.h"

Server::Server(){
    setupRoutes();
}

Server::~Server(){}

void Server::run()
{
    m_server.listen("127.0.0.1", 18080);
}

void Server::setupRoutes(){
//-----------------------------------------------
// ACCOUNT
//-----------------------------------------------
    #pragma region Account

    m_server.Get("/accounts", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    m_server.Post("/accounts/add", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    m_server.Get("/accounts/:id", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    m_server.Put("/accounts/:id", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    m_server.Delete("/accounts/:id", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    #pragma endregion
//-----------------------------------------------
// CALL
//-----------------------------------------------
    #pragma region Call

    m_server.Post("/calls/make", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    m_server.Post("/calls/hangup", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    #pragma endregion
//-----------------------------------------------
// AGENT
//-----------------------------------------------
    #pragma region Agent

    m_server.Get("/agents", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    m_server.Post("/agents/add", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    m_server.Get("/agents/:id", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    m_server.Put("/agents/:id", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    m_server.Delete("/agents/:id", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Not Implemented", "application/json");
    });

    #pragma endregion
}
