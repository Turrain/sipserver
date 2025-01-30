// jMediaPort.cpp
#include "sip/media_port.h"

MediaPort::MediaPort() :
    AudioMediaPort() { }

void MediaPort::addToQueue(const std::vector<int16_t> &audioData)
{
    audioQueue.emplace(audioData);
}

void MediaPort::onFrameRequested(pj::MediaFrame &frame)
{
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;

    const size_t requiredSamples = frameSize / sizeof(int16_t);
    std::vector<int16_t> tempBuffer(requiredSamples, 0); 

    size_t samplesCopied = 0;

    while (samplesCopied < requiredSamples) {
        if (pcmBufferIndex >= pcmBuffer.size()) {
            if (audioQueue.empty()) {
                break;
            }
            pcmBuffer = audioQueue.front();
            audioQueue.pop();
            pcmBufferIndex = 0;
        }

        size_t remainingSamples = requiredSamples - samplesCopied;
        size_t availableSamples = pcmBuffer.size() - pcmBufferIndex;
        size_t samplesToCopy = std::min(remainingSamples, availableSamples);


        std::copy(pcmBuffer.begin() + pcmBufferIndex,
                  pcmBuffer.begin() + pcmBufferIndex + samplesToCopy,
                  tempBuffer.begin() + samplesCopied);


        samplesCopied += samplesToCopy;
        pcmBufferIndex += samplesToCopy;
    }

    frame.buf.assign(
        reinterpret_cast<const uint8_t*>(tempBuffer.data()),
        reinterpret_cast<const uint8_t*>(tempBuffer.data() + requiredSamples));
    frame.size = static_cast<unsigned>(requiredSamples * sizeof(int16_t));
}

void MediaPort::onFrameReceived(pj::MediaFrame &frame)
{
    vad.processFrame(frame);
}

void MediaPort::clearQueue()
{
    audioQueue = std::queue<std::vector<int16_t>>(); // Empty the audio queue
    pcmBuffer.clear();               // Clear current PCM buffer
    pcmBufferIndex = 0;              // Reset buffer index
}
