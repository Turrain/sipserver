// jVAD.cpp
#include "sip/vad.h"
#include <algorithm>
VAD::VAD()
{
    vad.setMode(2);
    vadRingBuffer.resize(PADDING_MS / FRAME_DURATION_MS);
    voiceBuffer.reserve(MAX_BUFFER_SIZE);
}

void VAD::processFrame(const pj::MediaFrame &frame)
{
    if (frame.size == 0) {
        return;
    }
    std::lock_guard lock(bufferMutex);
    const auto *int_data = reinterpret_cast<const int16_t *>(frame.buf.data());
    const bool is_voiced = vad.process(8000, int_data, 160);
    processVAD(frame, is_voiced);
}

void VAD::setVoiceSegmentCallback(VoiceSegmentCallback callback)
{
    onVoiceSegment = std::move(callback);
}

void VAD::setSilenceCallback(SilenceCallback callback)
{
    onSilence = std::move(callback);
}

void VAD::setVoiceFrameCallback(VoiceFrameCallback callback)
{
    onVoiceFrame = std::move(callback);
}

void VAD::setSpeechStartedCallback(SpeechStartedCallback callback)
{
    onSpeechStarted = std::move(callback);
}

std::vector<int16_t> VAD::mergeFrames(const std::vector<pj::MediaFrame> &frames)
{
    std::vector<int16_t> result;
    result.reserve(frames.size() * 320);

    for (const auto &frame: frames) {
        const int16_t *samples = reinterpret_cast<const int16_t *>(frame.buf.data());
        result.insert(result.end(), samples, samples + frame.size / 2);
    }

    return result;
}

void VAD::processVAD(const pj::MediaFrame &frame, bool is_voiced)
{
    if (!triggered) {
        vadRingBuffer.emplace_back(frame, is_voiced);
        if (vadRingBuffer.size() > PADDING_MS / FRAME_DURATION_MS) {
            vadRingBuffer.pop_front();
        }

        int num_voiced = std::count_if(vadRingBuffer.begin(), vadRingBuffer.end(),
            [](const auto &pair) { return pair.second; });

        if (num_voiced > VAD_RATIO * vadRingBuffer.size()) {
            triggered = true;
            voiceBuffer.clear();
            if (onSpeechStarted) {
                onSpeechStarted();
            }
            for (const auto &[f, s]: vadRingBuffer) {
                processVoicedFrame(f);
            }
            vadRingBuffer.clear();
        }
    } else {
        processVoicedFrame(frame);
        vadRingBuffer.emplace_back(frame, is_voiced);
        if (vadRingBuffer.size() > PADDING_MS / FRAME_DURATION_MS) {
            vadRingBuffer.pop_front();
        }

        int num_unvoiced = std::count_if(vadRingBuffer.begin(), vadRingBuffer.end(),
            [](const auto &pair) { return !pair.second; });

        if (num_unvoiced > VAD_RATIO * vadRingBuffer.size()) {
            if (onVoiceSegment && !voiceBuffer.empty()) {
                onVoiceSegment(voiceBuffer);
            }
            triggered = false;
            processSilence();
            voiceBuffer.clear();
            vadRingBuffer.clear();
        }
    }
}

void VAD::processVoicedFrame(const pj::MediaFrame &frame)
{
    if (voiceBuffer.size() < MAX_BUFFER_SIZE) {
        voiceBuffer.push_back(frame);
    }
    if (onVoiceFrame) {
        onVoiceFrame(frame);
    }
}

void VAD::processSilence()
{
    if (onSilence) {
        onSilence();
    }
}
