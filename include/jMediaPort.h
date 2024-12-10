// jMediaPort.h
#pragma once

#include <pjsua2.hpp>
#include <queue>
#include <vector>
#include "jVAD.h"

using namespace pj;

class jMediaPort : public AudioMediaPort {
public:
    jVAD vad;

    explicit jMediaPort();
    void addToQueue(const std::vector<int16_t>& audioData);
    void onFrameRequested(MediaFrame& frame) override;
    void onFrameReceived(MediaFrame& frame) override;
    void clearQueue();

private:
    size_t frameSize = 320;
    std::queue<std::vector<int16_t>> audioQueue;
    std::vector<int16_t> pcmBuffer;
    size_t pcmBufferIndex = 0;
};