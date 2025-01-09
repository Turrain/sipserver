// jMediaPort.cpp
#include "sip/media_port.h"

media_port::media_port() : AudioMediaPort() {}

void media_port::addToQueue(const std::vector<int16_t>& audioData)
{
    audioQueue.emplace(audioData);
}

void media_port::onFrameRequested(MediaFrame& frame)
{
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;

    if (audioQueue.empty() && pcmBufferIndex >= pcmBuffer.size()) {
        frame.buf.clear();
        frame.size = 0;
        return;
    }

    size_t samplesToCopy = std::min(frameSize / sizeof(int16_t),
        pcmBuffer.size() - pcmBufferIndex);

    std::vector<int16_t> tempBuffer(samplesToCopy);

    if (pcmBufferIndex < pcmBuffer.size()) {
        std::copy(pcmBuffer.begin() + pcmBufferIndex,
            pcmBuffer.begin() + pcmBufferIndex + samplesToCopy,
            tempBuffer.begin());
        pcmBufferIndex += samplesToCopy;
    }

    if (pcmBufferIndex >= pcmBuffer.size() && !audioQueue.empty()) {
        pcmBuffer = audioQueue.front();
        audioQueue.pop();
        pcmBufferIndex = 0;
    }

    frame.buf.assign(
        reinterpret_cast<const uint8_t*>(tempBuffer.data()),
        reinterpret_cast<const uint8_t*>(tempBuffer.data() + tempBuffer.size()));
    frame.size = static_cast<unsigned>(tempBuffer.size() * sizeof(int16_t));
}

void media_port::onFrameReceived(MediaFrame& frame)
{
    vad.processFrame(frame);
}

void media_port::clearQueue()
{
    std::queue<std::vector<int16_t>>().swap(audioQueue);
    pcmBuffer.clear();
    pcmBufferIndex = 0;
}