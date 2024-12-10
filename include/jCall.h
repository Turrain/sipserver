// jCall.h
#pragma once

#include <pjsua2.hpp>
#include <memory>
#include "jMediaPort.h"
#include "agent.h"
#include "jAccount.h"
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