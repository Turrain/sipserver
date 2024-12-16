// Description: PJSIP Manager class for handling accounts and calls. Based on my
// Java implementation

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <pjsua2.hpp>
#include <queue>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include "webrtcvad.h"
using namespace pj;
//----------------------------------------------------------------------
// Logger class
//----------------------------------------------------------------------

enum class LogLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };

const std::map<::LogLevel, std::string> LogLevelColors = {
    {::LogLevel::DEBUG, "\033[36m"},   // Cyan
    {::LogLevel::INFO, "\033[32m"},    // Green
    {::LogLevel::WARNING, "\033[33m"}, // Yellow
    {::LogLevel::ERROR, "\033[31m"},   // Red
    {::LogLevel::CRITICAL, "\033[41m"} // Red background
};

const std::string ResetColor = "\033[0m";

class Logger {
public:
  static Logger &getInstance() {
    static Logger instance;
    return instance;
  }

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void setLogLevel(::LogLevel level) { logLevel = level; }

  void enableConsoleLogging(bool enable) { consoleLogging = enable; }

  void enableFileLogging(bool enable, const std::string &filename = "log.txt") {
    std::lock_guard<std::mutex> lock(mtx); // Ensure thread safety
    fileLogging = enable;
    if (fileLogging) {
      if (fileStream.is_open()) {
        fileStream.close();
      }
      fileStream.open(filename, std::ios::out | std::ios::app);
      if (!fileStream) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
        fileLogging = false;
      }
    } else {
      if (fileStream.is_open()) {
        fileStream.close();
      }
    }
  }

  void log(::LogLevel level, const std::string &message) {
    if (level < logLevel) {
      return;
    }

    std::lock_guard<std::mutex> lock(mtx);

    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = logLevelToString(level);
    std::string coloredLevelStr = getColoredLevel(level, levelStr);
    std::string coloredMessage = getColoredMessage(message, level);
    std::ostringstream oss;
    oss << "[" << timestamp << "] [" << coloredLevelStr << "] "
        << coloredMessage << "\n";
    std::string logMessage = oss.str();

    if (consoleLogging) {
      std::cout << logMessage;
    }

    if (fileLogging && fileStream.is_open()) {
      fileStream << "[" << timestamp << "] [" << levelStr << "] " << message
                 << "\n";
    }
  }

  // Existing logging methods accepting std::string
  void debug(const std::string &message) { log(::LogLevel::DEBUG, message); }
  void info(const std::string &message) { log(::LogLevel::INFO, message); }
  void warning(const std::string &message) {
    log(::LogLevel::WARNING, message);
  }
  void error(const std::string &message) { log(::LogLevel::ERROR, message); }
  void critical(const std::string &message) {
    log(::LogLevel::CRITICAL, message);
  }

  void debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::string formattedMessage = formatString(format, args);
    va_end(args);
    log(::LogLevel::DEBUG, formattedMessage);
  }

  void info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::string formattedMessage = formatString(format, args);
    va_end(args);
    log(::LogLevel::INFO, formattedMessage);
  }

  void warning(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::string formattedMessage = formatString(format, args);
    va_end(args);
    log(::LogLevel::WARNING, formattedMessage);
  }

  void error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::string formattedMessage = formatString(format, args);
    va_end(args);
    log(::LogLevel::ERROR, formattedMessage);
  }

  void critical(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::string formattedMessage = formatString(format, args);
    va_end(args);
    log(::LogLevel::CRITICAL, formattedMessage);
  }

private:
  ::LogLevel logLevel;
  bool consoleLogging;
  bool fileLogging;
  std::ofstream fileStream;
  std::mutex mtx;
  Logger()
      : logLevel(::LogLevel::DEBUG), consoleLogging(true), fileLogging(false) {}

  std::string logLevelToString(::LogLevel level) {
    switch (level) {
    case ::LogLevel::DEBUG:
      return "DEBUG";
    case ::LogLevel::INFO:
      return "INFO";
    case ::LogLevel::WARNING:
      return "WARNING";
    case ::LogLevel::ERROR:
      return "ERROR";
    case ::LogLevel::CRITICAL:
      return "CRITICAL";
    default:
      return "UNKNOWN";
    }
  }

  std::string getColoredLevel(::LogLevel level, const std::string &levelStr) {
    auto it = LogLevelColors.find(level);
    if (it != LogLevelColors.end()) {
      return it->second + levelStr + ResetColor;
    }
    return levelStr;
  }

  std::string getColoredMessage(const std::string &message, ::LogLevel level) {
    auto it = LogLevelColors.find(level);
    if (it != LogLevelColors.end()) {
      return it->second + message + ResetColor;
    }
    return message;
  }

  std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()) %
                        1000;

    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &timeT);
