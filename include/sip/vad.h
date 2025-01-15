#pragma once

#include "deps/webrtcvad.h"
#include <deque>
#include <functional>
#include <mutex>
#include <pjsua2.hpp>
#include <vector>

class VAD {
public:
    using VoiceSegmentCallback = std::function<void(const std::vector<pj::MediaFrame> &)>;
    using SilenceCallback = std::function<void()>;
    using VoiceFrameCallback = std::function<void(const pj::MediaFrame &)>;
    using SpeechStartedCallback = std::function<void()>;

public:
    VAD();
    void processFrame(const pj::MediaFrame &frame);

    void setVoiceSegmentCallback(VoiceSegmentCallback callback);
    void setSilenceCallback(SilenceCallback callback);
    void setVoiceFrameCallback(VoiceFrameCallback callback);
    void setSpeechStartedCallback(SpeechStartedCallback callback);

    static std::vector<int16_t> mergeFrames(const std::vector<pj::MediaFrame> &frames);

private:
    WebRtcVad vad;
    std::mutex bufferMutex;
    std::deque<std::pair<pj::MediaFrame, bool>> vadRingBuffer;
    std::vector<pj::MediaFrame> voiceBuffer;
    bool triggered = false;

    VoiceSegmentCallback onVoiceSegment;
    SilenceCallback onSilence;
    VoiceFrameCallback onVoiceFrame;
    SpeechStartedCallback onSpeechStarted;

    static constexpr size_t MAX_BUFFER_SIZE = 10000;
    static constexpr int PADDING_MS = 800;
    static constexpr int FRAME_DURATION_MS = 20;
    static constexpr float VAD_RATIO = 0.85;

    void processVAD(const pj::MediaFrame &frame, bool is_voiced);
    void processVoicedFrame(const pj::MediaFrame &frame);
    void processSilence();
};
