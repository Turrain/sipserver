// jCall.cpp
#include "sip/call.h"

#include "utils/logger.h"
#include "agent/agent.h"

void call::onCallState(pj::OnCallStateParam& prm)
{
    pj::CallInfo ci = getInfo();
    LOG_DEBUG("onCallState: Call %d state: %d", ci.id, ci.state);
}

void call::onCallMediaState(pj::OnCallMediaStateParam& prm)
{
    
    auto agent = getAgent();
    pj::CallInfo ci = getInfo();
    LOG_DEBUG("onCallMediaState: Call %d media state: %d", ci.id, ci.state);

    for (int i = 0; i < ci.media.size(); i++) {
        if (ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE && ci.media[i].type == PJMEDIA_TYPE_AUDIO && getMedia(i)) {
            auto* aud_med = dynamic_cast<pj::AudioMedia*>(getMedia(i));
            auto& aud_dev_manager = pj::Endpoint::instance().audDevManager();

            auto portInfo = aud_med->getPortInfo();
            auto format = portInfo.format;
            if (direction == call::INCOMING) {
                LOG_DEBUG("AUDIO INCOMING");
                agent->think("Привет, я твой ассистент.");
                
             //   agent->sendText("Привет, я твой ассистент.");
            }
            aud_med->startTransmit(mediaPort);
            mediaPort.startTransmit(*aud_med);
        }
    }
}

std::shared_ptr<Agent> call::getAgent() const
{
    return m_account.getAgent();
}

call::call(jAccount& acc, int call_id) :
    pj::Call(acc, call_id),
    m_account(acc)
{
    direction = OUTGOING;
    mediaPort.vad.setVoiceSegmentCallback(
        [this](const std::vector<pj::MediaFrame>& frames) {
            LOG_DEBUG("Voice segment");
         //   agent->sendAudio(jVAD::mergeFrames(frames));
        });

    mediaPort.vad.setSpeechStartedCallback(
        [this]() {
            LOG_DEBUG("Speech started");
            mediaPort.clearQueue();
        });

    // agent->setAudioChunkCallback(
    //     [this](const std::vector<int16_t>& audio_data) {
    //         mediaPort.addToQueue(audio_data);
    //     });

    if (mediaPort.getPortId() == PJSUA_INVALID_ID) {
        auto mediaFormatAudio = pj::MediaFormatAudio();
        mediaFormatAudio.type = PJMEDIA_TYPE_AUDIO;
        mediaFormatAudio.frameTimeUsec = 20000;
        mediaFormatAudio.channelCount = 1;
        mediaFormatAudio.clockRate = 8000;
        mediaFormatAudio.bitsPerSample = 16;
        mediaFormatAudio.avgBps = 128000;
        mediaFormatAudio.maxBps = 128000;
        mediaPort.createPort("default", mediaFormatAudio);
    }
}