#else
    localtime_r(&timeT, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
        << std::setw(3) << milliseconds.count();
    return oss.str();
  }

  std::string formatString(const char *format, va_list args) {
    std::array<char, 1024> buffer;
    va_list argsCopy;
    va_copy(argsCopy, args);

    int length = std::vsnprintf(buffer.data(), buffer.size(), format, argsCopy);
    va_end(argsCopy);

    if (length < 0) {
      return "Formatting error";
    }

    if (static_cast<size_t>(length) < buffer.size()) {
      return std::string(buffer.data(), length);
    } else {
      std::string result(length, '\0');
      std::vsnprintf(&result[0], length + 1, format, args);
      return result;
    }
  }
};

//----------------------------------------------------------------------
// VAD class
//----------------------------------------------------------------------

class jVAD {
public:
  using VoiceSegmentCallback =
      std::function<void(const std::vector<MediaFrame> &)>;
  using SilenceCallback = std::function<void()>;
  using VoiceFrameCallback = std::function<void(const MediaFrame &)>;

public:
  jVAD() {
    vad.setMode(2);
    vadRingBuffer.resize(PADDING_MS / FRAME_DURATION_MS);
    voiceBuffer.reserve(MAX_BUFFER_SIZE);
  }
  void processFrame(const MediaFrame &frame) {
    std::lock_guard lock(bufferMutex);
    const auto *int_data = reinterpret_cast<const int16_t *>(frame.buf.data());
    const bool is_voiced = vad.process(8000, int_data, 160);
    processVAD(frame, is_voiced);
  }

  void setVoiceSegmentCallback(VoiceSegmentCallback callback) {
    onVoiceSegment = std::move(callback);
  }

  void setSilenceCallback(SilenceCallback callback) {
    onSilence = std::move(callback);
  }

  void setVoiceFrameCallback(VoiceFrameCallback callback) {
    onVoiceFrame = std::move(callback);
  }
  std::vector<int16_t> mergeFrames(const std::vector<MediaFrame> &frames) {
    std::vector<int16_t> result;

    result.reserve(frames.size() * 320);

    for (const auto &frame : frames) {
      const int16_t *samples =
          reinterpret_cast<const int16_t *>(frame.buf.data());
      result.insert(result.end(), samples, samples + frame.size / 2);
    }

    return result;
  }

private:
  WebRtcVad vad;
  std::mutex bufferMutex;
  std::deque<std::pair<MediaFrame, bool>> vadRingBuffer;
  std::vector<MediaFrame> voiceBuffer;
  bool triggered = false;

  VoiceSegmentCallback onVoiceSegment;
  SilenceCallback onSilence;
  VoiceFrameCallback onVoiceFrame;

  static constexpr size_t MAX_BUFFER_SIZE = 10000;
  static constexpr int PADDING_MS = 800;
  static constexpr int FRAME_DURATION_MS = 20;
  static constexpr float VAD_RATIO = 0.85;

