// account.h
#pragma once

#include "agent/agent.h"
#include <pjsua2.hpp>
#include <functional>
#include <string>
#include <memory>

class Agent;

class Account : public pj::Account {
public:
    using onRegStateCallback = std::function<void(bool, pj_status_t)>;
    Account(std::shared_ptr<AgentManager> agentManager);
    void setAgent(const std::string& agentId);
    std::shared_ptr<Agent> getAgent() const;
    void registerRegStateCallback(onRegStateCallback cb);
    void onRegState(pj::OnRegStateParam& prm) override;
    void onIncomingCall(pj::OnIncomingCallParam& iprm) override;

    ~Account() override;

private:
    onRegStateCallback regStateCallback = nullptr;
    std::string m_agentId;
    std::shared_ptr<Agent> m_agent;
    std::shared_ptr<AgentManager> m_agentManager;
};