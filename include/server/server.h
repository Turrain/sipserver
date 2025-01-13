
#include "agent/agent.h"
#include "sip/manager.h"
#include "deps/httplib.h"
#include "deps/json.hpp"

class Server {
  public:
    Server();
    ~Server();
    void run();
    void setupRoutes();

  private:
    Manager m_sipManager;
    httplib::Server m_server;
    AgentManager* m_agentManager;
};
