// jCall.cpp
#include "sip/call.h"

#include "agent/agent.h"
#include "utils/logger.h"

void Call::onCallState(pj::OnCallStateParam &prm)
{
    pj::CallInfo ci = getInfo();
    LOG_DEBUG("onCallState: Call %d state: %d", ci.id, ci.state);
}

void Call::onCallMediaState(pj::OnCallMediaStateParam &prm)
{

    auto agent = getAgent();
    pj::CallInfo ci = getInfo();
    LOG_DEBUG("onCallMediaState: Call %d media state: %d", ci.id, ci.state);

    for (int i = 0; i < ci.media.size(); i++) {
        if (ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE && ci.media[i].type == PJMEDIA_TYPE_AUDIO && getMedia(i)) {
            auto *aud_med = dynamic_cast<pj::AudioMedia *>(getMedia(i));
            auto &aud_dev_manager = pj::Endpoint::instance().audDevManager();

            auto portInfo = aud_med->getPortInfo();
            auto format = portInfo.format;
            if (direction == Call::INCOMING) {
                LOG_DEBUG("AUDIO INCOMING");
                agent->speak("Привет, я твой ассистент.");

                //   agent->sendText("Привет, я твой ассистент.");
            }
            aud_med->startTransmit(mediaPort);
            mediaPort.startTransmit(*aud_med);
        }
    }
}

std::shared_ptr<Agent> Call::getAgent() const
{
    return m_account.getAgent();
}

Call::Call(Account &acc, int call_id) :
    pj::Call(acc, call_id),
    m_account(acc)
{
    direction = OUTGOING;

    this->getAgent()->setSpeechCallback(
        [this](const std::vector<int16_t> &audio_data) {
            mediaPort.addToQueue(audio_data);
        });
    
    mediaPort.vad.setVoiceSegmentCallback(
        [this](const std::vector<pj::MediaFrame> &frames) {
            LOG_DEBUG("Voice segment");
            this->getAgent()->listen(VAD::mergeFrames(frames));
        });

    mediaPort.vad.setSpeechStartedCallback(
        [this]() {
            LOG_DEBUG("Speech started");
            mediaPort.clearQueue();
        });

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
