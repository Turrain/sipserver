#pragma once

#include <functional>
#include <vector>
#include <deque>
#include <mutex>
#include <pjsua2.hpp>
#include "webrtcvad.h"

using namespace pj;

class jVAD {
public:
    using VoiceSegmentCallback = std::function<void(const std::vector<MediaFrame>&)>;
    using SilenceCallback = std::function<void()>;
    using VoiceFrameCallback = std::function<void(const MediaFrame&)>;
    using SpeechStartedCallback = std::function<void()>;

public:
    jVAD();
    void processFrame(const MediaFrame& frame);
    
    void setVoiceSegmentCallback(VoiceSegmentCallback callback);
    void setSilenceCallback(SilenceCallback callback);
    void setVoiceFrameCallback(VoiceFrameCallback callback);
    void setSpeechStartedCallback(SpeechStartedCallback callback);

    static std::vector<int16_t> mergeFrames(const std::vector<MediaFrame>& frames);

private:
    WebRtcVad vad;
    std::mutex bufferMutex;
    std::deque<std::pair<MediaFrame, bool>> vadRingBuffer;
    std::vector<MediaFrame> voiceBuffer;
    bool triggered = false;

    VoiceSegmentCallback onVoiceSegment;
    SilenceCallback onSilence;
    VoiceFrameCallback onVoiceFrame;
    SpeechStartedCallback onSpeechStarted;

    static constexpr size_t MAX_BUFFER_SIZE = 10000;
    static constexpr int PADDING_MS = 800;
    static constexpr int FRAME_DURATION_MS = 20;
    static constexpr float VAD_RATIO = 0.85;

    void processVAD(const MediaFrame& frame, bool is_voiced);
    void processVoicedFrame(const MediaFrame& frame);
    void processSilence();
};