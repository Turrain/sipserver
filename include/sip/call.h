// call.h
#pragma once

#include <pjsua2.hpp>
#include <memory>
#include "sip/jMediaPort.h"
#include "agent/agent.h"
#include "sip/jAccount.h"
class jCall : public pj::Call {
public:
    void onCallState(pj::OnCallStateParam& prm) override;
    void onCallMediaState(pj::OnCallMediaStateParam& prm) override;

    std::shared_ptr<Agent> getAgent() const;

    jCall(jAccount& acc, int call_id = PJSUA_INVALID_ID);

    enum Direction {
        INCOMING,
        OUTGOING,
    } direction;

private:
    jAccount m_account;
    jMediaPort mediaPort;
};