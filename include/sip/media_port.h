// media_port.h
#pragma once

#include "sip/vad.h"
#include <pjsua2.hpp>
#include <queue>
#include <vector>

class MediaPort: public pj::AudioMediaPort {
public:
    VAD vad;

    explicit MediaPort();
    void addToQueue(const std::vector<int16_t> &audioData);
    void onFrameRequested(pj::MediaFrame &frame) override;
    void onFrameReceived(pj::MediaFrame &frame) override;
    void clearQueue();

private:
    size_t frameSize = 320;
    std::queue<std::vector<int16_t>> audioQueue;
    std::vector<int16_t> pcmBuffer;
    size_t pcmBufferIndex = 0;
};
