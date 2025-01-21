// jAccount.cpp
#include "sip/account.h"
#include "agent/agent.h"
#include "sip/call.h"
#include "utils/logger.h"

Account::Account(std::shared_ptr<AgentManager> agentManager) :
    m_agentManager(agentManager)
{
}

void Account::setAgent(const std::string &agentId)
{
    m_agentId = agentId;
    m_agent = m_agentManager->getAgent(agentId);
}

std::shared_ptr<Agent> Account::getAgent() const
{
    return m_agent;
}

void Account::registerRegStateCallback(onRegStateCallback cb)
{
    regStateCallback = std::move(cb);
}

void Account::onRegState(pj::OnRegStateParam &prm)
{
    pj::AccountInfo ai = getInfo();
    if (regStateCallback) {
        regStateCallback(ai.regIsActive, ai.regStatus);
    }
    LOG_DEBUG << "Registration status: " << ai.regStatus;
    LOG_DEBUG << "Registration active: " << ai.regIsActive;
}

void Account::onIncomingCall(pj::OnIncomingCallParam &iprm)
{
    auto *call = new Call(*this, iprm.callId);
    pj::CallInfo ci = call->getInfo();
    LOG_DEBUG << "Incoming call from " << ci.remoteUri;
    pj::CallOpParam prm;
    prm.statusCode = PJSIP_SC_OK;
    call->direction = Call::INCOMING;
    call->answer(prm);
}

Account::~Account() { }