  void processVAD(const MediaFrame &frame, bool is_voiced) {
    if (!triggered) {
      vadRingBuffer.emplace_back(frame, is_voiced);
      if (vadRingBuffer.size() > PADDING_MS / FRAME_DURATION_MS) {
        vadRingBuffer.pop_front();
      }

      int num_voiced =
          std::count_if(vadRingBuffer.begin(), vadRingBuffer.end(),
                        [](const auto &pair) { return pair.second; });

      if (num_voiced > VAD_RATIO * vadRingBuffer.size()) {
        triggered = true;
        voiceBuffer.clear();

        // Add initial padding frames to voice buffer
        for (const auto &[f, s] : vadRingBuffer) {
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

      // Count unvoiced frames
      int num_unvoiced =
          std::count_if(vadRingBuffer.begin(), vadRingBuffer.end(),
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
  void processVoicedFrame(const MediaFrame &frame) {
    if (voiceBuffer.size() < MAX_BUFFER_SIZE) {
      voiceBuffer.push_back(frame);
    }
    if (onVoiceFrame) {
      onVoiceFrame(frame);
    }
  }

  void processSilence() {
    if (onSilence) {
      onSilence();
    }
  }
};

//----------------------------------------------------------------------
// MediaPort class
//----------------------------------------------------------------------

class jMediaPort : public AudioMediaPort {
public:
  explicit jMediaPort() : AudioMediaPort() {
    vad.setVoiceFrameCallback([this](const MediaFrame &frame) {
      logger.info("Voice frame received, size: {} bytes", frame.size);
    });
    vad.setVoiceSegmentCallback([this](const std::vector<MediaFrame> &frames) {
      logger.info("Complete voice segment detected with {} frames",
                  frames.size());
    });
  }
  void onFrameRequested(MediaFrame &frame) override {
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
  }
  void onFrameReceived(MediaFrame &frame) override { vad.processFrame(frame); }

private:
  ::Logger &logger = ::Logger::getInstance();
  jVAD vad;
};

//----------------------------------------------------------------------
// Call class
//----------------------------------------------------------------------

class jCall : public Call {
public:
  void onCallState(OnCallStateParam &prm) override {
    CallInfo ci = getInfo();
    logger.info("Call {} state: {}", ci.id, ci.stateText);
  }
  void onCallMediaState(OnCallMediaStateParam &prm) override {
    CallInfo ci = getInfo();
    logger.info("Call %d media state: %s", ci.id, ci.stateText);
    for (int i = 0; i < ci.media.size(); i++)
      if (ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE &&
          ci.media[i].type == PJMEDIA_TYPE_AUDIO && getMedia(i)) {
        auto *aud_med = dynamic_cast<AudioMedia *>(getMedia(i));
        auto &aud_dev_manager = Endpoint::instance().audDevManager();

        auto portInfo = aud_med->getPortInfo();
        auto format = portInfo.format;

        logger.info("Port info: %s", portInfo.name);
      }
  }

  explicit jCall(Account &acc, int call_id = PJSUA_INVALID_ID)
      : Call(acc, call_id) {}

private:
  ::Logger &logger = ::Logger::getInstance();
};

//----------------------------------------------------------------------
// Account class
//----------------------------------------------------------------------

class jAccount : public Account {
public:
  using onRegStateCallback = std::function<void(bool, pj_status_t)>;

  void registerRegStateCallback(onRegStateCallback cb) {
    regStateCallback = std::move(cb);
  }
  void onRegState(OnRegStateParam &prm) override {
    AccountInfo ai = getInfo();

    logger.info("Registration state: %d", ai.regIsActive);
    logger.info("Registration status: %d", ai.regStatus);
  }
  void onIncomingCall(OnIncomingCallParam &iprm) override {
    auto *call = new jCall(*this, iprm.callId);
    CallInfo ci = call->getInfo();
    logger.info("Incoming call from %s", ci.remoteUri);

    CallOpParam prm;
    prm.statusCode = PJSIP_SC_OK;
    call->answer(prm);
  }

  ~jAccount() override {}

private:
  onRegStateCallback regStateCallback = nullptr;
  ::Logger &logger = ::Logger::getInstance();
};

//----------------------------------------------------------------------
// Manager class
//----------------------------------------------------------------------

class Manager {
private:
  ::Logger &logger = ::Logger::getInstance();

public:
  Manager() {
    try {
      // Initialize PJSIP endpoint
      m_endpoint.libCreate();

      pj::EpConfig epConfig;
      epConfig.logConfig.level = 2;
      m_endpoint.libInit(epConfig);

      // Create UDP transport
      pj::TransportConfig transportConfig;
      transportConfig.port = 0;
      m_endpoint.transportCreate(PJSIP_TRANSPORT_UDP, transportConfig);

      // Disable audio device
      m_endpoint.audDevManager().setNullDev();

      // Start library
      m_endpoint.libStart();
      logger.info("PJSIP Endpoint initialized successfully");
      // Start worker thread
      m_workerThread =
          std::make_unique<std::thread>(&Manager::workerThreadMain, this);
    } catch (pj::Error &err) {
      std::cerr << "PJSIP Initialization Error: " << err.info() << std::endl;
      throw;
    }
  }
  ~Manager() { shutdown(); }

  void addAccount(const std::string &accountId, const std::string &domain,
                  const std::string &username, const std::string &password,
                  const std::string &registrarUri) {
    enqueueTask([this, accountId, domain, username, password, registrarUri]() {
      try {
        std::lock_guard<std::mutex> lock(m_accountsMutex);

        if (m_accounts.find(accountId) != m_accounts.end()) {
          throw std::invalid_argument("Account already exists: " + accountId);
        }
        pj::AccountConfig accountConfig;
        accountConfig.idUri = "sip:" + username + "@" + domain;
        accountConfig.regConfig.registrarUri = registrarUri;

        pj::AuthCredInfo credInfo("digest", "*", username, 0, password);
        accountConfig.sipConfig.authCreds.push_back(credInfo);

        auto account = std::make_unique<jAccount>();
        account->create(accountConfig);
        account->registerRegStateCallback([](auto state, auto status) {
          std::cout << "Registration state: " << state << " Status: " << status
                    << std::endl;
        });

        m_accounts[accountId] = std::move(account);

      } catch (const pj::Error &err) {

      } catch (const std::exception &e) {
      }
    });
  }

  void removeAccount(const std::string &accountId) {
    enqueueTask([this, accountId]() {
      try {
        std::lock_guard<std::mutex> lock(m_accountsMutex);

        auto it = m_accounts.find(accountId);
        if (it != m_accounts.end()) {
          it->second->shutdown();
          m_accounts.erase(it);

        } else {
        }
      } catch (const pj::Error &err) {
      }
    });
  }

  void makeCall(const std::string &accountId, const std::string &destUri) {
    enqueueTask([this, accountId, destUri]() {
      try {
        std::lock_guard<std::mutex> lock(m_accountsMutex);

        auto accountIt = m_accounts.find(accountId);
        if (accountIt == m_accounts.end()) {
          throw std::invalid_argument("Account not found: " + accountId);
        }

        pj::CallOpParam callOpParam;
        auto call = std::make_unique<jCall>(*accountIt->second);
        call->makeCall(destUri, callOpParam);

        {
          std::lock_guard<std::mutex> callsLock(m_callsMutex);
          m_activeCalls[call->getId()] = std::move(call);
        }
      } catch (const pj::Error &err) {
      }
    });
  }

  void hangupCall(int callId) {
    enqueueTask([this, callId]() {
      try {
        std::lock_guard<std::mutex> lock(m_callsMutex);

        auto it = m_activeCalls.find(callId);
        if (it != m_activeCalls.end()) {
          pj::CallOpParam callOpParam;
          callOpParam.statusCode = PJSIP_SC_DECLINE;
          it->second->hangup(callOpParam);
          m_activeCalls.erase(it);

        } else {
        }
      } catch (const pj::Error &err) {
      }
    });
  }

  void shutdown() {
    m_running = false;
    m_taskQueue.stop();

    if (m_workerThread && m_workerThread->joinable()) {
      m_workerThread->join();
    }
  }

private:
  // Thread-safe task queue
  class TaskQueue {
  public:
    void enqueue(std::function<void()> task) {
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopped)
          return;
        tasks.push(std::move(task));
      }
      condition.notify_one();
    }

    std::function<void()> dequeue() {
      std::unique_lock<std::mutex> lock(mutex);
      condition.wait(lock, [this] { return !tasks.empty() || stopped; });

      if (stopped && tasks.empty()) {
        return nullptr;
      }

      auto task = std::move(tasks.front());
      tasks.pop();
      return task;
    }
    void stop() {
      {
        std::lock_guard<std::mutex> lock(mutex);
        stopped = true;
      }
      condition.notify_all();
    }

  private:
    std::queue<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable condition;
    std::atomic<bool> stopped{false};
  };

  void workerThreadMain() {
    pj_thread_desc threadDesc;
    pj_thread_t *thread = nullptr;

    // Register this thread with PJSIP
    if (pj_thread_register("WorkerThread", threadDesc, &thread) != PJ_SUCCESS) {
      std::cerr << "Failed to register worker thread" << std::endl;
      return;
    }

    while (m_running) {
      try {
        auto task = m_taskQueue.dequeue();
        if (task)
          task();
      } catch (const std::exception &e) {
        std::cerr << "Worker Thread Error: " << e.what() << std::endl;
      }
    }
    shutdownPjsip();
  }
  void shutdownPjsip() {
    // Hangup all active calls
    {
      std::lock_guard<std::mutex> lock(m_callsMutex);
      for (auto &[id, call] : m_activeCalls) {
        try {
          pj::CallOpParam callOpParam;
          callOpParam.statusCode = PJSIP_SC_DECLINE;
          call->hangup(callOpParam);
        } catch (...) {
          std::cerr << "Error hanging up call: " << id << std::endl;
        }
      }
      m_activeCalls.clear();
    }

    // Remove all accounts
    {
      std::lock_guard<std::mutex> lock(m_accountsMutex);
      m_accounts.clear();
    }

    // Destroy PJSIP library
    m_endpoint.libDestroy();
  }
  void enqueueTask(std::function<void()> task) {
    if (m_running) {
      m_taskQueue.enqueue(std::move(task));
    } else {
      throw std::runtime_error("Manager is shutting down");
    }
  }

  pj::Endpoint m_endpoint;
  std::unordered_map<std::string, std::unique_ptr<jAccount>> m_accounts;
  std::unordered_map<int, std::unique_ptr<jCall>> m_activeCalls;

  TaskQueue m_taskQueue;
  std::unique_ptr<std::thread> m_workerThread;
  std::atomic<bool> m_running{true};
  std::mutex m_accountsMutex;
  std::mutex m_callsMutex;
};
