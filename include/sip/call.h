// call.h
#pragma once

#include "agent/agent.h"
#include "sip/account.h"
#include "sip/media_port.h"
#include <memory>
#include <pjsua2.hpp>
class Call: public pj::Call {
public:
    void onCallState(pj::OnCallStateParam &prm) override;
    void onCallMediaState(pj::OnCallMediaStateParam &prm) override;

    std::shared_ptr<Agent> getAgent() const;

    Call(Account &acc, int call_id = PJSUA_INVALID_ID);

    enum Direction {
        INCOMING,
        OUTGOING,
    } direction;

private:
    UltravoxAgent uagent;
    Account m_account;
    MediaPort mediaPort;
};