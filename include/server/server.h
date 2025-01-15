
#include "agent/agent.h"
#include "deps/httplib.h"
#include "sip/manager.h"
#include <memory>

class Server {
public:
    Server();
    ~Server();
    void run();
    void setupRoutes();

private:
    std::shared_ptr<Manager> m_manager;
    std::shared_ptr<AgentManager> m_agentManager;
    httplib::Server m_server;
};
