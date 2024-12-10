// jAccount.h
#pragma once

#include <pjsua2.hpp>
#include <functional>
#include <string>
#include <memory>

class Agent;

class jAccount : public pj::Account {
public:
    using onRegStateCallback = std::function<void(bool, pj_status_t)>;

    void setAgent(const std::string& agentId);
    std::shared_ptr<Agent> getAgent() const;
    void registerRegStateCallback(onRegStateCallback cb);
    void onRegState(pj::OnRegStateParam& prm) override;
    void onIncomingCall(pj::OnIncomingCallParam& iprm) override;

    ~jAccount() override;

private:
    onRegStateCallback regStateCallback = nullptr;
    std::string m_agentId;
    std::shared_ptr<Agent> m_agent;
};