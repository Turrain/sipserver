//
// Created by Asus on 25.11.2024.
//

#ifndef WEBRTCVAD_H
#define WEBRTCVAD_H
#include "webrtc/common_audio/vad/include/webrtc_vad.h"
#include <stdexcept>
#
class WebRtcVad {
public:
  WebRtcVad() {
    handle_ = WebRtcVad_Create();
    if (!handle_) {
      throw std::runtime_error("Failed to create VAD instance");
    }
    if (WebRtcVad_Init(handle_) != 0) {
      WebRtcVad_Free(handle_);
      throw std::runtime_error("Failed to initialize VAD instance");
    }
  }

  ~WebRtcVad() {
    if (handle_) {
      WebRtcVad_Free(handle_);
      handle_ = nullptr;
    }
  }

  void setMode(int mode) {
    if (mode < 0 || mode > 3) {
      throw std::invalid_argument("Mode must be between 0 and 3");
    }
    if (WebRtcVad_set_mode(handle_, mode) != 0) {
      throw std::runtime_error("Failed to set VAD mode");
    }
  }

  bool validRateAndFrameLength(int rate, int frame_length) {
    return WebRtcVad_ValidRateAndFrameLength(rate, frame_length) == 0;
  }

  bool process(int fs, const int16_t* audio_frame, size_t frame_length) {
    int result = WebRtcVad_Process(handle_, fs, audio_frame, frame_length);
    if (result == -1) {
      throw std::runtime_error("Error processing audio frame");
    }
    return result == 1;
  }

private:
  VadInst* handle_;
};
#endif //WEBRTCVAD_H
