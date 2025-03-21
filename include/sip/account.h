// account.h
#pragma once

#include "agent/agent.h"
#include <functional>
#include <memory>
#include <pjsua2.hpp>
#include <string>

class Agent;

class Account: public pj::Account {
public:
    using onRegStateCallback = std::function<void(bool, pj_status_t)>;
    Account();
    void setAgent(const std::string &agentId);
    std::shared_ptr<Agent> getAgent() const;
    void registerRegStateCallback(onRegStateCallback cb);
    void onRegState(pj::OnRegStateParam &prm) override;
    void onIncomingCall(pj::OnIncomingCallParam &iprm) override;

    ~Account() override;

private:
    onRegStateCallback regStateCallback = nullptr;
    std::string m_agentId;
    std::shared_ptr<Agent> m_agent;
    AgentManager& m_agentManager = AgentManager::getInstance();
};