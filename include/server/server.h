#pragma once
#include "agent/agent.h"
#include "core/configuration.h"
#include "deps/httplib.h"
#include "sip/manager.h"
#include <memory>
#include <string>

class Server {
public:
    Server();
    ~Server();
    
    void run();
    void setupRoutes();

private:
    AppConfig &config = AppConfig::getInstance();
    std::shared_ptr<Manager> m_manager;
    std::shared_ptr<AgentManager> m_agentManager;
    httplib::Server m_server;
};
