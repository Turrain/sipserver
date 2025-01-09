// call.h
#pragma once

#include <pjsua2.hpp>
#include <memory>
#include "sip/media_port.h"
#include "agent/agent.h"
#include "sip/account.h"
class Call : public pj::Call {
public:
    void onCallState(pj::OnCallStateParam& prm) override;
    void onCallMediaState(pj::OnCallMediaStateParam& prm) override;

    std::shared_ptr<Agent> getAgent() const;

    Call(Account& acc, int call_id = PJSUA_INVALID_ID);

    enum Direction {
        INCOMING,
        OUTGOING,
    } direction;

private:
    Account m_account;
    MediaPort mediaPort;
};