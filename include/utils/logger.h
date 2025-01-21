#pragma once 
#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <string>
#include <ctime>

enum class Level {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class Logger {
public:
    static void setMinLevel(Level level) {
        std::lock_guard<std::mutex> lock(logMutex);
        minLevel = level;
    }

    static Level getMinLevel() {
        std::lock_guard<std::mutex> lock(logMutex);
        return minLevel;
    }

    static void output(Level level, const char* file, int line, const std::string& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        #ifdef _WIN32
            localtime_s(&tm, &in_time_t);
        #else
            localtime_r(&in_time_t, &tm);
        #endif
        
        std::stringstream timeStream;
        timeStream << std::put_time(&tm, "%T");
        
        std::cerr << getColorCode(level)
                  << "[" << timeStream.str() << "] "
                  << "[" << levelToString(level) << "] "
                  << "[" << file << ":" << line << "] "
                  << message << "\033[0m" << std::endl;
    }

private:
  inline static std::mutex logMutex;
    inline static Level minLevel = Level::Info;

    static const char* getColorCode(Level level) {
        switch (level) {
            case Level::Trace:    return "\033[37m";  // White
            case Level::Debug:    return "\033[36m";  // Cyan
            case Level::Info:     return "\033[32m";  // Green
            case Level::Warning:  return "\033[33m";  // Yellow
            case Level::Error:    return "\033[31m";  // Red
            case Level::Critical: return "\033[31;1m";// Bold Red
            default:              return "\033[0m";   // Reset
        }
    }

    static const char* levelToString(Level level) {
        switch (level) {
            case Level::Trace:    return "TRACE";
            case Level::Debug:    return "DEBUG";
            case Level::Info:     return "INFO";
            case Level::Warning:  return "WARNING";
            case Level::Error:    return "ERROR";
            case Level::Critical: return "CRITICAL";
            default:              return "UNKNOWN";
        }
    }
};


class LogStream {
public:
    LogStream(Level level, const char* file, int line) 
        : level(level), file(file), line(line) {
        if (level < Logger::getMinLevel()) {
            enabled = false;
            return;
        }
        enabled = true;
    }

    ~LogStream() {
        if (enabled) {
            Logger::output(level, file, line, stream.str());
        }
    }

    template<typename T>
    LogStream& operator<<(T&& value) {
        if (enabled) {
            stream << std::forward<T>(value);
        }
        return *this;
    }

private:
    bool enabled;
    Level level;
    const char* file;
    int line;
    std::ostringstream stream;
};

#define LOG_TRACE   LogStream(Level::Trace,   __FILE__, __LINE__)
#define LOG_DEBUG   LogStream(Level::Debug,   __FILE__, __LINE__)
#define LOG_INFO    LogStream(Level::Info,    __FILE__, __LINE__)
#define LOG_WARNING LogStream(Level::Warning, __FILE__, __LINE__)
#define LOG_ERROR   LogStream(Level::Error,   __FILE__, __LINE__)
#define LOG_CRITICAL LogStream(Level::Critical, __FILE__, __LINE__)