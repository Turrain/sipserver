// jAccount.cpp
#include "sip/account.h"
#include "sip/call.h"
#include "utils/logger.h"
#include "agent/agent.h"

void jAccount::setAgent(const std::string& agentId)
{
    m_agentId = agentId;
    m_agent = AgentManager::getInstance()->getAgent(agentId);
}

std::shared_ptr<Agent> jAccount::getAgent() const
{
    return m_agent;
}

void jAccount::registerRegStateCallback(onRegStateCallback cb)
{
    regStateCallback = std::move(cb);
}

void jAccount::onRegState(pj::OnRegStateParam& prm)
{
    pj::AccountInfo ai = getInfo();
    LOG_DEBUG("Registration state: %d", ai.regIsActive);
    LOG_DEBUG("Registration status: %d", ai.regStatus);
}

void jAccount::onIncomingCall(pj::OnIncomingCallParam& iprm)
{
    auto* call = new jCall(*this, iprm.callId);
    pj::CallInfo ci = call->getInfo();
    LOG_DEBUG("Incoming call from %s", ci.remoteUri.c_str());
    pj::CallOpParam prm;
    prm.statusCode = PJSIP_SC_OK;
    call->direction = jCall::INCOMING;
    call->answer(prm);
}

jAccount::~jAccount() {}